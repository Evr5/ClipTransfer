#include "ClipTransfer/client.hpp"
#include "ClipTransfer/server.hpp"

#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <iostream>

std::string Client::getLocalIp() {
    asio::io_context ctx;
    asio::ip::udp::resolver resolver(ctx);
    asio::ip::udp::resolver::query query("8.8.8.8", "80");
    asio::ip::udp::endpoint ep = *resolver.resolve(query).begin();
    return ep.address().to_string();
}

std::optional<std::string> Client::discoverServerIp(asio::io_context& io) {
    using namespace asio;
    ip::udp::socket socket(io);
    socket.open(ip::udp::v4());
    socket.set_option(socket_base::broadcast(true));

    ip::udp::endpoint broadcast_endpoint(ip::address_v4::broadcast(), 54001);
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

    std::error_code ec;
    std::error_code socketError = socket.connect(endpoint, ec);
    if (ec) {
        throw std::runtime_error("TCP connection failed");
    }
    if (socketError) {
        throw std::runtime_error("TCP connection failed: " + socketError.message());
    }

    std::atomic<bool> disconnected{false};

    std::thread reader([&socket, &disconnected]() {
        try {
            while (true) {
                asio::streambuf buffer;
                asio::read_until(socket, buffer, '\n');
                std::istream is(&buffer);
                std::string msg;
                std::getline(is, msg);
                std::cout << "Server: " << msg << std::endl;
            }
        } catch (const std::exception& e) {
            disconnected = true;
            // Client become a server if the old server disconnects
            asio::io_context new_io;
            Server server;
            server.run(new_io);
        }
    });

    std::string input;
    while (!disconnected && std::getline(std::cin, input)) {
        input += "\n";
        asio::write(socket, asio::buffer(input));
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