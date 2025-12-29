#include "ClipTransfer/chat.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QNetworkInterface>

static constexpr int kConnectTimeoutMs = 1000;
static constexpr qsizetype kFileChunkSize = 64 * 1024;
static constexpr qint64 kMaxBufferedBytesToWrite = 256 * 1024;

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
    connect(socket, &QTcpSocket::disconnected, this, &ChatBackend::onSocketDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &ChatBackend::onSocketError);
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
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return;

    for (const auto &ip : peers_) {
        auto *socket = new QTcpSocket(this);
        auto *file = new QFile(filePath, socket);
        if (!file->open(QIODevice::ReadOnly)) {
            socket->deleteLater();
            continue;
        }

        QByteArray header;
        QDataStream out(&header, QIODevice::WriteOnly);
        out << static_cast<int>(FileHeader)
            << nickname_
            << info.fileName()
            << static_cast<qint64>(file->size());

        outgoingFiles_.insert(socket, OutgoingFileState{file, header, static_cast<qint64>(file->size()), false});

        connect(socket, &QTcpSocket::connected, this, [this, socket]() {
            processOutgoing(socket);
        });
        connect(socket, &QTcpSocket::bytesWritten, this, [this, socket](qint64) {
            processOutgoing(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, &ChatBackend::onSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred, this, &ChatBackend::onSocketError);

        socket->connectToHost(ip, static_cast<quint16>(tcpServer_->serverPort()));
        if (!socket->waitForConnected(kConnectTimeoutMs)) {
            cleanupOutgoing(socket);
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }
}

void ChatBackend::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    processIncoming(socket);
}

void ChatBackend::processIncoming(QTcpSocket *socket) {
    if (!socket) return;

    while (true) {
        // Si on est en cours de réception de fichier, consommer d'abord les octets.
        if (incomingFiles_.contains(socket)) {
            auto &st = incomingFiles_[socket];
            if (!st.dest) {
                cleanupIncoming(socket);
                return;
            }

            while (socket->bytesAvailable() > 0 && st.receivedBytes < st.expectedBytes) {
                const qint64 remaining = st.expectedBytes - st.receivedBytes;
                const qint64 toRead = qMin<qint64>(remaining, socket->bytesAvailable());
                QByteArray chunk = socket->read(toRead);
                if (chunk.isEmpty()) break;
                st.dest->write(chunk);
                st.receivedBytes += chunk.size();
            }

            if (st.receivedBytes >= st.expectedBytes) {
                st.dest->close();
                emit newMessage("Système", "Fichier reçu : " + st.fileName);
                cleanupIncoming(socket);
                continue; // Il peut rester d'autres messages dans le buffer TCP.
            }

            return; // On attend plus d'octets (prochain readyRead)
        }

        // Sinon, parser un en-tête logique (type + from) sans bloquer.
        QDataStream in(socket);
        int type = 0;
        QString from;

        in.startTransaction();
        in >> type >> from;
        if (!in.commitTransaction()) {
            return; // Header incomplet
        }

        if (type == Message) {
            QString text;
            in.startTransaction();
            in >> text;
            if (!in.commitTransaction()) {
                return; // Message incomplet
            }
            emit newMessage(from, text);
            continue;
        }

        if (type == FileHeader) {
            QString fileName;
            qint64 size = 0;
            in.startTransaction();
            in >> fileName >> size;
            if (!in.commitTransaction()) {
                return; // Header fichier incomplet
            }

            const QString safeName = QFileInfo(fileName).fileName();
            auto *dest = new QFile("received_" + safeName, this);
            if (!dest->open(QIODevice::WriteOnly)) {
                dest->deleteLater();
                return;
            }

            IncomingFileState st;
            st.dest = dest;
            st.fileName = safeName;
            st.from = from;
            st.expectedBytes = size;
            st.receivedBytes = 0;
            incomingFiles_.insert(socket, st);

            // Tenter de consommer immédiatement les octets déjà reçus.
            continue;
        }

        // Type inconnu: ignorer le reste du buffer pour éviter boucle.
        socket->readAll();
        return;
    }
}

void ChatBackend::processOutgoing(QTcpSocket *socket) {
    if (!socket) return;
    if (!outgoingFiles_.contains(socket)) return;
    auto &st = outgoingFiles_[socket];
    if (!st.file) {
        cleanupOutgoing(socket);
        socket->disconnectFromHost();
        return;
    }

    if (socket->state() != QAbstractSocket::ConnectedState) return;

    if (!st.headerSent) {
        socket->write(st.header);
        st.headerSent = true;
    }

    while (socket->bytesToWrite() < kMaxBufferedBytesToWrite && !st.file->atEnd()) {
        const QByteArray chunk = st.file->read(kFileChunkSize);
        if (chunk.isEmpty()) break;
        socket->write(chunk);
    }

    if (st.file->atEnd() && socket->bytesToWrite() == 0) {
        cleanupOutgoing(socket);
        socket->disconnectFromHost();
        socket->deleteLater();
    }
}

void ChatBackend::cleanupIncoming(QTcpSocket *socket) {
    if (!socket) return;
    if (!incomingFiles_.contains(socket)) return;
    auto st = incomingFiles_.take(socket);
    if (st.dest) {
        if (st.dest->isOpen()) st.dest->close();
        st.dest->deleteLater();
    }
}

void ChatBackend::cleanupOutgoing(QTcpSocket *socket) {
    if (!socket) return;
    if (!outgoingFiles_.contains(socket)) return;
    auto st = outgoingFiles_.take(socket);
    if (st.file) {
        if (st.file->isOpen()) st.file->close();
        st.file->deleteLater();
    }
}

void ChatBackend::onSocketDisconnected() {
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    cleanupIncoming(socket);
    cleanupOutgoing(socket);
}

void ChatBackend::onSocketError(QAbstractSocket::SocketError) {
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    cleanupIncoming(socket);
    cleanupOutgoing(socket);
    socket->disconnectFromHost();
    socket->deleteLater();
}