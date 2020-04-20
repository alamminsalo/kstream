#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QDataStream>
#include <QMap>
#include <QDebug>

#include "../model/singletonprovider.h"

class HttpServer: public QObject
{
    QML_SINGLETON
    Q_OBJECT

    QTcpServer *server = 0;

    bool listenError = false;
    QString m_port;

    explicit HttpServer(QObject *parent = 0);
public:
    static HttpServer *getInstance();

    Q_INVOKABLE QString port();

    bool isOk() const;

public slots:
    // starts server
    void start();

    // destroys server
    void stop();

    void onConnect();

    void onRead();

signals:
    void codeReceived(QString code);
    void error();
};

#endif // HTTPSERVER_H
