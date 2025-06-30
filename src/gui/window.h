#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#pragma once

#include <QMainWindow>
#include <QThread>
#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
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

#endif // MAINWINDOW_H
