#pragma once

#include <QObject>
#include "../model/singletonprovider.h"

#ifdef Q_OS_WIN
    #include <Windows.h>
#endif

class Power: public QObject
{
    QML_SINGLETON
    Q_OBJECT

    Q_PROPERTY(bool screensaver WRITE setScreensaver)

    Power();

public:
    static Power *getInstance();
    ~Power();

    Q_INVOKABLE void setScreensaver(bool);

private:
    quint32 cookie;

    // QObject interface
protected:
    void timerEvent(QTimerEvent *event);
};

