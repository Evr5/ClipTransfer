#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QClipboard>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <QFrame>
#include <QPropertyAnimation>
#include <atomic>
#include <thread>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow(); // <-- virtuel obligatoire

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void sendClipboard();
    void copyLastReceived();
    void sendManualMessage();

private:
    QPushButton *btnSendClipboard;
    QPushButton *btnCopyLast;
    QPushButton *btnSendManual;
    QTextEdit *receivedMessages;
    QLineEdit *manualInput;
    QClipboard *clipboard;
    QString lastReceived;
    std::thread networkThread;
    std::atomic<bool> stopNetwork{false};

    void startNetwork();
    void appendReceivedMessage(const QString &msg);
    void setupStyle();
};

#endif // MAINWINDOW_H
