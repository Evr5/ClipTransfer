#include "ClipTransfer/chat.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QNetworkInterface>

ChatBackend::ChatBackend(QObject *parent) : QObject(parent) {
    tcpServer_ = new QTcpServer(this);
    udpDiscovery_ = new QUdpSocket(this);
    discoveryTimer_ = new QTimer(this);

    connect(tcpServer_, &QTcpServer::newConnection, this, &ChatBackend::onNewConnection);
    connect(udpDiscovery_, &QUdpSocket::readyRead, this, &ChatBackend::readPresence);
    connect(discoveryTimer_, &QTimer::timeout, this, &ChatBackend::broadcastPresence);
}

bool ChatBackend::start(int port) {
    if (!tcpServer_->listen(QHostAddress::Any, static_cast<quint16>(port))) return false;
    
    // Discovery UDP sur le port 50001
    udpDiscovery_->bind(50001, QUdpSocket::ShareAddress);
    discoveryTimer_->start(3000); 
    return true;
}

void ChatBackend::stop() {
    discoveryTimer_->stop();
    tcpServer_->close();
    udpDiscovery_->close();
}

void ChatBackend::broadcastPresence() {
    QByteArray datagram = nickname_.toUtf8();
    udpDiscovery_->writeDatagram(datagram, QHostAddress::Broadcast, 50001);
}

void ChatBackend::readPresence() {
    while (udpDiscovery_->hasPendingDatagrams()) {
        QHostAddress senderIp;
        udpDiscovery_->readDatagram(nullptr, 0, &senderIp);
        
        // On n'ajoute pas sa propre IP
        bool isLocal = false;
        for (const auto &iface : QNetworkInterface::allAddresses()) {
            if (iface == senderIp) { isLocal = true; break; }
        }
        
        if (!isLocal) {
            peers_.insert(senderIp);
        }
    }
}

void ChatBackend::onNewConnection() {
    QTcpSocket *socket = tcpServer_->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &ChatBackend::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void ChatBackend::sendMessage(const QString &text) {
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << static_cast<int>(Message) << nickname_ << text;
    transmit(data);
}

void ChatBackend::transmit(const QByteArray &data) {
    for (const QHostAddress &ad : peers_) {
        QTcpSocket socket;
        socket.connectToHost(ad, static_cast<quint16>(tcpServer_->serverPort()));
        if (socket.waitForConnected(500)) {
            socket.write(data);
            socket.waitForBytesWritten();
            socket.disconnectFromHost();
        }
    }
}

void ChatBackend::sendFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QFileInfo info(file);
    QByteArray header;
    QDataStream out(&header, QIODevice::WriteOnly);
    out << static_cast<int>(FileHeader) << nickname_ << info.fileName() << static_cast<qint64>(file.size());
    
    for (const auto &ip : peers_) {
        QTcpSocket socket;
        socket.connectToHost(ip, static_cast<quint16>(tcpServer_->serverPort()));
        if (socket.waitForConnected(1000)) {
            socket.write(header);
            socket.waitForBytesWritten();
            while (!file.atEnd()) {
                socket.write(file.read(64000));
                socket.waitForBytesWritten();
            }
            socket.disconnectFromHost();
        }
        file.seek(0); // Reset pour le prochain peer
    }
}

void ChatBackend::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    QDataStream in(socket);
    
    int type;
    QString from;
    in >> type >> from;

    if (type == Message) {
        QString text;
        in >> text;
        emit newMessage(from, text);
    } else if (type == FileHeader) {
        QString fileName;
        qint64 size;
        in >> fileName >> size;
        
        QFile *dest = new QFile("received_" + fileName);
        if (dest->open(QIODevice::WriteOnly)) {
            while (dest->size() < size && (socket->bytesAvailable() > 0 || socket->waitForReadyRead(5000))) {
                dest->write(socket->readAll());
            }
            dest->close();
            emit newMessage("Système", "Fichier reçu : " + fileName);
        }
        dest->deleteLater();
    }
}