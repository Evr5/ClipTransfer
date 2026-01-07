#pragma once

#include <QObject>
#include <QStringList>
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>

#include "ClipTransfer/chat.hpp"

class ChatController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString nickname READ nickname WRITE setNickname NOTIFY nicknameChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString lastFullMessage READ lastFullMessage NOTIFY lastFullMessageChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)

public:
    explicit ChatController(QObject* parent = nullptr);
    ~ChatController() override;

    QString nickname() const { return nickname_; }
    void setNickname(const QString& nickname);

    QStringList history() const { return history_; }

    QString lastFullMessage() const { return lastFullMessage_; }

    bool running() const { return running_; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    Q_INVOKABLE void sendClipboard();
    Q_INVOKABLE void sendText(const QString& text);
    Q_INVOKABLE void copyLastReceivedToClipboard();
    Q_INVOKABLE void clearHistory();

signals:
    void nicknameChanged();
    void historyChanged();
    void lastFullMessageChanged();
    void runningChanged();

private:
    void appendLine(const QString& line);

    QClipboard* clipboard_ = nullptr;
    ChatBackend chat_;
    QString nickname_;
    QStringList history_;
    QString lastFullMessage_;
    bool running_ = false;
};
