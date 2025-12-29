#ifndef CHAT_HPP
#define CHAT_HPP

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QSet>
#include <QTimer>
#include <QString>
#include <QHash>

class QFile;

class ChatBackend : public QObject {
    Q_OBJECT
public:
    enum DataType { Message, FileHeader };

    explicit ChatBackend(QObject *parent = nullptr);
    
    // Démarre le serveur sur le port spécifié
    bool start(int port = 50000);
    void stop();

    void sendMessage(const QString &text);
    void sendFile(const QString &filePath);
    void setNickname(const QString &name) { nickname_ = name; }

signals:
    // Signal émis vers l'interface graphique lors d'une réception
    void newMessage(const QString &from, const QString &text);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void readPresence();
    void broadcastPresence();

private:
    void transmit(const QByteArray &data);
    void processIncoming(QTcpSocket *socket);
    void processOutgoing(QTcpSocket *socket);
    void cleanupIncoming(QTcpSocket *socket);
    void cleanupOutgoing(QTcpSocket *socket);

    struct IncomingFileState {
        QFile *dest = nullptr;
        QString fileName;
        QString from;
        qint64 expectedBytes = 0;
        qint64 receivedBytes = 0;
    };

    struct OutgoingFileState {
        QFile *file = nullptr;
        QByteArray header;
        qint64 totalBytes = 0;
        bool headerSent = false;
    };

    QString nickname_{"Anonyme"};
    QTcpServer *tcpServer_ = nullptr;
    QUdpSocket *udpDiscovery_ = nullptr;
    QTimer *discoveryTimer_ = nullptr;
    QSet<QHostAddress> peers_; // Liste des IP découvertes sur le LAN

    QHash<QTcpSocket*, IncomingFileState> incomingFiles_;
    QHash<QTcpSocket*, OutgoingFileState> outgoingFiles_;
};

#endif // CHAT_HPP