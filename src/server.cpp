#include "ClipTransfer/server.hpp"
#include "ClipTransfer/client.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>

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

    std::vector<std::shared_ptr<asio::ip::tcp::socket>> clients;
    std::vector<std::thread> clientThreads;
    std::mutex clientsMutex;

    std::thread inputThread([&]() {
        std::string input;
        while (true) {
            std::cout << "[Your text] : ";
            if (!std::getline(std::cin, input)) break;

            input += "\n";

            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto it = clients.begin(); it != clients.end();) {
                auto& sock = *it;
                if (sock && sock->is_open()) {
                    asio::error_code ec;
                    asio::write(*sock, asio::buffer(input), ec);
                    if (ec) {
                        sock->close();
                        it = clients.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
    });


    while (true) {
        auto socket = std::make_shared<asio::ip::tcp::socket>(io);
        asio::error_code ec;
        asio::error_code acceptorError = acceptor.accept(*socket, ec);
        if (ec || acceptorError) {
            std::cout << "Accept error: " << ec.message() << std::endl;
            continue;
        }
        
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(socket);
        }

        clientThreads.emplace_back([socket, &clients, &clientsMutex]() {
            try {
                while (true) {
                    asio::streambuf buffer;
                    asio::read_until(*socket, buffer, '\n');
                    std::istream is(&buffer);
                    std::string msg;
                    std::getline(is, msg);
                    std::cout   << "\r"
                            << "\33[2K"
                            << "[Client] : " << msg << std::endl
                            << "[Your text] : "
                            << std::flush;
                }
            } catch (const std::exception& e) {
                socket->close();
                std::lock_guard<std::mutex> lock(clientsMutex);
                // Remove the socket from the clients list
                clients.erase(std::remove(clients.begin(), clients.end(), socket), clients.end());
            }
        });
    }

    if (inputThread.joinable()) {
        inputThread.join();
    }
    for (auto& t : clientThreads) {
        if (t.joinable()) {
            t.join();
        }
    }

    stopUdp = true;
    if (udpDiscoveryThread.joinable()) {
        udpDiscoveryThread.join();
    }
}
