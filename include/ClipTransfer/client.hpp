#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <optional>
#include <asio.hpp>

class Client {
    public:
        Client() = default;
        ~Client() = default;

        std::string getLocalIp();
        std::optional<std::string> discoverServerIp(asio::io_context& io);
        void run(asio::io_context& io);
        void startUdpDiscoveryServer(asio::io_context& io, std::atomic<bool>* stop_flag = nullptr);
};

#endif // CLIENT_HPP
