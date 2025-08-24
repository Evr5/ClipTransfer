#include "window.h"

#include "ClipTransfer/client.hpp"
#include "ClipTransfer/server.hpp"

#include <asio/error_code.hpp>
#include <iostream>
#include <thread>
#include <queue>
#include <asio.hpp>

#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QClipboard>

namespace {
    // Queue of messages to send from the UI to the network
    std::queue<QString> outgoingMessages;
    std::mutex outgoingMutex;
    std::condition_variable outgoingCv;

    // Added: utility function to clear the message queue
    void clearOutgoingMessages() {
        std::lock_guard<std::mutex> lock(outgoingMutex);
        std::queue<QString> empty;
        std::swap(outgoingMessages, empty);
    }
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
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

    btnSendClipboard = new QPushButton("Send clipboard", this);
    btnCopyLast = new QPushButton("Copy last received message", this);

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
    manualInput->setPlaceholderText("Write a message to send...");
    manualInput->setStyleSheet(
        "background-color: #23232B; color: #F8F8FF; border-radius: 10px; font-size: 15px; padding: 8px;"
    );

    btnSendManual = new QPushButton("Send", this);
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

    // Properly stop the network thread when the window is closed
    connect(qApp, &QApplication::aboutToQuit, this, [this]() {
        stopNetwork = true;
        outgoingCv.notify_all();
        if (networkThread.joinable()) networkThread.join();
    });
}

MainWindow::~MainWindow() {
    stopNetwork = true;
    outgoingCv.notify_all();
    if (networkThread.joinable()) networkThread.join();
}

void MainWindow::setupStyle() {
    setStyleSheet("QMainWindow { background-color: #1E1E24; }");
    setWindowIcon(QIcon());
}

void MainWindow::sendClipboard() {
    QString text = clipboard->text();
    if (text.isEmpty()) return;
    {
        std::lock_guard<std::mutex> lock(outgoingMutex);
        outgoingMessages.push(text);
    }
    outgoingCv.notify_one();
    appendReceivedMessage("[You] : " + text);
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
        appendReceivedMessage("[You] : " + text);
        manualInput->clear();
    }
}

QString removePrefix(const QString& str) {
    const QStringList prefixes = {"[You] : ", "[Client] : ", "[Server] : "};
    for (const auto& prefix : prefixes) {
        if (str.startsWith(prefix)) { 
            return str.mid(prefix.length());
        }
    }
    return str;
}

void MainWindow::appendReceivedMessage(const QString &msg) {
    lastReceived = removePrefix(msg);
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
                asio::error_code socketError = socket.connect(endpoint, ec);
                if (ec) throw std::runtime_error("TCP connection failed");

                std::atomic<bool> disconnected{false};

                std::thread reader([&]() {
                    try {
                        while (!stopNetwork && !disconnected) {
                            asio::streambuf buffer;
                            asio::error_code ec;
                            std::size_t n = asio::read_until(socket, buffer, '\n', ec);
                            if (ec) {
                                disconnected = true;
                                outgoingCv.notify_all();
                                break;
                            }
                            if (n == 0) {
                                disconnected = true;
                                outgoingCv.notify_all();
                                break;
                            }
                            std::istream is(&buffer);
                            std::string msg;
                            std::getline(is, msg);
                            if (!msg.empty()) {
                                QMetaObject::invokeMethod(this, "appendReceivedMessage", Qt::QueuedConnection,
                                    Q_ARG(QString, "[Server] : " + QString::fromStdString(msg)));
                            }
                        }
                    } catch (...) {
                        disconnected = true;
                        outgoingCv.notify_all();
                    }
                });

                while (!stopNetwork && !disconnected) {
                    std::unique_lock<std::mutex> lock(outgoingMutex);
                    outgoingCv.wait(lock, [this, &disconnected] { 
                        return !outgoingMessages.empty() || stopNetwork || disconnected; 
                    });
                    if (stopNetwork || disconnected) break;
                    while (!outgoingMessages.empty()) {
                        QString msg = outgoingMessages.front();
                        outgoingMessages.pop();
                        lock.unlock();
                        std::string toSend = msg.toStdString() + "\n";
                        asio::error_code write_ec;
                        asio::write(socket, asio::buffer(toSend), write_ec);
                        if (write_ec) {
                            disconnected = true;
                            outgoingCv.notify_all();
                            break;
                        }
                        lock.lock();
                    }
                }

                if (reader.joinable()) reader.join();

                if (disconnected && !stopNetwork) {
                    isClient = false;
                    // let the loop continue to switch to server mode
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
                asio::error_code er = acceptor.open(endpoint.protocol(), ec);
                if (ec) {
                    stopUdp = true;
                    if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                asio::error_code error = acceptor.set_option(asio::socket_base::reuse_address(true), ec);
                if (ec) {
                    stopUdp = true;
                    if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                asio::error_code errorBind = acceptor.bind(endpoint, ec);
                if (ec) {
                    // Port already in use: switch back to client mode without crashing
                    stopUdp = true;
                    if (udpDiscoveryThread.joinable()) udpDiscoveryThread.join();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                acceptor.listen();

                // Waiting for at least one client to connect before accepting messages to send
                std::vector<std::shared_ptr<asio::ip::tcp::socket>> clients;
                std::mutex clientsMutex;
                std::atomic<bool> serverRunning{true};
                std::atomic<bool> hasClient{false};

                // Thread for sending messages from the UI
                std::thread sender([this, &clients, &clientsMutex, &serverRunning, &hasClient]() {
                    while (!stopNetwork && serverRunning) {
                        std::unique_lock<std::mutex> lock(outgoingMutex);
                        outgoingCv.wait(lock, [this] { return !outgoingMessages.empty() || stopNetwork; });
                        if (stopNetwork) break;
                        while (!outgoingMessages.empty()) {
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
                        asio::error_code errorAcceptor = acceptor.accept(*socket, ec3);
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

void MainWindow::closeEvent(QCloseEvent *event) {
    stopNetwork = true;
    outgoingCv.notify_all();
    if (networkThread.joinable()) {
        networkThread.detach(); // Do not block window closing
    }
    QMainWindow::closeEvent(event);
}
