#include "ClipTransfer/server.hpp"
#include "ClipTransfer/client.hpp"

#include <iostream>
#include <thread>
#include <atomic>

void Server::run(asio::io_context& io) {
    std::atomic<bool> stopUdp{false};
    std::thread udpDiscoveryThread([&io, &stopUdp]() {
        Client dummy;
        dummy.startUdpDiscoveryServer(io, &stopUdp);
    });

    asio::ip::tcp::acceptor acceptor(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), PORT);
    acceptor.open(endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen();
    std::cout << "Server mode : waiting for connection...\n";
    asio::ip::tcp::socket socket(io);
    acceptor.accept(socket);
    std::cout << "Connection received.\n";

    std::thread reader([&socket]() {
        try {
            while (true) {
                asio::streambuf buffer;
                asio::read_until(socket, buffer, '\n');
                std::istream is(&buffer);
                std::string msg;
                std::getline(is, msg);
                std::cout << "Client: " << msg << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Client disconnected: " << e.what() << std::endl;
        }
    });

    std::string input;
    while (std::getline(std::cin, input)) {
        input += "\n";
        asio::write(socket, asio::buffer(input));
    }

    socket.close();
    if (reader.joinable()) {
        reader.join();
    }

    stopUdp = true;
    if (udpDiscoveryThread.joinable()) {
        udpDiscoveryThread.join();
    }
}
