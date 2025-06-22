#include <iostream>
#include <thread>
#include <asio.hpp>
#include <optional>

using asio::ip::tcp;

constexpr int PORT = 54000;

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
    std::error_code error = socket.connect(endpoint, ec);
    if (ec || error) {
        throw std::runtime_error("TCP connection failed");
    }

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
    while (std::getline(std::cin, line)) {
        asio::write(socket, asio::buffer(line), ec);
        if (ec) break;
    }

    socket.close();
    reader.join();
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

        // Try first as client
        std::thread try_client([&io]() {
            try {
                run_client(io);
            } catch (...) {
                // nothing to do here, we will run the server if client fails
            }
        });

        // Delay to allow client to try connecting
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (io.stopped() == false) {
            std::cout << "No server detected. Starting in server mode.\n";
            std::thread udp_discovery_thread([&io]() {
                start_udp_discovery_server(io);
            });
            run_server(io);
            udp_discovery_thread.join();
        }

        try_client.join();
    } catch (std::exception& e) {
        std::cerr << "Error : " << e.what() << std::endl;
    }
}
