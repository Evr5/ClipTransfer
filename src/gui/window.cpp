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

    splitter_ = new QSplitter(Qt::Vertical, this);
    splitter_->setChildrenCollapsible(false);
    splitter_->addWidget(history_);
    splitter_->addWidget(manualInput_);

    // Au départ: la saisie prend plus de place
    splitter_->setSizes({100, 380});

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
    mainLayout->addWidget(splitter_);
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

void MainWindow::growHistoryAreaStep() {
    if (!splitter_) return;
    const QList<int> sizes = splitter_->sizes();
    if (sizes.size() != 2) return;

    const int historyPx = sizes[0];
    const int inputPx = sizes[1];
    const int total = historyPx + inputPx;
    if (total <= 0) return;

    const int half = total / 2;
    if (historyPx >= half) return;

    const int newHistory = (historyPx + kHistoryGrowStepPx > half) ? half : (historyPx + kHistoryGrowStepPx);
    splitter_->setSizes({newHistory, total - newHistory});
}

void MainWindow::appendReceivedMessage(const QString &line) {
    // Désactiver temporairement le rendu pour gagner du temps
    history_->setUpdatesEnabled(false);

    QTextCursor cursor = history_->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // beginEditBlock dit à Qt : "Ne recalcule rien tant que je n'ai pas fini"
    cursor.beginEditBlock();
    cursor.insertText(line + "\n");
    cursor.endEditBlock();

    history_->setUpdatesEnabled(true);

    // Fait grandir progressivement l'historique jusqu'à 50/50
    growHistoryAreaStep();

    // Toujours défiler en bas pour afficher la dernière notif
    history_->verticalScrollBar()->setValue(history_->verticalScrollBar()->maximum());
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
    lastFullMessageContent_.clear();

    if (splitter_) {
        const QList<int> sizes = splitter_->sizes();
        if (sizes.size() == 2) {
            const int total = sizes[0] + sizes[1];
            const int historySmall = (total > 0) ? std::max(80, total / 5) : 120;
            splitter_->setSizes({historySmall, std::max(80, total - historySmall)});
        } else {
            splitter_->setSizes({120, 360});
        }
    }

    // Backend
    chat_.clearHistory();
}