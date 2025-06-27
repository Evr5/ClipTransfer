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
}

MainWindow::~MainWindow()
{
    stopNetwork = true;
    if (networkThread.joinable()) networkThread.join();
}

void MainWindow::setupStyle() {
    setStyleSheet("QMainWindow { background-color: #1E1E24; }");
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowIcon(QIcon()); // Ajoutez une icône personnalisée ici si souhaité
}

void MainWindow::sendClipboard() {
    QString text = clipboard->text();
    // TODO: envoyer le texte du presse-papier au serveur/clients
    appendReceivedMessage("[Vous] : " + text);
}

void MainWindow::copyLastReceived() {
    clipboard->setText(lastReceived);
}

void MainWindow::sendManualMessage() {
    QString text = manualInput->text();
    if (!text.isEmpty()) {
        // TODO: envoyer le texte saisi au serveur/clients
        appendReceivedMessage("[Vous] : " + text);
        manualInput->clear();
    }
}

void MainWindow::appendReceivedMessage(const QString &msg) {
    lastReceived = msg;
    receivedMessages->append(msg);
}

void MainWindow::startNetwork() {
    // TODO: intégrer la logique réseau ici, et appeler appendReceivedMessage via QMetaObject::invokeMethod
    networkThread = std::thread([this]() {
        asio::io_context io;
        Client client;
        Server server;
        try {
            client.run(io);
        } catch (...) {
            asio::io_context new_io;
            server.run(new_io);
        }
        // TODO: intégrer la réception des messages et appeler appendReceivedMessage via QMetaObject::invokeMethod
    });
}
