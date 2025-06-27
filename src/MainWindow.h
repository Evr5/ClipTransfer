#pragma once

#include <QMainWindow>
#include <QThread>
#include <atomic>

class QTextEdit;
class QPushButton;
class QLineEdit;
class QClipboard;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void sendClipboard();
    void copyLastReceived();
    void sendManualMessage();

public slots:
    Q_INVOKABLE void appendReceivedMessage(const QString &msg);

private:
    void setupStyle();
    void startNetwork();

    QTextEdit *receivedMessages = nullptr;
    QPushButton *btnSendClipboard = nullptr;
    QPushButton *btnCopyLast = nullptr;
    QPushButton *btnSendManual = nullptr;
    QLineEdit *manualInput = nullptr;
    QClipboard *clipboard = nullptr;

    std::thread networkThread;
    std::atomic<bool> stopNetwork{false};
    QString lastReceived;

protected:
    void closeEvent(QCloseEvent *event) override;
};