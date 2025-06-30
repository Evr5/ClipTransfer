#ifndef SERVER_HPP
#define SERVER_HPP

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <asio.hpp>
#pragma clang diagnostic pop

class Server {
    public:
        static constexpr int PORT = 54000;

        Server() = default;
        ~Server() = default;

        void run(asio::io_context& io);
};

#endif // SERVER_HPP
