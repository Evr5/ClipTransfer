#include <QApplication>
#include <QStyleFactory>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#include "gui/window.h"

namespace {

QString normalizeLanguageTag(const QString& raw) {
    const QString v = raw.trimmed().toLower();
    if (v == "fr" || v.startsWith("fr_") || v.startsWith("fr-")) return "fr";
    if (v == "en" || v.startsWith("en_") || v.startsWith("en-")) return "en";
    return {};
}

QString chooseLanguage(const QCommandLineParser& parser) {
    // 1) CLI
    if (parser.isSet("lang")) {
        const QString cli = normalizeLanguageTag(parser.value("lang"));
        if (!cli.isEmpty()) return cli;
    }

    // 2) Persisted setting
    {
        QSettings settings;
        const QString saved = normalizeLanguageTag(settings.value("ui/language").toString());
        if (!saved.isEmpty()) return saved;
    }

    // 3) System locale
    const QLocale::Language sysLang = QLocale::system().language();
    return (sysLang == QLocale::French) ? "fr" : "en";
}

void installTranslations(QApplication& app, const QString& lang) {
    // App translation (our strings)
    if (lang == "en") {
        auto* appTranslator = new QTranslator(&app);

        const QString appDir = QCoreApplication::applicationDirPath();
        const QString qmPath = QDir(appDir).filePath("i18n/ClipTransfer_en.qm");
        if (appTranslator->load(qmPath)) {
            QCoreApplication::installTranslator(appTranslator);
        } else {
            delete appTranslator;
        }
    }

    // Qt base translation (buttons like OK/Cancel for Qt dialogs)
    if (lang != "fr") {
        auto* qtTranslator = new QTranslator(&app);
        const QString qtBase = QStringLiteral("qtbase_") + lang;
        const QString qtTransPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        if (qtTranslator->load(qtBase, qtTransPath)) {
            QCoreApplication::installTranslator(qtTranslator);
        } else {
            delete qtTranslator;
        }
    }
}

}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ClipTransfer");
    QCoreApplication::setApplicationName("ClipTransfer");

    QCommandLineParser parser;
    parser.setApplicationDescription("ClipTransfer");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        {"l", "lang"},
        "Force la langue de l'interface (fr|en). Peut aussi être enregistré dans les paramètres.",
        "lang"
    ));
    parser.process(app);

    const QString lang = chooseLanguage(parser);
    installTranslations(app, lang);

    // Si l'utilisateur force une langue via CLI, on la persiste.
    if (parser.isSet("lang")) {
        const QString forced = normalizeLanguageTag(parser.value("lang"));
        if (!forced.isEmpty()) {
            QSettings settings;
            settings.setValue("ui/language", forced);
        }
    }

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 36));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(24, 24, 28));
    palette.setColor(QPalette::AlternateBase, QColor(36, 36, 42));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(45, 45, 55));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor(80, 80, 160));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    MainWindow w;
    w.setWindowTitle("ClipTransfer");
    w.resize(720, 720);
    w.show();
    return app.exec();
}