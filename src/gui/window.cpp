#include "window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QGuiApplication>
#include <QCloseEvent>
#include <QScrollBar>
#include <QInputDialog>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QMimeData>
#include <QTextDocument>

namespace {

class SafePlainTextEdit final : public QPlainTextEdit {
public:
    explicit SafePlainTextEdit(QWidget* parent = nullptr) : QPlainTextEdit(parent) {}

protected:
    void insertFromMimeData(const QMimeData* source) {
        if (!source || !source->hasText()) {
            QPlainTextEdit::insertFromMimeData(source);
            return;
        }

        QString text = source->text();

        fastPaste(text);
    }

private:
    void fastPaste(const QString& text) {
        setUpdatesEnabled(false);
        bool oldUndoRedo = isUndoRedoEnabled();
        setUndoRedoEnabled(false); // Indispensable pour les gros volumes

        // On remplace tout le contenu ou on insère à la position actuelle
        this->appendPlainText(text); 

        setUndoRedoEnabled(oldUndoRedo);
        setUpdatesEnabled(true);
    }
};

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    clipboard_ = QGuiApplication::clipboard();
    setupUi();

    if (!ensureNickname()) {
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
    chat_.setNickname(nickname_.toStdString());

    // Démarre le backend réseau
    bool ok = chat_.start(
        [this](const std::string& fromName, const std::string& text) {
            QString qFrom = QString::fromStdString(fromName);
            QString qText = QString::fromStdString(text);

            // On repasse sur le thread GUI
            QMetaObject::invokeMethod(this, [this, qFrom, qText]() {
                lastReceived_ = qText;
                lastFullMessageContent_ = qText;
                appendReceivedMessage("[" + qFrom + "] a envoyé un message.");
            });
        }
    );

    if (!ok) {
        appendReceivedMessage("[Erreur] Impossible de démarrer le chat réseau.");
    }
}

MainWindow::~MainWindow() {
    chat_.stop();
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *title = new QLabel("ClipTransfer – Chat LAN (UDP broadcast)", this);
    title->setStyleSheet("font-weight: bold; font-size: 16px;");

    history_ = new QPlainTextEdit(this);
    history_->setReadOnly(true);
    history_->setLineWrapMode(QPlainTextEdit::NoWrap);
    history_->setMaximumBlockCount(20000);

    manualInput_ = new SafePlainTextEdit(this);
    manualInput_->setPlaceholderText("Écrire un message... Ctrl+Entrée pour envoyer le message");
    manualInput_->setTabChangesFocus(true);
    manualInput_->setLineWrapMode(QPlainTextEdit::NoWrap);

    btnSendClip_ = new QPushButton("Envoyer le presse-papiers", this);
    btnSendManual_ = new QPushButton("Envoyer le texte", this);
    btnCopyLast_ = new QPushButton("Copier message complet", this);
    btnClearHistory_ = new QPushButton("Effacer conversation", this);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(btnSendClip_);
    btnRow->addWidget(btnSendManual_);
    btnRow->addWidget(btnCopyLast_);
    btnRow->addWidget(btnClearHistory_);

    mainLayout->addWidget(title);
    mainLayout->addWidget(history_);
    mainLayout->addWidget(manualInput_);
    mainLayout->addLayout(btnRow);

    setCentralWidget(central);

    // Connexions
    connect(btnSendClip_,   &QPushButton::clicked,
            this,           &MainWindow::sendClipboard);
    connect(btnSendManual_, &QPushButton::clicked,
            this,           &MainWindow::sendManualMessage);
    connect(btnCopyLast_,   &QPushButton::clicked,
            this,           &MainWindow::copyLastReceived);

    connect(btnClearHistory_, &QPushButton::clicked,
            this,             &MainWindow::clearHistory);


    // Ctrl+Entrée / Ctrl+Enter => envoyer (Enter simple reste une nouvelle ligne)
    {
        auto *sendShortcut1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
        sendShortcut1->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sendShortcut1, &QShortcut::activated, this, &MainWindow::sendManualMessage);

        auto *sendShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this);
        sendShortcut2->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sendShortcut2, &QShortcut::activated, this, &MainWindow::sendManualMessage);
    }
}

QString MainWindow::truncateForDisplay(const QString& text) const {
    if (text.isEmpty()) return text;

    // 1) Limite en caractères (évite les énormes copies côté QTextDocument)
    QString limited = text;
    bool truncated = false;
    if (limited.size() > kMaxDisplayCharsPerMessage) {
        limited = limited.left(kMaxDisplayCharsPerMessage);
        truncated = true;
    }

    // 2) Limite en lignes
    const QStringList lines = limited.split('\n');
    if (lines.size() <= kMaxDisplayLinesPerMessage && !truncated) {
        return limited;
    }

    const int take = (lines.size() < kMaxDisplayLinesPerMessage)
        ? static_cast<int>(lines.size())
        : kMaxDisplayLinesPerMessage;
    QString out;
    out.reserve(limited.size());
    for (int i = 0; i < take; ++i) {
        if (i) out.append('\n');
        out.append(lines[i]);
    }

    out.append("\n… [message tronqué]");
    return out;
}

void MainWindow::appendReceivedMessage(const QString &line) {
    // Désactiver temporairement le rendu pour gagner du temps
    history_->setUpdatesEnabled(false);

    const bool wasAtBottom = (history_->verticalScrollBar()->value() >= history_->verticalScrollBar()->maximum());

    QTextCursor cursor = history_->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // beginEditBlock dit à Qt : "Ne recalcule rien tant que je n'ai pas fini"
    cursor.beginEditBlock();
    cursor.insertText(line + "\n");
    cursor.endEditBlock();

    history_->setUpdatesEnabled(true);

    historyAppendCount_ += 1;
    if (wasAtBottom && ((historyAppendCount_ % kAutoScrollEveryNAppends) == 0)) {
        history_->verticalScrollBar()->setValue(history_->verticalScrollBar()->maximum());
    }
}

void MainWindow::sendClipboard() {
    if (!clipboard_) return;
    QString text = clipboard_->text();
    if (text.isEmpty()) return;

    lastFullMessageContent_ = text;
    chat_.enqueueMessage(text.toStdString());
    appendReceivedMessage("[" + nickname_ + "] a envoyé un message.");
}

void MainWindow::sendManualMessage() {
    QString text = manualInput_->toPlainText();
    if (text.trimmed().isEmpty()) return;

    manualInput_->clear();
    lastFullMessageContent_ = text;
    chat_.enqueueMessage(text.toStdString());
    appendReceivedMessage("[" + nickname_ + "] a envoyé un message.");
}

void MainWindow::copyLastReceived() {
    if (!clipboard_) return;
    if (lastFullMessageContent_.isEmpty()) return;
    clipboard_->setText(lastFullMessageContent_);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    chat_.stop();
    QMainWindow::closeEvent(event);
}

bool MainWindow::ensureNickname() {
    QSettings settings;
    nickname_ = settings.value("user/nickname", "").toString().trimmed();

    while (nickname_.isEmpty()) {
        bool ok = false;
        QString value = QInputDialog::getText(
            this,
            "Choisir un pseudo",
            "Entrez votre pseudo (obligatoire) :",
            QLineEdit::Normal,
            "",
            &ok
        );

        if (!ok) {
            return false; // annulation -> quitter
        }

        value = value.trimmed();
        if (value.isEmpty()) {
            continue;
        }
        if (value.contains('|')) {
            QMessageBox::warning(this, "Pseudo invalide", "Le caractère '|' n'est pas autorisé.");
            continue;
        }

        nickname_ = value;
        settings.setValue("user/nickname", nickname_);
    }

    return true;
}

void MainWindow::clearHistory() {
    // GUI
    if (history_) {
        history_->clear();
    }
    lastReceived_.clear();
    lastFullMessageContent_.clear();

    // Backend
    chat_.clearHistory();
}