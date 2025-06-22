#include <iostream>
#include <thread>
#include <asio.hpp>
#include <optional>
#include <unistd.h>
#include <poll.h>

using asio::ip::tcp;

constexpr int PORT = 54000;

std::atomic<bool> client_connected{false};

std::string get_local_ip() {
    asio::io_context ctx;
    asio::ip::udp::resolver resolver(ctx);
    asio::ip::udp::resolver::query query("8.8.8.8", "80");
    asio::ip::udp::endpoint ep = *resolver.resolve(query).begin();
    return ep.address().to_string();
}

void run_server(asio::io_context& io) {
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), PORT));
    std::cout << "Server mode : waiting for connection...\n";
    tcp::socket socket(io);
    acceptor.accept(socket);
    std::cout << "Connection received.\n";

    std::thread reader([&socket]() {
        std::array<char, 1024> data;
        std::error_code error;
        while (true) {
            size_t length = socket.read_some(asio::buffer(data), error);
            if (error) break;
            std::cout << "[Received] : " << std::string(data.data(), length) << std::endl;
        }
    });

    std::string line;
    std::error_code ec;
    while (std::getline(std::cin, line)) {
        asio::write(socket, asio::buffer(line), ec);
        if (ec) break;
    }

    socket.close();
    reader.join();
}


std::optional<std::string> discover_server_ip(asio::io_context& io) {
    using namespace asio;
    ip::udp::socket socket(io);
    socket.open(ip::udp::v4());
    socket.set_option(socket_base::broadcast(true));

    ip::udp::endpoint broadcast_endpoint(ip::address_v4::broadcast(), 54001);
    std::string message = "DISCOVER_CLIPSERVER";
    socket.send_to(buffer(message), broadcast_endpoint);

    std::array<char, 1024> recv_buf;
    ip::udp::endpoint sender_endpoint;
    socket.non_blocking(true);

    for (int i = 0; i < 50; ++i) { // wait max 5 sec
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        asio::error_code ec;
        size_t len = socket.receive_from(buffer(recv_buf), sender_endpoint, 0, ec);
        if (!ec && std::string(recv_buf.data(), len) == "I_AM_CLIPSERVER") {
            return sender_endpoint.address().to_string();
        }
    }
    return std::nullopt;
}

void start_udp_discovery_server(asio::io_context& io);

void run_client(asio::io_context& io) {
    auto ip_opt = discover_server_ip(io);
    if (!ip_opt) {
        throw std::runtime_error("No server detected on the local network.");
    }

    std::string SERVER_IP = *ip_opt;
    tcp::socket socket(io);
    tcp::endpoint endpoint(asio::ip::make_address(SERVER_IP), PORT);
    std::cout << "Connection to detected server at : " << SERVER_IP << "\n";

    std::error_code ec;
    socket.connect(endpoint, ec);
    if (ec) {
        throw std::runtime_error("TCP connection failed");
    }

    // Mettre le socket en mode non-bloquant
    socket.non_blocking(true);

    std::atomic<bool> disconnected = false;
    std::atomic<bool> stop_input = false;

    // Thread lecture socket
    std::thread reader([&socket, &disconnected, &stop_input]() {
        std::array<char, 1024> data;
        std::error_code error;
        while (!disconnected && !stop_input) {
            size_t length = 0;
            try {
                length = socket.read_some(asio::buffer(data), error);
            } catch (...) {
                error = asio::error::operation_aborted;
            }
            if (error == asio::error::would_block || error == asio::error::try_again) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (error) {
                disconnected = true;
                stop_input = true;
                break;
            }
            if (length > 0)
                std::cout << "[Received] : " << std::string(data.data(), length) << std::endl;
        }
    });

    // Thread lecture utilisateur non bloquant
    std::thread input_thread([&socket, &ec, &disconnected, &stop_input]() {
        std::string line;
        while (!stop_input && !disconnected) {
            struct pollfd pfd;
            pfd.fd = 0; // stdin
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, 100); // timeout 100ms
            if (ret > 0 && (pfd.revents & POLLIN)) {
                if (!std::getline(std::cin, line)) {
                    stop_input = true;
                    break;
                }
                asio::write(socket, asio::buffer(line), ec);
                if (ec) {
                    disconnected = true;
                    break;
                }
            }
        }
        stop_input = true;
    });

    // Attendre la fin d'un des threads
    while (!disconnected && !stop_input) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    socket.close();
    stop_input = true;
    if (input_thread.joinable()) input_thread.join();
    if (reader.joinable()) reader.join();

    if (disconnected) {
        std::cout << "[Client] Disconnected from server. Becoming new server...\n";
        asio::io_context new_io;
        std::thread udp_discovery_thread([&new_io]() {
            start_udp_discovery_server(new_io);
        });
        run_server(new_io);
        udp_discovery_thread.join();
    }
}


void start_udp_discovery_server(asio::io_context& io) {
    using namespace asio;
    ip::udp::socket socket(io, ip::udp::endpoint(ip::udp::v4(), 54001));
    std::array<char, 1024> recv_buf;

    for (;;) {
        ip::udp::endpoint remote_endpoint;
        std::error_code ec;
        size_t len = socket.receive_from(buffer(recv_buf), remote_endpoint, 0, ec);

        if (!ec && std::string(recv_buf.data(), len) == "DISCOVER_CLIPSERVER") {
            std::string reply = "I_AM_CLIPSERVER";
            socket.send_to(buffer(reply), remote_endpoint);
        }
    }
}


int main() {
    try {
        asio::io_context io;

        try {
            run_client(io);
        } catch (...) {
            std::cout << "No server detected. Starting in server mode.\n";
            asio::io_context new_io;  // nouveau contexte propre
            std::thread udp_discovery_thread([&new_io]() {
                start_udp_discovery_server(new_io);
            });
            run_server(new_io);
            udp_discovery_thread.join();
        }

    } catch (std::exception& e) {
        std::cerr << "Error : " << e.what() << std::endl;
    }
}
