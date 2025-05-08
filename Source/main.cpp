#include <QApplication>
#include "ChatWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    bool isServer = false;
    QString host = "127.0.0.1";
    quint16 port = 1234;

    if (argc > 1 && QString(argv[1]) == "server")
        isServer = true;

    ChatWindow window( isServer, host, port );
    window.show();

    return app.exec();
}
