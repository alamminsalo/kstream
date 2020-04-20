#ifndef VIEWERSMODEL_H
#define VIEWERSMODEL_H

#include <QObject>
#include "singletonprovider.h"
#include "../network/networkmanager.h"

class ViewersModel : public QObject
{
    QML_SINGLETON
    Q_OBJECT

    NetworkManager *netman;
    static ViewersModel *instance;

    explicit ViewersModel(QObject *parent = nullptr);

public:
    static ViewersModel *getInstance();

signals:
    void chatterListLoaded(QVariantMap chatters);

public slots:
    void loadChatterList(const QString channel);
    void processChatterList(QMap<QString, QList<QString>> chatters);

};

#endif // VIEWERSMODEL_H
