#include "ClipTransfer/client.hpp"
#include "ClipTransfer/server.hpp"

#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <iostream>
#include <array>
#include <atomic>

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _WIN32
    #include <winsock2.h>
    #include <iphlpapi.h>
    #include <ws2tcpip.h>
#else
    #include <ifaddrs.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <net/if.h>
#endif

#ifdef _WIN32
bool MyConvertLengthToIpv4Mask(ULONG prefixLength, ULONG* mask) {
    if (prefixLength > 32) return false;
    *mask = prefixLength == 0 ? 0 : htonl(0xFFFFFFFF << (32 - prefixLength));
    return true;
}
#endif

std::string Client::getLocalIp() {
    asio::io_context ctx;
    asio::ip::udp::resolver resolver(ctx);
    asio::ip::udp::resolver::query query("8.8.8.8", "80");
    asio::ip::udp::endpoint ep = *resolver.resolve(query).begin();
    return ep.address().to_string();
}

static std::string getBroadcastAddress() {
#ifdef _WIN32
    ULONG outBufLen = 0;
    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &outBufLen);
    if (dwRetVal != ERROR_BUFFER_OVERFLOW) {
        std::cerr << "[ERROR] GetAdaptersAddresses initial call failed: " << dwRetVal << std::endl;
        return "255.255.255.255";
    }

    std::vector<BYTE> buffer(outBufLen);
    IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &outBufLen);
    if (dwRetVal != ERROR_SUCCESS) {
        std::cerr << "[ERROR] GetAdaptersAddresses failed: " << dwRetVal << std::endl;
        return "255.255.255.255";
    }

    for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        // Ignore interfaces marked as VPN
        if (adapter->IfType == IF_TYPE_PPP || strstr(adapter->AdapterName, "NordLynx") || wcsstr(adapter->Description, L"VPN")) {
            continue;
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;

            sockaddr_in* sa = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            ULONG prefixLen = ua->OnLinkPrefixLength;
            ULONG mask = prefixLen == 0 ? 0 : htonl(0xFFFFFFFF << (32 - prefixLen));
            ULONG ip = ntohl(sa->sin_addr.s_addr);
            ULONG msk = ntohl(mask);
            ULONG bcast = (ip & msk) | (~msk);

            in_addr bcast_addr;
            bcast_addr.s_addr = htonl(bcast);
            char* addrStr = inet_ntoa(bcast_addr);
            if (addrStr && std::string(addrStr).find("192.168.") == 0) {
                return std::string(addrStr);
            }
        }
    }

    return "255.255.255.255";
#else
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa, *mask;
    char *addr;
    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
                (ifa->ifa_flags & IFF_BROADCAST) && !(ifa->ifa_flags & IFF_LOOPBACK)) {
                sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
                mask = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask);
                uint32_t ip = ntohl(sa->sin_addr.s_addr);
                uint32_t msk = ntohl(mask->sin_addr.s_addr);
                uint32_t bcast = (ip & msk) | (~msk);
                struct in_addr bcast_addr;
                bcast_addr.s_addr = htonl(bcast);
                addr = inet_ntoa(bcast_addr);
                std::string result(addr);
                freeifaddrs(ifap);
                return result;
            }
        }
        freeifaddrs(ifap);
    }
    return "255.255.255.255";
#endif
}

std::optional<std::string> Client::discoverServerIp(asio::io_context& io) {
    using namespace asio;
    ip::udp::socket socket(io);
    socket.open(ip::udp::v4());
    socket.set_option(socket_base::broadcast(true));

    std::string broadcast_ip = getBroadcastAddress();
    ip::udp::endpoint broadcast_endpoint(ip::make_address(broadcast_ip), 54001);
    std::string message = "DISCOVER_CLIPSERVER";
    socket.send_to(buffer(message), broadcast_endpoint);

    std::array<char, 1024> recvBuf;
    ip::udp::endpoint senderEndpoint;
    socket.non_blocking(true);

    for (int i = 0; i < 10; ++i) { // wait max 1 sec
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        asio::error_code ec;
        size_t len = socket.receive_from(buffer(recvBuf), senderEndpoint, 0, ec);
        if (!ec && std::string(recvBuf.data(), len) == "I_AM_CLIPSERVER") {
            return senderEndpoint.address().to_string();
        }
    }
    return std::nullopt;
}

void Client::run(asio::io_context& io) {
    std::optional<std::string> ip = discoverServerIp(io);
    if (!ip) {
        throw std::runtime_error("No server detected on the local network.");
    }

    std::string SERVER_IP = *ip;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(SERVER_IP), Server::PORT);
    std::cout << "Client mode : connection made to server" << std::endl;

    asio::error_code ec;
    asio::error_code socketError = socket.connect(endpoint, ec);
    if (ec || socketError) {
        throw std::runtime_error("TCP connection failed");
    }

    std::atomic<bool> disconnected{false};

    std::thread reader([&socket, &disconnected]() {
        try {
            while (true) {
                asio::streambuf buffer;
                asio::error_code ec;
                std::size_t n = asio::read_until(socket, buffer, '\n', ec);
                if (ec) {
                    disconnected = true;
                    break;
                }
                if (n == 0) {
                    disconnected = true;
                    break;
                }
                std::istream is(&buffer);
                std::string msg;
                std::getline(is, msg);
                std::cout   << "\r"
                            << "\33[2K"
                            << "[Server] : " << msg << std::endl
                            << "[Your text] : "
                            << std::flush;
            }
        } catch (const std::exception& e) {
            disconnected = true;
        }
        if (disconnected) {
            asio::io_context new_io;
            Server server;
            server.run(new_io);
        }
    });

    std::string input;
    while (!disconnected) {
        std::cout << "[Your text] : ";
        if (!std::getline(std::cin, input)) break;

        input += "\n";
        asio::error_code write_ec;
        asio::write(socket, asio::buffer(input), write_ec);
        if (write_ec) {
            disconnected = true;
            break;
        }
    }

    if (reader.joinable()) {
        reader.join();
    }
}

void Client::startUdpDiscoveryServer(asio::io_context& io, std::atomic<bool>* stop_flag) {
    using namespace asio;
    ip::udp::socket socket(io, ip::udp::endpoint(ip::udp::v4(), 54001));
    std::array<char, 1024> buf;

    socket.non_blocking(true);

    while (!stop_flag || !(*stop_flag)) {
        ip::udp::endpoint remoteEndpoint;
        std::error_code ec;
        size_t len = 0;
        try {
            len = socket.receive_from(buffer(buf), remoteEndpoint, 0, ec);
        } catch (...) {
            // nothing to do here, just continue
        }
        if (!ec && len > 0 && std::string(buf.data(), len) == "DISCOVER_CLIPSERVER") {
            std::string reply = "I_AM_CLIPSERVER";
            socket.send_to(buffer(reply), remoteEndpoint);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // avoid unnecessary CPU usage
    }
}
