/*
 * Copyright © 2015-2016 Antti Lamminsalo
 *
 * This file is part of Orion.
 *
 * Orion is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with Orion.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "channel.h"
#include "channellistmodel.h"
#include "gamelistmodel.h"
#include "game.h"
#include "../network/networkmanager.h"
#include "settingsmanager.h"

#include <QSettings>
#include <QSortFilterProxyModel>

#include "singletonprovider.h"

#define DEFAULT_LOGO_URL    "http://static-cdn.jtvnw.net/jtv_user_pictures/xarth/404_user_150x150.png"

class ChannelManager: public QObject
{
    QML_SINGLETON
    Q_OBJECT

    NetworkManager* netman;
    SettingsManager *settingsManager;

    ChannelListModel* tempFavourites;
    ChannelListModel* favouritesModel;
    QSortFilterProxyModel* favouritesProxy;
    ChannelListModel* resultsModel;

    //Games (and game search results)
    GameListModel* gamesModel;

    //Oauth
    QString user_name;
    quint64 user_id;

    /**
     * @brief createFollowedChannelsModel
     * Creates ready-to-use followed channels model
     * @return
     */
    ChannelListModel *createFollowedChannelsModel();
    bool isAccessTokenAvailable() { return settingsManager->hasAccessToken(); }

    ChannelManager();

public:
    static ChannelManager *getInstance();

    ~ChannelManager();

    void load();
    void save();

    Q_INVOKABLE bool containsFavourite(const quint32&);

    void checkStreams(const QList<Channel*>&);

    ChannelListModel *getFavouritesModel() const;

    QSortFilterProxyModel *getFavouritesProxy() const;

    ChannelListModel *getResultsModel() const;
    GameListModel *getGamesModel() const;

    quint64 getUser_id() const;

signals:
    void pushNotification(const QString &title, const QString &message, const QString &imgUrl);
    void resultsUpdated(int numAdded, int total);
    void searchingStarted();
    void foundPlaybackStream(const QVariantMap &streams);
    void deletedChannel(const quint32 &chanid);
    void addedChannel(const quint32 &chanid);
    void gamesSearchStarted();
    void gamesUpdated();
    void followedUpdated();

    //oauth methods
    void accessTokenUpdated();
    void userNameUpdated(const QString name);
    void login(const QString &username, const QString &password);

public slots:
    void checkFavourites();
    void addToFavourites(const quint32&);
    void removeFromFavourites(const quint32&);
    void searchChannels(QString, const quint32&, const quint32&, bool);
    void notify(Channel*);
    void notifyMultipleChannelsOnline(const QList<Channel*> &);
    void findPlaybackStream(const QString&);
    void addToFavourites(const quint32 &id, const QString &serviceName, const QString &title,
                                     const  QString &info, const QString &logo, const QString &preview,
                                     const QString& game, const qint32 &viewers, bool online);
    void getFollowedChannels(const quint32 &limit = FOLLOWED_FETCH_LIMIT, const quint32 &offset = 0);
    void searchGames(QString, const quint32&, const quint32&);
    QString username() const;

private slots:
    void addSearchResults(const QList<Channel*>&, const int total);
    void updateFavourites(const QList<Channel*>&);
    void updateStreams(const QList<Channel*>&);
    void addGames(const QList<Game*>&);
    void onUserUpdated(const QString &name, const quint64 userId);
    void addFollowedResults(const QList<Channel*>&, const quint32, const quint32);
    void slotNetworkAccessChanged(bool);
    void updateAccessToken(QString accessToken);

    // QObject interface
protected:
    void timerEvent(QTimerEvent *event);
};
