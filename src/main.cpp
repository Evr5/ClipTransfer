#include "ClipTransfer/client.hpp"
#include "ClipTransfer/server.hpp"

#include <iostream>

int main() {
    Client client;
    Server server;
    asio::io_context io;

    try {
        try {
            client.run(io);
        } catch (...) {
            asio::io_context new_io;
            server.run(new_io);
        }

    } catch (std::exception& e) {
        std::cerr << "Error : " << e.what() << std::endl;
    }
}