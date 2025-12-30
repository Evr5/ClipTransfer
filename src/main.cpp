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
    if (v == "es" || v.startsWith("es_") || v.startsWith("es-")) return "es";
    if (v == "pt" || v.startsWith("pt_") || v.startsWith("pt-")) return "pt";
    if (v == "de" || v.startsWith("de_") || v.startsWith("de-")) return "de";
    if (v == "nl" || v.startsWith("nl_") || v.startsWith("nl-")) return "nl";
    if (v == "zh" || v.startsWith("zh_") || v.startsWith("zh-")) return "zh";
    if (v == "ar" || v.startsWith("ar_") || v.startsWith("ar-")) return "ar";
    if (v == "pl" || v.startsWith("pl_") || v.startsWith("pl-")) return "pl";
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
    switch (sysLang) {
    case QLocale::French:
        return "fr";
    case QLocale::English:
        return "en";
    case QLocale::Spanish:
        return "es";
    case QLocale::Portuguese:
        return "pt";
    case QLocale::German:
        return "de";
    case QLocale::Dutch:
        return "nl";
    case QLocale::Chinese:
        return "zh";
    case QLocale::Arabic:
        return "ar";
    case QLocale::Polish:
        return "pl";
    default:
        return "en";
    }
}

void installTranslations(QApplication& app, const QString& lang) {
    // App translation (our strings)
    if (lang != "fr") {
        auto* appTranslator = new QTranslator(&app);

        const QString appDir = QCoreApplication::applicationDirPath();
        const QString qmPath = QDir(appDir).filePath(QStringLiteral("i18n/ClipTransfer_") + lang + QStringLiteral(".qm"));
        if (appTranslator->load(qmPath)) {
            QCoreApplication::installTranslator(appTranslator);
        } else {
            delete appTranslator;
        }
    }

    // Qt base translation (buttons like OK/Cancel for Qt dialogs)
    if (lang != "fr") {
        auto* qtTranslator = new QTranslator(&app);
        const QString qtTransPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

        QStringList candidates;
        candidates << (QStringLiteral("qtbase_") + lang);
        if (lang == "zh") {
            candidates << QStringLiteral("qtbase_zh_CN") << QStringLiteral("qtbase_zh_TW") << QStringLiteral("qtbase_zh");
        }
        if (lang == "pt") {
            candidates << QStringLiteral("qtbase_pt") << QStringLiteral("qtbase_pt_BR") << QStringLiteral("qtbase_pt_PT");
        }
        if (lang == "pl") {
            candidates << QStringLiteral("qtbase_pl");
        }

        bool loaded = false;
        for (const QString& base : candidates) {
            if (qtTranslator->load(base, qtTransPath)) {
                loaded = true;
                break;
            }
        }

        if (loaded) {
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