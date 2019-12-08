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

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QNetworkConfigurationManager>
#include <QNetworkInterface>
//#include <QtWebKitWidgets/QtWebKitWidgets>
#include <QUrl>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QTimer>
#include "urls.h"

#include "../model/channel.h"
#include "../model/game.h"
#include "../model/vod.h"

#include "replaychat.h"
#include "../model/singletonprovider.h"

#define ONLY_BROADCASTS true
#define USE_HLS true
#define FOLLOWED_FETCH_LIMIT 25

// Map of action prefix -> bits number str -> url
typedef QMap<QString, QMap<QString, QString>> BitsQStringsMap;

class NetworkManager: public QObject
{
    QML_SINGLETON
    Q_OBJECT

    Q_PROPERTY(bool up READ networkAccess NOTIFY networkAccessChanged)

    enum M3U8TYPE {
        LIVE = QNetworkRequest::CustomVerbAttribute + 1,
        VOD
    };

    void getM3U8Data(const QString&, M3U8TYPE type);
    bool handleNetworkError(QNetworkReply *error);

    static NetworkManager *singleton;

    NetworkManager(QNetworkAccessManager *mgr);

    QString access_token;

public:
    ~NetworkManager();

    static void initialize(QNetworkAccessManager *);

    static NetworkManager *getInstance() {
        return singleton;
    }

    Q_INVOKABLE void getStream(const quint64);
    void getStreams(const QString&);
    void getGames(const quint32&, const quint32&);
    void searchChannels(const QString&, const quint32&, const quint32&);
    void searchGames(const QString&);
    void getFeaturedStreams();
    void getStreamsForGame(const QString&, const quint32&, const quint32&);
    void getChannelPlaybackStream(const QString&);
    void getBroadcasts(const quint64 channelId, quint32 offset, quint32 limit);
    void getBroadcastPlaybackStream(const QString &vod);

    //Methods using oauth
    void getUser();
    void getUserFavourites(const quint64 userId, quint32 offset, quint32 limit);
    void editUserFavourite(const quint64 userId, const quint64 channelId, bool add);
    void getEmoteSets(const QList<int> &emoteSetIDs);
    void getChannelBadgeUrls(const quint64 channelId);
    void getChannelBadgeUrlsBeta(const int channelID);
    void getGlobalBadgesUrlsBeta();
    void getChannelBitsUrls(const int channelID);
    void getGlobalBitsUrls();
    void getChannelBttvEmotes(const QString channel);
    void getGlobalBttvEmotes();

    Q_INVOKABLE void getVodChatPiece(quint64 vodId, quint64 offset);
    Q_INVOKABLE void getNextVodChatPiece(quint64 vodId, QString cursor);
    Q_INVOKABLE void cancelLastVodChatRequest();
    Q_INVOKABLE void resetVodChat();
    Q_INVOKABLE void loadChatterList(const QString channel);
    void getBlockedUserList(const quint64 userId, const quint32 offset, const quint32 limit);
    void editUserBlock(const quint64 myUserId, const QString & blockUserName, const bool isBlock);

    QNetworkAccessManager *getManager() const;

    //TODO: move to new class if more operations need to be added
    Q_INVOKABLE QString getClientId() const { return QString(CLIENT_ID); }
    Q_INVOKABLE QString getPrivateClientId() const { return QString(CLIENT_ID_PRIVATE); }
    Q_INVOKABLE bool networkAccess();

signals:
    void finishedConnectionTest();

    void allStreamsOperationFinished(const QList<Channel *>&);
    void gamesOperationFinished(const QList<Game *>&);
    void gameStreamsOperationFinished(const QList<Channel *>&, const int total);
    void featuredStreamsOperationFinished(const QList<Channel *>&, const int total);
    void searchChannelsOperationFinished(const QList<Channel *>&, const int total);
    void searchGamesOperationFinished(const QList<Game *>&);
    void broadcastsOperationFinished(const QList<Vod *>&);
    void m3u8OperationFinished(const QVariantMap&);
    void m3u8OperationBFinished(const QVariantMap&);
    void fileOperationFinished(const QByteArray&);
    void favouritesReplyFinished(const QList<Channel *>&, const quint32, const quint32);
    void streamGetOperationFinished(const quint64 channelId, const bool online);
    void error(const QString &error);

    //oauth
    void userOperationFinished(const QString&, const quint64);
    void userEditFollowsOperationFinished();
    void getEmoteSetsOperationFinished(const QMap<int, QMap<int, QString>>);
    void getChannelBadgeUrlsOperationFinished(const quint64, const QMap<QString, QMap<QString, QString>>);
    void getChannelBadgeBetaUrlsOperationFinished(const int, const QMap<QString, QMap<QString, QMap<QString, QString>>>);
    void getGlobalBadgeBetaUrlsOperationFinished(const QMap<QString, QMap<QString, QMap<QString, QString>>>);

    void vodChatPieceGetOperationFinished(ReplayChatPiece);
    void chatterListLoadOperationFinished(QMap<QString, QList<QString>>);
    void blockedUserListLoadOperationFinished(QList<QString>, const quint32 nextOffset, const quint32 total);

    void getChannelBitsUrlsOperationFinished(int channelID, BitsQStringsMap channelBitsUrls, BitsQStringsMap channelBitsColors);
    void getGlobalBitsUrlsOperationFinished(BitsQStringsMap globalBitsUrls, BitsQStringsMap globalBitsColors);

    void getChannelBttvEmotesOperationFinished(const QString channel, QMap<QString, QString> & emotesByCode);
    void getGlobalBttvEmotesOperationFinished(QMap<QString, QString> & emotesByCode);

    void networkAccessChanged(bool up);

    void userBlocked(quint64 myUserId, const QString & blockedUsername);
    void userUnblocked(quint64 myUserId, const QString & unblockedUsername);

    void versionCheckEnded(QString version, QString url);

public slots:
    void checkVersion();

private slots:
    void testNetworkInterface();
    void testConnection();
    void testConnectionReply();
    void handleSslErrors(QNetworkReply * reply, QList<QSslError> errors);
    void allStreamsReply();
    void gamesReply();
    void gameStreamsReply();
    void featuredStreamsReply();
    void searchChannelsReply();
    void searchGamesReply();
    void streamExtractReply();
    void m3u8Reply();
    void broadcastsReply();
    void favouritesReply();
    void editUserFavouritesReply();
    void streamReply();
    void vodChatPieceReply();
    void chatterListReply();
    void blockedUserListReply();
    void blockUserReply();

    //Oauth slots
    void userReply();
    void emoteSetsReply();
    void channelBadgeUrlsReply();
    void channelBadgeUrlsBetaReply();
    void globalBadgeUrlsBetaReply();
    void channelBitsUrlsReply();
    void globalBitsUrlsReply();
    void blockUserLookupReply();
    void globalBttvEmotesReply();
    void channelBttvEmotesReply();

    void setAccessToken(const QString &accessToken);

private:
    static const QString CHANNEL_BADGES_URL_PREFIX;
    static const QString CHANNEL_BADGES_URL_SUFFIX;
    static const QString CHANNEL_BADGES_BETA_URL_PREFIX;
    static const QString CHANNEL_BADGES_BETA_URL_SUFFIX;
    static const QString GLOBAL_BADGES_BETA_URL;

    QNetworkAccessManager *operation;
    bool connectionOK;
    QTimer offlinePoller;

    const int REPLAY_CHAT_DEDUPE_SWAP_ITERATIONS = 5;
    int replayChatPartNum = 0;

    QSet<QString> * curChatReplayDedupeBatch;
    QSet<QString> * prevChatReplayDedupeBatch;

    void initReplayChat();
    void teardownReplayChat();
    void filterReplayChat(QList<ReplayChatMessage> & replayChat);

    QNetworkReply *lastVodChatRequest;

    void editUserBlockWithId(const quint64 myUserId, const QString & blockUsername, const quint64 blockUserId, const bool isBlock);
};

#endif // NETWORKMANAGER_H
