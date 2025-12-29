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
#include <QFileDialog>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    clipboard_ = QGuiApplication::clipboard();
    setupUi();

    if (!ensureNickname()) {
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
    chat_.setNickname(nickname_);

    // Connexion : on formate le message reçu avant de l'afficher
    connect(&chat_, &ChatBackend::newMessage, this, [this](const QString &from, const QString &text) {
        lastReceived_ = text;
        appendReceivedMessage("[" + from + "] : " + text);
    });

    if (!chat_.start()) {
        appendReceivedMessage("[Erreur] Impossible de démarrer le serveur.");
    }
}

MainWindow::~MainWindow() {
    chat_.stop();
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *title = new QLabel("ClipTransfer – LAN Chat", this);
    title->setStyleSheet("font-weight: bold; font-size: 16px;");

    history_ = new QTextEdit(this);
    history_->setReadOnly(true);

    manualInput_ = new QLineEdit(this);
    manualInput_->setPlaceholderText("Écrire un message...");

    btnSendClip_ = new QPushButton("Envoyer Presse-papiers", this);
    btnSendManual_ = new QPushButton("Envoyer Texte", this);
    btnCopyLast_ = new QPushButton("Copier dernier", this);
    auto *btnFile = new QPushButton("Envoyer Fichier", this);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(btnSendClip_);
    btnRow->addWidget(btnSendManual_);
    btnRow->addWidget(btnCopyLast_);
    btnRow->addWidget(btnFile);

    mainLayout->addWidget(title);
    mainLayout->addWidget(history_);
    mainLayout->addWidget(manualInput_);
    mainLayout->addLayout(btnRow);

    setCentralWidget(central);

    connect(btnSendClip_,   &QPushButton::clicked, this, &MainWindow::sendClipboard);
    connect(btnSendManual_, &QPushButton::clicked, this, &MainWindow::sendManualMessage);
    connect(btnCopyLast_,   &QPushButton::clicked, this, &MainWindow::copyLastReceived);
    connect(manualInput_,   &QLineEdit::returnPressed, this, &MainWindow::sendManualMessage);

    connect(btnFile, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Sélectionner un fichier");
        if (!path.isEmpty()) {
            chat_.sendFile(path);
            appendReceivedMessage("[Moi] Envoi de : " + QFileInfo(path).fileName());
        }
    });
}

void MainWindow::sendClipboard() {
    if (!clipboard_) return;
    QString text = clipboard_->text();
    if (text.isEmpty()) return;
    chat_.sendMessage(text);
    appendReceivedMessage("[Moi] : " + text);
}

void MainWindow::sendManualMessage() {
    QString text = manualInput_->text();
    if (text.isEmpty()) return;
    manualInput_->clear();
    chat_.sendMessage(text);
    appendReceivedMessage("[Moi] : " + text);
}

void MainWindow::appendReceivedMessage(const QString &line) {
    history_->append(line);
    history_->verticalScrollBar()->setValue(history_->verticalScrollBar()->maximum());
}

void MainWindow::copyLastReceived() {
    if (clipboard_ && !lastReceived_.isEmpty()) {
        clipboard_->setText(lastReceived_);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    chat_.stop();
    QMainWindow::closeEvent(event);
}

bool MainWindow::ensureNickname() {
    QSettings settings;
    nickname_ = settings.value("user/nickname", "").toString().trimmed();
    if (!nickname_.isEmpty()) return true;

    bool ok;
    QString val = QInputDialog::getText(this, "Pseudo", "Entrez votre pseudo :", QLineEdit::Normal, "", &ok);
    if (ok && !val.trimmed().isEmpty()) {
        nickname_ = val.trimmed();
        settings.setValue("user/nickname", nickname_);
        return true;
    }
    return false;
}