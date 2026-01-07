#include "chat_controller.h"

#include <QMetaObject>

#if defined(Q_OS_ANDROID)
    #include <QJniObject>
    #if __has_include(<QNativeInterface>)
        #include <QNativeInterface>
    #elif __has_include(<QtCore/QNativeInterface>)
        #include <QtCore/QNativeInterface>
    #elif __has_include(<QtCore/qnativeinterface.h>)
        #include <QtCore/qnativeinterface.h>
    #endif
#endif

namespace {

void androidAcquireMulticastLock() {
#if defined(Q_OS_ANDROID)
    // Calls: org.cliptransfer.Net.acquireMulticastLock(android.content.Context)
    auto ctx = QNativeInterface::QAndroidApplication::context();
    if (!ctx.isValid()) return;

    QJniObject::callStaticMethod<void>(
        "org/cliptransfer/Net",
        "acquireMulticastLock",
        "(Landroid/content/Context;)V",
        ctx.object()
    );
#endif
}

}

ChatController::ChatController(QObject* parent)
    : QObject(parent) {
    clipboard_ = QGuiApplication::clipboard();

    QSettings settings;
    nickname_ = settings.value("user/nickname", "").toString().trimmed();

    if (!nickname_.isEmpty()) {
        chat_.setNickname(nickname_.toStdString());
        start();
    }
}

ChatController::~ChatController() {
    stop();
}

void ChatController::setNickname(const QString& nickname) {
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty()) return;
    if (trimmed == nickname_) return;
    if (trimmed.contains('|')) return; // keep protocol safe

    // Nickname should be entered only once (first app start).
    // If it's already set, ignore further changes.
    if (!nickname_.isEmpty()) return;

    nickname_ = trimmed;

    QSettings settings;
    settings.setValue("user/nickname", nickname_);

    chat_.setNickname(nickname_.toStdString());
    emit nicknameChanged();

    // Auto-start once the user has provided the nickname the first time.
    start();
}

void ChatController::start() {
    if (running_) return;
    if (nickname_.trimmed().isEmpty()) return;

    androidAcquireMulticastLock();

    const bool ok = chat_.start([this](const std::string& fromName, const std::string& text) {
        const QString qFrom = QString::fromStdString(fromName);
        const QString qText = QString::fromStdString(text);

        QMetaObject::invokeMethod(this, [this, qFrom, qText]() {
            lastFullMessage_ = qText;
            emit lastFullMessageChanged();
            appendLine(tr("[%1] a envoyé un message.").arg(qFrom));
        });
    });

    running_ = ok;
    emit runningChanged();

    if (!ok) {
        appendLine(tr("[Erreur] Impossible de démarrer le chat réseau."));
    }
}

void ChatController::stop() {
    if (!running_) return;
    chat_.stop();
    running_ = false;
    emit runningChanged();
}

void ChatController::sendClipboard() {
    if (!running_) return;
    if (!clipboard_) return;

    const QString text = clipboard_->text();
    if (text.isEmpty()) return;

    lastFullMessage_ = text;
    emit lastFullMessageChanged();

    chat_.enqueueMessage(text.toStdString());
    appendLine(tr("[%1] a envoyé un message.").arg(nickname_));
}

void ChatController::sendText(const QString& text) {
    if (!running_) return;

    const QString t = text;
    if (t.trimmed().isEmpty()) return;

    lastFullMessage_ = t;
    emit lastFullMessageChanged();

    chat_.enqueueMessage(t.toStdString());
    appendLine(tr("[%1] a envoyé un message.").arg(nickname_));
}

void ChatController::copyLastReceivedToClipboard() {
    if (!clipboard_) return;
    if (lastFullMessage_.isEmpty()) return;
    clipboard_->setText(lastFullMessage_);
}

void ChatController::clearHistory() {
    history_.clear();
    emit historyChanged();
    chat_.clearHistory();
}

void ChatController::appendLine(const QString& line) {
    // Keep memory bounded on mobile
    static constexpr int kMaxLines = 4000;

    history_.append(line);
    if (history_.size() > kMaxLines) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - kMaxLines));
    }
    emit historyChanged();
}
