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
#include <thread>
#include <chrono>
#include <QCloseEvent>

namespace {
    // File de messages à envoyer depuis l'UI vers le réseau
    std::queue<QString> outgoingMessages;
    std::mutex outgoingMutex;
    std::condition_variable outgoingCv;

    // Ajout : fonction utilitaire pour vider la file de messages
    void clearOutgoingMessages() {
        std::lock_guard<std::mutex> lock(outgoingMutex);
        std::queue<QString> empty;
        std::swap(outgoingMessages, empty);
    }
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
    connect(manualInput, &QLineEdit::returnPressed, this, &MainWindow::sendManualMessage); // <-- Ajouté

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
        while (!stopNetwork) {
            clearOutgoingMessages();
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
                socket.connect(endpoint, ec);
                if (ec) throw std::runtime_error("TCP connection failed");

                std::atomic<bool> disconnected{false};

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

                while (!stopNetwork && !disconnected) {
                    std::unique_lock<std::mutex> lock(outgoingMutex);
                    outgoingCv.wait(lock, [this] { return !outgoingMessages.empty() || stopNetwork; });
                    if (stopNetwork) break;
                    while (!outgoingMessages.empty()) {
                        QString msg = outgoingMessages.front();
                        outgoingMessages.pop();
                        lock.unlock();
                        std::string toSend = msg.toStdString() + "\n";
                        asio::error_code write_ec;
                        asio::write(socket, asio::buffer(toSend), write_ec);
                        if (write_ec) {
                            disconnected = true;
                            break;
                        }
                        lock.lock();
                    }
                }

                if (reader.joinable()) reader.join();

                if (disconnected && !stopNetwork) {
                    isClient = false;
                } else {
                    break;
                }
            } catch (...) {
                isClient = false;
            }

            if (!isClient && !stopNetwork) {
                // --- SERVER MODE ---
                std::atomic<bool> stopUdp{false};
                std::thread udpDiscoveryThread([&io, &stopUdp]() {
                    Client dummy;
                    dummy.startUdpDiscoveryServer(io, &stopUdp);
                });

                asio::ip::tcp::acceptor acceptor(io);
                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), Server::PORT);

                asio::error_code ec;
                acceptor.open(endpoint.protocol(), ec);
                if (ec) {
                    stopUdp = true;
                    if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                acceptor.set_option(asio::socket_base::reuse_address(true), ec);
                if (ec) {
                    stopUdp = true;
                    if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                acceptor.bind(endpoint, ec);
                if (ec) {
                    // Port déjà utilisé : repasser client sans crash
                    stopUdp = true;
                    if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                acceptor.listen();

                // --- Correction : attendre qu'au moins un client soit connecté avant d'accepter les messages à envoyer ---
                std::vector<std::shared_ptr<asio::ip::tcp::socket>> clients;
                std::mutex clientsMutex;
                std::atomic<bool> serverRunning{true};
                std::atomic<bool> hasClient{false};

                // Thread d'envoi depuis l'UI
                std::thread sender([this, &clients, &clientsMutex, &serverRunning, &hasClient]() {
                    while (!stopNetwork && serverRunning) {
                        std::unique_lock<std::mutex> lock(outgoingMutex);
                        outgoingCv.wait(lock, [this] { return !outgoingMessages.empty() || stopNetwork; });
                        if (stopNetwork) break;
                        while (!outgoingMessages.empty()) {
                            // --- Correction : attendre qu'il y ait au moins un client ---
                            if (!hasClient) {
                                lock.unlock();
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                lock.lock();
                                continue;
                            }
                            QString msg = outgoingMessages.front();
                            outgoingMessages.pop();
                            lock.unlock();
                            std::string toSend = msg.toStdString() + "\n";
                            std::lock_guard<std::mutex> lock2(clientsMutex);
                            for (auto& s : clients) {
                                if (s->is_open()) {
                                    asio::error_code ec2;
                                    asio::write(*s, asio::buffer(toSend), ec2);
                                }
                            }
                            lock.lock();
                        }
                    }
                });

                auto handle_client = [this, &clients, &clientsMutex, &serverRunning, &hasClient](std::shared_ptr<asio::ip::tcp::socket> socket) {
                    hasClient = true;
                    try {
                        while (!stopNetwork && serverRunning) {
                            asio::streambuf buffer;
                            asio::read_until(*socket, buffer, '\n');
                            std::istream is(&buffer);
                            std::string msg;
                            std::getline(is, msg);
                            if (!msg.empty()) {
                                QMetaObject::invokeMethod(this, "appendReceivedMessage", Qt::QueuedConnection,
                                    Q_ARG(QString, "[Client] : " + QString::fromStdString(msg)));
                                std::lock_guard<std::mutex> lock(clientsMutex);
                                for (auto& s : clients) {
                                    if (s != socket && s->is_open()) {
                                        asio::error_code ec2;
                                        asio::write(*s, asio::buffer(msg + "\n"), ec2);
                                    }
                                }
                            }
                        }
                    } catch (...) {
                        socket->close();
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        clients.erase(std::remove(clients.begin(), clients.end(), socket), clients.end());
                        if (clients.empty()) hasClient = false;
                    }
                };

                std::vector<std::thread> clientThreads;
                try {
                    while (!stopNetwork) {
                        auto socket = std::make_shared<asio::ip::tcp::socket>(io);
                        asio::error_code ec3;
                        acceptor.accept(*socket, ec3);
                        if (ec3) {
                            if (ec3 == asio::error::address_in_use) {
                                serverRunning = false;
                                break;
                            }
                            continue;
                        }
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            clients.push_back(socket);
                        }
                        clientThreads.emplace_back(handle_client, socket);
                        hasClient = true;
                    }
                } catch (...) {
                    serverRunning = false;
                }

                stopUdp = true;
                if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                if (sender.joinable()) sender.join();
                for (auto& t : clientThreads) {
                    if (t.joinable()) t.join();
                }

                if (!stopNetwork && !serverRunning) {
                    continue;
                } else {
                    break;
                }
            }
        }
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopNetwork = true;
    outgoingCv.notify_all();
    // Attendre la fin du thread réseau sans bloquer l'UI
    if (networkThread.joinable()) {
        // Débloquer le thread réseau dans un thread temporaire pour éviter le freeze de l'UI
        std::thread joiner([this]() {
            networkThread.join();
            QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
        });
        joiner.detach();
        event->ignore(); // On ignore la fermeture pour laisser le thread finir puis quitter proprement
    } else {
        QMainWindow::closeEvent(event);
    }
}
