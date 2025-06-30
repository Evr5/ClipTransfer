#ifndef SERVER_HPP
#define SERVER_HPP

#include <asio.hpp>

class Server {
    public:
        static constexpr int PORT = 54000;

        Server() = default;
        ~Server() = default;

        void run(asio::io_context& io);
};

#endif // SERVER_HPP
