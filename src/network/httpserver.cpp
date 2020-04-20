#include "httpserver.h"

HttpServer::HttpServer(QObject *parent): QObject(parent)
{

}

HttpServer *HttpServer::getInstance() {
    static HttpServer *instance = new HttpServer();
    return instance;
}

QString HttpServer::port() {
    return m_port;
}

bool HttpServer::isOk() const
{
    return !listenError;
}

void HttpServer::start() {
    if (server) {
        stop();
    }

    server = new QTcpServer(this);

    /// IMPORTANT!
    quint16 port = 8979;

    connect(server, &QTcpServer::newConnection, this, &HttpServer::onConnect);
    if (!server->listen(QHostAddress::LocalHost, port)) {
        emit error();
    }

    m_port = QString::number(port);

    qDebug() << "listening port" << m_port;
}

void HttpServer::stop() {
    if (server) {
        qDebug() << "Stopping server";
        server->deleteLater();
        server = 0;
    }
}

void HttpServer::onConnect() {
    qDebug()<<"Connected";
    QTcpSocket *socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &HttpServer::onRead);
}

void HttpServer::onRead() {
    qDebug() << "Reading request...";
    QTcpSocket *socket = (QTcpSocket*) this->sender();
    socket->connect(socket, &QTcpSocket::disconnected, &QObject::deleteLater);

    /// Read data
    QString code;
    QStringList tokens = QString(socket->readAll()).split(QRegExp("[ \r\n][ \r\n]*"));
    if (tokens[0] == "GET") {
        if (tokens.length() >= 1) {
            QString params = tokens[1];

            //params to map
            QMap<QString,QString> map;
            params = params.mid(params.indexOf("?")+1);

            foreach (const QString & s, params.split("&")) {
                QStringList pair = s.split("=");
                if (pair.length() == 2){
                    map.insert(pair[0], pair[1]);
                }
            }

            if (map.contains("access_token")) {
                //Code found
                qDebug() << "Found code!";
                code = map["access_token"];
            }
        }
    }

    // Respond with 200
    QByteArray block;

    // http payload message body
    QByteArray content;
    if (code.isEmpty()) {
        content = "<!DOCTYPE html><html><script>"
                  "var uri = '' + window.location.href;"
                  "window.location.href = uri.replace('#','?');"
                  "</script><body></body></html>";
    }

    else {
        content = "<!DOCTYPE html><html>"
                  "<body><h1>Success!</h1><p>You can close this page now</p></body></html>";
    }

    QString response = "HTTP/1.1 200 OK\n";
    response += "Content-Type: text/html; charset=utf-8\n";
    response += "Connection: Closed\n";
    response += "Content-Length: " + QString::number(content.length()) + "\n";
    response += "\n" + content;

    socket->write(response.toUtf8());
    socket->waitForBytesWritten();
    socket->disconnectFromHost();

    // Check if we have the api code ready
    if (!code.isEmpty()) {
        qDebug() << "Got code" << code;
        emit codeReceived(code);

        // Spin down server
        stop();
    }
}
