#include <iostream>
#include <thread>
#include <asio.hpp>

using asio::ip::tcp;

constexpr int PORT = 54000;
constexpr const char* SERVER_IP = "192.168.1.100"; // à adapter

void run_server(asio::io_context& io) {
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), PORT));
    std::cout << "Mode serveur : en attente de connexion...\n";
    tcp::socket socket(io);
    acceptor.accept(socket);
    std::cout << "Connexion reçue.\n";

    for (;;) {
        std::array<char, 1024> data;
        std::error_code error;
        size_t length = socket.read_some(asio::buffer(data), error);
        if (error) break;

        std::cout << "Reçu : " << std::string(data.data(), length) << std::endl;
    }
}

void run_client(asio::io_context& io) {
    tcp::socket socket(io);
    tcp::endpoint endpoint(asio::ip::make_address(SERVER_IP), PORT);
    std::cout << "Essai de connexion au serveur...\n";

    std::error_code ec;
    socket.connect(endpoint, ec);
    if (ec) {
        throw std::runtime_error("Connexion impossible");
    }

    std::cout << "Connecté. Tapez du texte à envoyer :\n";
    std::string line;
    while (std::getline(std::cin, line)) {
        asio::write(socket, asio::buffer(line), ec);
        if (ec) break;
    }
}

int main() {
    try {
        asio::io_context io;

        // Essayer d'abord comme client
        std::thread try_client([&io]() {
            try {
                run_client(io);
            } catch (...) {
                // rien, on laisse tomber, l'autre thread gérera
            }
        });

        // Délai pour laisser au client une chance
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (io.stopped() == false) {
            std::cout << "Aucun serveur détecté. Lancement en mode serveur.\n";
            run_server(io);
        }

        try_client.join();
    } catch (std::exception& e) {
        std::cerr << "Erreur : " << e.what() << std::endl;
    }
}
