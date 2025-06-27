#include "MainWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QMetaObject>
#include <QGraphicsDropShadowEffect>
#include <asio.hpp>
#include "ClipTransfer/client.hpp"
#include "ClipTransfer/server.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QClipboard>
#include <QApplication>
#include <QDebug>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace {
    // File de messages à envoyer depuis l'UI vers le réseau
    std::queue<QString> outgoingMessages;
    std::mutex outgoingMutex;
    std::condition_variable outgoingCv;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    QLabel *title = new QLabel("ClipTransfer", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #A3A3FF; margin-bottom: 10px;");

    receivedMessages = new QTextEdit(this);
    receivedMessages->setReadOnly(true);
    receivedMessages->setMinimumHeight(180);
    receivedMessages->setStyleSheet(
        "background-color: #23232B; color: #F8F8FF; border-radius: 10px; font-size: 15px; padding: 8px;"
    );

    btnSendClipboard = new QPushButton("Envoyer le presse-papier", this);
    btnCopyLast = new QPushButton("Copier le dernier message reçu", this);

    QString btnStyle =
        "QPushButton {"
        "  background-color: #4F4FFF;"
        "  color: white;"
        "  border-radius: 18px;"
        "  padding: 10px 20px;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "  margin: 6px 0;"
        "  box-shadow: 0 2px 8px rgba(80,80,160,0.2);"
        "}"
        "QPushButton:hover {"
        "  background-color: #6F6FFF;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3F3FBF;"
        "}";

    btnSendClipboard->setStyleSheet(btnStyle);
    btnCopyLast->setStyleSheet(btnStyle);

    manualInput = new QLineEdit(this);
    manualInput->setPlaceholderText("Écrivez un message à envoyer...");
    manualInput->setStyleSheet(
        "background-color: #23232B; color: #F8F8FF; border-radius: 10px; font-size: 15px; padding: 8px;"
    );

    btnSendManual = new QPushButton("Envoyer", this);
    btnSendManual->setStyleSheet(btnStyle);

    QHBoxLayout *manualLayout = new QHBoxLayout();
    manualLayout->addWidget(manualInput, 1);
    manualLayout->addWidget(btnSendManual);

    mainLayout->addWidget(title);
    mainLayout->addWidget(receivedMessages);
    mainLayout->addWidget(btnSendClipboard);
    mainLayout->addWidget(btnCopyLast);
    mainLayout->addLayout(manualLayout);

    central->setLayout(mainLayout);
    setCentralWidget(central);

    clipboard = QApplication::clipboard();

    connect(btnSendClipboard, &QPushButton::clicked, this, &MainWindow::sendClipboard);
    connect(btnCopyLast, &QPushButton::clicked, this, &MainWindow::copyLastReceived);
    connect(btnSendManual, &QPushButton::clicked, this, &MainWindow::sendManualMessage);

    setupStyle();
    startNetwork();

    // Arrêt propre du thread réseau à la fermeture de la fenêtre
    connect(qApp, &QApplication::aboutToQuit, this, [this]() {
        stopNetwork = true;
        outgoingCv.notify_all();
        if (networkThread.joinable()) networkThread.join();
    });
}

MainWindow::~MainWindow()
{
    stopNetwork = true;
    outgoingCv.notify_all();
    if (networkThread.joinable()) networkThread.join();
}

void MainWindow::setupStyle() {
    setStyleSheet("QMainWindow { background-color: #1E1E24; }");
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowIcon(QIcon()); // Ajoutez une icône personnalisée ici si souhaité
}

void MainWindow::sendClipboard() {
    QString text = clipboard->text();
    if (text.isEmpty()) return;
    {
        std::lock_guard<std::mutex> lock(outgoingMutex);
        outgoingMessages.push(text);
    }
    outgoingCv.notify_one();
    appendReceivedMessage("[Vous] : " + text);
}

void MainWindow::copyLastReceived() {
    clipboard->setText(lastReceived);
}

void MainWindow::sendManualMessage() {
    QString text = manualInput->text();
    if (!text.isEmpty()) {
        {
            std::lock_guard<std::mutex> lock(outgoingMutex);
            outgoingMessages.push(text);
        }
        outgoingCv.notify_one();
        appendReceivedMessage("[Vous] : " + text);
        manualInput->clear();
    }
}

void MainWindow::appendReceivedMessage(const QString &msg) {
    lastReceived = msg;
    receivedMessages->append(msg);
}

void MainWindow::startNetwork() {
    networkThread = std::thread([this]() {
        asio::io_context io;
        Client client;
        bool isClient = true;
        try {
            // --- CLIENT MODE ---
            std::optional<std::string> ip = client.discoverServerIp(io);
            if (!ip) throw std::runtime_error("No server detected on the local network.");
            std::string SERVER_IP = *ip;
            asio::ip::tcp::socket socket(io);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(SERVER_IP), Server::PORT);
            asio::error_code ec;
            asio::error_code socketError = socket.connect(endpoint, ec);
            if (ec || socketError) throw std::runtime_error("TCP connection failed");

            std::atomic<bool> disconnected{false};

            // Thread de réception
            std::thread reader([&]() {
                try {
                    while (!stopNetwork && !disconnected) {
                        asio::streambuf buffer;
                        asio::read_until(socket, buffer, '\n');
                        std::istream is(&buffer);
                        std::string msg;
                        std::getline(is, msg);
                        if (!msg.empty()) {
                            QMetaObject::invokeMethod(this, "appendReceivedMessage", Qt::QueuedConnection,
                                Q_ARG(QString, "[Serveur] : " + QString::fromStdString(msg)));
                        }
                    }
                } catch (...) {
                    disconnected = true;
                }
            });

            // Boucle d'envoi
            while (!stopNetwork && !disconnected) {
                std::unique_lock<std::mutex> lock(outgoingMutex);
                outgoingCv.wait(lock, [this] { return !outgoingMessages.empty() || stopNetwork; });
                if (stopNetwork) break;
                while (!outgoingMessages.empty()) {
                    QString msg = outgoingMessages.front();
                    outgoingMessages.pop();
                    lock.unlock();
                    std::string toSend = msg.toStdString() + "\n";
                    asio::write(socket, asio::buffer(toSend));
                    lock.lock();
                }
            }

            if (reader.joinable()) reader.join();
        } catch (...) {
            isClient = false;
        }

        if (!isClient) {
            // --- SERVER MODE ---
            std::atomic<bool> stopUdp{false};
            std::thread udpDiscoveryThread([&io, &stopUdp]() {
                Client dummy;
                dummy.startUdpDiscoveryServer(io, &stopUdp);
            });

            asio::ip::tcp::acceptor acceptor(io);
            asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), Server::PORT);
            acceptor.open(endpoint.protocol());
            acceptor.set_option(asio::socket_base::reuse_address(true));
            acceptor.bind(endpoint);
            acceptor.listen();

            std::vector<std::shared_ptr<asio::ip::tcp::socket>> clients;
            std::mutex clientsMutex;

            // Thread de réception pour chaque client
            auto handle_client = [this, &clients, &clientsMutex](std::shared_ptr<asio::ip::tcp::socket> socket) {
                try {
                    while (!stopNetwork) {
                        asio::streambuf buffer;
                        asio::read_until(*socket, buffer, '\n');
                        std::istream is(&buffer);
                        std::string msg;
                        std::getline(is, msg);
                        if (!msg.empty()) {
                            QMetaObject::invokeMethod(this, "appendReceivedMessage", Qt::QueuedConnection,
                                Q_ARG(QString, "[Client] : " + QString::fromStdString(msg)));
                            // Broadcast aux autres clients
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            for (auto& s : clients) {
                                if (s != socket && s->is_open()) {
                                    asio::write(*s, asio::buffer(msg + "\n"));
                                }
                            }
                        }
                    }
                } catch (...) {
                    socket->close();
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clients.erase(std::remove(clients.begin(), clients.end(), socket), clients.end());
                }
            };

            // Thread d'envoi depuis l'UI
            std::thread sender([this, &clients, &clientsMutex]() {
                while (!stopNetwork) {
                    std::unique_lock<std::mutex> lock(outgoingMutex);
                    outgoingCv.wait(lock, [this] { return !outgoingMessages.empty() || stopNetwork; });
                    if (stopNetwork) break;
                    while (!outgoingMessages.empty()) {
                        QString msg = outgoingMessages.front();
                        outgoingMessages.pop();
                        lock.unlock();
                        std::string toSend = msg.toStdString() + "\n";
                        std::lock_guard<std::mutex> lock2(clientsMutex);
                        for (auto& s : clients) {
                            if (s->is_open()) {
                                asio::write(*s, asio::buffer(toSend));
                            }
                        }
                        lock.lock();
                    }
                }
            });

            // Boucle d'acceptation des clients
            while (!stopNetwork) {
                auto socket = std::make_shared<asio::ip::tcp::socket>(io);
                asio::error_code ec;
                acceptor.accept(*socket, ec);
                if (ec) continue;
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clients.push_back(socket);
                }
                std::thread(handle_client, socket).detach();
            }

            stopUdp = true;
            if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
            if (sender.joinable()) sender.join();
        }
    });
}
