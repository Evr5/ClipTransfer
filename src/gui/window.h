#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
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
    QString lastReceived_;
    QString lastFullMessageContent_;

    int historyAppendCount_ = 0;
    static constexpr int kMaxDisplayLinesPerMessage = 50;
    static constexpr int kMaxDisplayCharsPerMessage = 20000;
    static constexpr int kAutoScrollEveryNAppends = 1;
    static constexpr int kHistoryGrowStepPx = 24;

    void setupUi();
    void appendReceivedMessage(const QString &line);
    QString truncateForDisplay(const QString& text) const;
    void growHistoryAreaStep();
    bool ensureNickname();

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
