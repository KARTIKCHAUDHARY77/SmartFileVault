#include "dashboard_controller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QGuiApplication::setApplicationName("SmartFileVault");
    QGuiApplication::setOrganizationName("SmartFileVault");

    QQuickStyle::setStyle("Basic");

    DashboardController dashboard;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("dashboard", &dashboard);
    engine.load(QUrl("qrc:/SmartFileVault/desktop/Main.qml"));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
