#ifndef CLIENT_HPP
#define CLIENT_HPP

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <asio.hpp>
#pragma clang diagnostic pop

#include <optional>

class Client {
    public:
        Client() = default;
        ~Client() = default;

        static std::string getLocalIp();
        std::optional<std::string> discoverServerIp(asio::io_context& io);
        void run(asio::io_context& io);
        void startUdpDiscoveryServer(asio::io_context& io, std::atomic<bool>* stop_flag = nullptr);
};

#endif // CLIENT_HPP
