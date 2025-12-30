#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QString>
#include <QSplitter>

#include "ClipTransfer/chat.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void sendClipboard();
    void sendManualMessage();
    void copyLastReceived();
    void clearHistory();

private:
    // UI
    QSplitter *splitter_ = nullptr;
    QPlainTextEdit *history_  = nullptr;
    QPushButton *btnSendClip_ = nullptr;
    QPushButton *btnSendManual_ = nullptr;
    QPushButton *btnCopyLast_ = nullptr;
    QPushButton *btnClearHistory_ = nullptr;
    QPlainTextEdit *manualInput_  = nullptr;
    QClipboard *clipboard_    = nullptr;
    QString nickname_;

    // Backend réseau
    ChatBackend chat_;
    QString lastFullMessageContent_;
    static constexpr int kHistoryGrowStepPx = 24;

    void setupUi();
    void appendReceivedMessage(const QString &line);
    void growHistoryAreaStep();
    bool ensureNickname();

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
