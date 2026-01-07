#include <QGuiApplication>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "chat_controller.h"

namespace {
QString chooseLanguage() {
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

void installTranslations(QGuiApplication& app, const QString& lang) {
    if (lang != "fr") {
        auto* appTranslator = new QTranslator(&app);

        const QString qmPath = QStringLiteral(":/i18n/ClipTransfer_") + lang + QStringLiteral(".qm");
        if (appTranslator->load(qmPath)) {
            QCoreApplication::installTranslator(appTranslator);
        } else {
            delete appTranslator;
        }
    }

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
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ClipTransfer");
    QCoreApplication::setApplicationName("ClipTransfer");

    const QString lang = chooseLanguage();
    installTranslations(app, lang);

    ChatController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("chatController", &controller);

    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );
    engine.load(url);

    return app.exec();
}
