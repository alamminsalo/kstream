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

#include "networkmanager.h"
#include "../util/fileutils.h"
#include "../util/jsonparser.h"
#include "../util/m3u8parser.h"
#include <QNetworkConfiguration>
#include <QEventLoop>
#include <QSet>
#include <QUrlQuery>
#include "../model/settingsmanager.h"

NetworkManager *NetworkManager::singleton = 0;

NetworkManager::NetworkManager(QNetworkAccessManager *man) : QObject(man)
{
    operation = man;

    initReplayChat();

    //Select interface
    connectionOK = false;
    testNetworkInterface();

    //SSL errors handle (down the drain)
    connect(operation, &QNetworkAccessManager::sslErrors, this, &NetworkManager::handleSslErrors);

    //Handshake
    operation->connectToHost(TWITCH_API);

    //Set up offline poller
    offlinePoller.setInterval(2000);
    connect(&offlinePoller, &QTimer::timeout, this, &NetworkManager::testNetworkInterface);

    //Set up listening to access token changes
    connect(SettingsManager::getInstance(), &SettingsManager::accessTokenChanged, this, &NetworkManager::setAccessToken);
    setAccessToken(SettingsManager::getInstance()->accessToken());

    lastVodChatRequest = nullptr;
}

void NetworkManager::setAccessToken(const QString &accessToken)
{
    access_token = accessToken;
}

NetworkManager::~NetworkManager()
{
    offlinePoller.stop();
    qDebug() << "Destroyer: NetworkManager";
    //operation->deleteLater();
    teardownReplayChat();
}

void NetworkManager::initialize(QNetworkAccessManager *mgr)
{
    singleton = new NetworkManager(mgr);
}

void NetworkManager::testNetworkInterface()
{
    //Chooses a working network interface from interfaces list, if default configuration doesn't work

    QNetworkConfigurationManager conf;
    QString identifier;

    QEventLoop loop;
    connect(this, &NetworkManager::finishedConnectionTest, &loop, &QEventLoop::quit);

    //Test default configuration
    operation->setConfiguration(conf.defaultConfiguration());

    testConnection();
    loop.exec();

    if (connectionOK == true) {
        qDebug() << "Selected default network configuration";
        return;
    }

    else {
        qDebug() << "Failure on default configuration, attempt to choose another interaface..";

        foreach (const QNetworkInterface & interface, QNetworkInterface::allInterfaces())
        {
            if (!interface.isValid())
                continue;

//            qDebug() << "Identifier: " << interface.name();
//            qDebug() << "HW addr: " << interface.hardwareAddress();

            bool isUp = interface.flags().testFlag(QNetworkInterface::IsUp);
            bool isLoopback = interface.flags().testFlag(QNetworkInterface::IsLoopBack);
            bool isActive = interface.flags().testFlag(QNetworkInterface::IsRunning);
//            bool isPtP = interface.flags().testFlag(QNetworkInterface::IsPointToPoint);

//            qDebug() << "Properties: ";
//            qDebug() << (isUp ? "Is up" : "Is down");

//            if (isLoopback)
//                qDebug() << "Loopback";

//            qDebug() << (isActive ? "Active" : "Inactive");

//            if (isPtP)
//                qDebug() << "Is Point-to-Point";

//            qDebug() << "";

            if (isUp && isActive && !isLoopback) {
                identifier = interface.name();
                qDebug() << "Testing connection for interface " << identifier;
                operation->setConfiguration(conf.configurationFromIdentifier(identifier));

                testConnection();
                loop.exec();

                if (connectionOK == true) {
                    qDebug() << "Success!";
                    return;
                }

                else
                    qDebug() << "Failure, trying another interface...";
            }

        }
    }

    if (!connectionOK) {
        operation->setConfiguration(conf.defaultConfiguration());
        offlinePoller.start();
    }
}

bool NetworkManager::networkAccess() {
    return connectionOK;
}

void NetworkManager::testConnection()
{
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(KRAKEN_API));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::testConnectionReply);
}

void NetworkManager::testConnectionReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

//    if (reply->error() == QNetworkReply::NoError)
//        qDebug() << "Got response: " << reply->readAll();

    handleNetworkError(reply);

    emit finishedConnectionTest();
}

void NetworkManager::checkVersion()
{
    QNetworkRequest req;
    req.setRawHeader("User-Agent", "Orion");
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    req.setUrl(QUrl("https://api.github.com/repos/alamminsalo/orion/releases/latest"));

    QNetworkReply *reply = operation->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, this](){
        QPair<QString, QString> info = JsonParser::parseVersion(reply->readAll());
        emit versionCheckEnded(info.first, info.second);
        reply->deleteLater();
    });
}

/**
 * @brief NetworkManager::getStream
 * Gets single stream status. Usable for polling a channel's stream
 */
void NetworkManager::getStream(const quint64 channelId)
{
    QString url = KRAKEN_API + QString("/streams/%1").arg(channelId);
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::streamReply);
}

void NetworkManager::getStreams(const QString &url)
{
    //qDebug() << "GET: " << url;
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::allStreamsReply);
}

void NetworkManager::getGames(const quint32 &offset, const quint32 &limit)
{
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    QString url = KRAKEN_API;
    url += QString("/games/top?limit=%1").arg(limit)
            + QString("&offset=%1").arg(offset);
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::gamesReply);
}

void NetworkManager::searchChannels(const QString &query, const quint32 &offset, const quint32 &limit)
{
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    QString url = QString(KRAKEN_API)
            + QString("/search/channels?query=") + QUrl::toPercentEncoding(query)
            + QString("&offset=%1").arg(offset)
            + QString("&limit=%1").arg(limit);

    qDebug() << "requesting" << url;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::searchChannelsReply);
}

void NetworkManager::searchGames(const QString &query)
{
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    QString url = QString(KRAKEN_API)
            + QString("/search/games?query=") + QUrl::toPercentEncoding(query);

    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::searchGamesReply);
}

void NetworkManager::getFeaturedStreams()
{
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    QString url = QString(KRAKEN_API)
            + "/streams/featured?limit=25&offset=0";
    request.setUrl(QUrl(url));

    //qDebug() << url;

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::featuredStreamsReply);
}

void NetworkManager::getStreamsForGame(const QString &game, const quint32 &offset, const quint32 &limit)
{
    QNetworkRequest request;
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    QString url = QString(KRAKEN_API)
            + QString("/streams?game=") + QUrl::toPercentEncoding(game)
            + QString("&offset=%1").arg(offset)
            + QString("&limit=%1").arg(limit);
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::gameStreamsReply);
}

void NetworkManager::getChannelPlaybackStream(const QString &channelName)
{
    QString url = QString(TWITCH_API)
            + QString("/channels/%1").arg(channelName)
            + QString("/access_token");
    QNetworkRequest request;
    request.setRawHeader("Client-ID", getPrivateClientId().toUtf8());
    request.setUrl(QUrl(url));

    request.setAttribute(QNetworkRequest::User, LIVE);

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::streamExtractReply);
}

void NetworkManager::getBroadcasts(const quint64 channelId, quint32 offset, quint32 limit)
{
    QString url = QString(KRAKEN_API)
            + QString("/channels/%1/videos").arg(channelId)
            + QString("?offset=%1").arg(offset)
            + QString("&limit=%1").arg(limit);

    if (ONLY_BROADCASTS)
        url += "&broadcast_type=archive";

    //if (USE_HLS)
        //url += "&hls=true";

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::broadcastsReply);
}

void NetworkManager::getBroadcastPlaybackStream(const QString &vod)
{
    QString url = QString(TWITCH_API)
            + QString("/vods/%1").arg(vod)
            + QString("/access_token");
    QNetworkRequest request;
    request.setRawHeader("Client-ID", getPrivateClientId().toUtf8());
    request.setUrl(QUrl(url));

    request.setAttribute(QNetworkRequest::User, VOD);

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::streamExtractReply);
}

void NetworkManager::getUser()
{
    QString url = QString(KRAKEN_API) + "/user";
    QString auth = "OAuth " + access_token;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));
    request.setRawHeader(QString("Authorization").toUtf8(), auth.toUtf8());

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::userReply);
}

void NetworkManager::getUserFavourites(const quint64 userId, quint32 offset, quint32 limit)
{
    if (!userId)
        return;

    QString url = QString(KRAKEN_API) + "/users/" + QString::number(userId) + "/follows/channels"
            + QString("?offset=%1").arg(offset)
            + QString("&limit=%1").arg(limit);
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));
    request.setAttribute(QNetworkRequest::User, (int) (offset + limit));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::favouritesReply);
}

void NetworkManager::getEmoteSets(const QList<int> &emoteSetIDs) {
    QList<QString> emoteSetsIDsStr;
    for (auto id : emoteSetIDs) {
        emoteSetsIDsStr.append(QString::number(id));
    }

    QString url = QString(KRAKEN_API) + "/chat/emoticon_images"
        + QString("?emotesets=") + emoteSetsIDsStr.join(',');
    QString auth = "OAuth " + access_token;

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));
    request.setRawHeader(QString("Authorization").toUtf8(), auth.toUtf8());

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::emoteSetsReply);
}

void NetworkManager::loadChatterList(const QString channel) {
    qDebug() << "Loading viewer list for" << channel;
    const QString url = QString(TWITCH_TMI_USER_API) + channel + QString("/chatters");

    qDebug() << "Request" << url;

    QNetworkRequest request;
    request.setUrl(url);

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::chatterListReply);
}

void NetworkManager::getBlockedUserList(const quint64 userId, const quint32 offset, const quint32 limit) {
    qDebug() << "Loading blocked user list for user" << userId;
    const QString url = QString(KRAKEN_API) + QString("/users/") + QString::number(userId) + QString("/blocks?offset=" + QString::number(offset) + "&limit=" + QString::number(limit) );
    qDebug() << "Request" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(url);

    int nextOffset = offset + limit;
    request.setAttribute(QNetworkRequest::User, nextOffset);

    QString auth = "OAuth " + access_token;
    request.setRawHeader(QString("Authorization").toUtf8(), auth.toUtf8());

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::blockedUserListReply);
}

void NetworkManager::editUserBlock(const quint64 myUserId, const QString & blockUsername, const bool isBlock) {
    const QString url = QString(KRAKEN_API) + QString("/users?login=") + QUrl::toPercentEncoding(blockUsername);

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(url);

    request.setAttribute(QNetworkRequest::User, myUserId);
    request.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1), blockUsername);
    request.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2), isBlock);
    request.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 3), access_token);

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::blockUserLookupReply);
}

void NetworkManager::blockUserLookupReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }

    quint64 myUserId = reply->request().attribute(QNetworkRequest::User).toULongLong();
    QString blockUsername = reply->request().attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1)).toString();
    bool isBlock = reply->request().attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2)).toBool();
    //QString access_token = reply->request().attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 3)).toString();

    QByteArray data = reply->readAll();
    const auto & userIds = JsonParser::parseUsers(data);

    if (userIds.length() == 0 || userIds[0] == 0) {
        qDebug() << "userId lookup failed for" << blockUsername;
    }

    quint64 blockUserId = userIds[0];

    editUserBlockWithId(myUserId, blockUsername, blockUserId, isBlock);
}

void NetworkManager::editUserBlockWithId(const quint64 myUserId, const QString & blockUsername, const quint64 blockUserId, const bool isBlock) {
    qDebug() << "Setting block for user" << blockUserId << "to" << isBlock << "for user" << myUserId;
    const QString url = QString(KRAKEN_API) + QString("/users/") + QString::number(myUserId) + QString("/blocks/") + QString::number(blockUserId);
    qDebug() << "Request" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(url);

    request.setAttribute(QNetworkRequest::User, myUserId);
    request.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1), blockUsername);
    request.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2), isBlock);

    QString auth = "OAuth " + access_token;
    request.setRawHeader(QString("Authorization").toUtf8(), auth.toUtf8());

    QNetworkReply *reply;
    if (isBlock) {
        reply = operation->put(request, "");
    }
    else {
        reply = operation->deleteResource(request);
    }

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::blockUserReply);
}

void NetworkManager::blockUserReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401) {
            qWarning() << "Warning: Not authorized to edit blocked users list; logout and log in again to update OAuth scopes";
        }
        return;
    }

    quint64 myUserId = reply->request().attribute(QNetworkRequest::User).toULongLong();
    QString blockUsername = reply->request().attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1)).toString();
    bool isBlock = reply->request().attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2)).toBool();

    if (isBlock) {
        emit userBlocked(myUserId, blockUsername);
    }
    else {
        emit userUnblocked(myUserId, blockUsername);
    }
}

void NetworkManager::chatterListReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }

    QByteArray data = reply->readAll();

    //qDebug() << data;

    QMap<QString, QList<QString>> ret = JsonParser::parseChatterList(data);

    emit chatterListLoadOperationFinished(ret);

    reply->deleteLater();
}

void NetworkManager::blockedUserListReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401) {
            qWarning() << "Warning: Not authorized to read blocked users list; logout and log in again to update OAuth scopes";
        }
        return;
    }

    QByteArray data = reply->readAll();

    auto result = JsonParser::parseBlockList(data);

    int nextOffset = reply->request().attribute(QNetworkRequest::User).toInt();

    emit blockedUserListLoadOperationFinished(result.items, nextOffset, result.total);

    reply->deleteLater();
}

void NetworkManager::getVodChatPiece(quint64 vodId, quint64 offset) {
    QString url = QString(TWITCH_API_V5) + QString("/videos/%2/comments?content_offset_seconds=%1").arg(offset).arg(vodId);

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(url);

    QNetworkReply *reply = operation->get(request);

    lastVodChatRequest = reply;
    
    connect(reply, &QNetworkReply::finished, this, &NetworkManager::vodChatPieceReply);
}

void NetworkManager::getNextVodChatPiece(quint64 vodId, QString cursor) {
    QString url = QString(TWITCH_API_V5) + QString("/videos/%2/comments?cursor=%1").arg(cursor).arg(vodId);

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(url);

    QNetworkReply *reply = operation->get(request);

    lastVodChatRequest = reply;

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::vodChatPieceReply);
}

void NetworkManager::cancelLastVodChatRequest() {
    if (lastVodChatRequest != nullptr) {
        lastVodChatRequest->abort();
        lastVodChatRequest = nullptr;
    }
}

void NetworkManager::resetVodChat() {
    replayChatPartNum = 0;
    curChatReplayDedupeBatch->clear();
    prevChatReplayDedupeBatch->clear();
}

void NetworkManager::initReplayChat() {
    curChatReplayDedupeBatch = new QSet<QString>();
    prevChatReplayDedupeBatch = new QSet<QString>();
}

void NetworkManager::teardownReplayChat() {
    delete curChatReplayDedupeBatch;
    delete prevChatReplayDedupeBatch;
}

void NetworkManager::filterReplayChat(QList<ReplayChatMessage> & replayChat) {
    if (replayChatPartNum % REPLAY_CHAT_DEDUPE_SWAP_ITERATIONS == 0) {
        auto oldPrev = prevChatReplayDedupeBatch;
        prevChatReplayDedupeBatch = curChatReplayDedupeBatch;
        oldPrev->clear();
        curChatReplayDedupeBatch = oldPrev;
    }

    for (auto entry = replayChat.begin(); entry != replayChat.end(); ) {
        if (entry->deleted || curChatReplayDedupeBatch->contains(entry->id) || prevChatReplayDedupeBatch->contains(entry->id)) {
            qDebug() << "DUPE" << entry->from << ":" << entry->id << entry->message;
            entry = replayChat.erase(entry);
        }
        else {
            //qDebug() << "GOOD" << entry->from << ":" << entry->id << entry->message;
            curChatReplayDedupeBatch->insert(entry->id);
            entry++;
        }
    }

    replayChatPartNum++;
}

void NetworkManager::vodChatPieceReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (lastVodChatRequest == reply) {
        lastVodChatRequest = nullptr;
    }

    if (!handleNetworkError(reply)) {
        return;
    }

    QByteArray data = reply->readAll();

    //qDebug() << data;

    ReplayChatPiece ret = JsonParser::parseVodChatPiece(data);

    filterReplayChat(ret.comments);

    emit vodChatPieceGetOperationFinished(ret);

    reply->deleteLater();
}

const QString NetworkManager::CHANNEL_BADGES_URL_PREFIX = QString(KRAKEN_API) + "/chat/";
const QString NetworkManager::CHANNEL_BADGES_URL_SUFFIX = "/badges";

void NetworkManager::getChannelBadgeUrls(const quint64 channelId) {
    QString url = CHANNEL_BADGES_URL_PREFIX + QString::number(channelId) + CHANNEL_BADGES_URL_SUFFIX;
    QString auth = "OAuth " + access_token;

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));
    request.setRawHeader(QString("Authorization").toUtf8(), auth.toUtf8());

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::channelBadgeUrlsReply);
}

const QString NetworkManager::CHANNEL_BADGES_BETA_URL_PREFIX = "https://badges.twitch.tv/v1/badges/channels/";
const QString NetworkManager::CHANNEL_BADGES_BETA_URL_SUFFIX = "/display?language=en";
const QString NetworkManager::GLOBAL_BADGES_BETA_URL = "https://badges.twitch.tv/v1/badges/global/display?language=en";

void NetworkManager::getChannelBadgeUrlsBeta(const int channelID) {
    QString url = CHANNEL_BADGES_BETA_URL_PREFIX + QString::number(channelID) + CHANNEL_BADGES_BETA_URL_SUFFIX;

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::channelBadgeUrlsBetaReply);
}

void NetworkManager::getGlobalBadgesUrlsBeta() {
    QString url = GLOBAL_BADGES_BETA_URL;

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.twitchtv.v5+json");
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::globalBadgeUrlsBetaReply);
}

void NetworkManager::getChannelBitsUrls(const int channelID) {
    QString url = QString(KRAKEN_API) + QString("/bits/actions?channel_id=") + QString::number(channelID);

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setRawHeader("Accept", QString("application/vnd.twitchtv.v5+json").toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::channelBitsUrlsReply);
}

void NetworkManager::channelBitsUrlsReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    QString urlString = reply->url().toString();

    qDebug() << "url was" << urlString;

    int eqPos = urlString.lastIndexOf('=');

    if (eqPos != -1) {
        QString channelIDStr = urlString.mid(eqPos + 1);
        int channelID = channelIDStr.toInt();
        qDebug() << "bits urls for channel" << channelID << "loaded";
        BitsQStringsMap urls;
        BitsQStringsMap colors;
        JsonParser::parseBitsData(data, urls, colors);

        emit getChannelBitsUrlsOperationFinished(channelID, urls, colors);
    }
    else {
        qDebug() << "can't determine channel from request url";
    }

    reply->deleteLater();
}

void NetworkManager::getGlobalBitsUrls() {
    QString url = QString(KRAKEN_API) + QString("/bits/actions");

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setRawHeader("Accept", QString("application/vnd.twitchtv.v5+json").toUtf8());
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::globalBitsUrlsReply);
}

void NetworkManager::globalBitsUrlsReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    QString urlString = reply->url().toString();


    BitsQStringsMap urls;
    BitsQStringsMap colors;
    JsonParser::parseBitsData(data, urls, colors);

    emit getGlobalBitsUrlsOperationFinished(urls, colors);

    reply->deleteLater();
}

void NetworkManager::getChannelBttvEmotes(const QString channel) {
    QString url = QString(BTTV_API) + QString("/channels/") + QUrl::toPercentEncoding(channel);

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::channelBttvEmotesReply);
}

void NetworkManager::channelBttvEmotesReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    auto url = reply->url();
    QString urlString = url.toString();
    QString channel = urlString.mid(urlString.lastIndexOf("/") + 1);

    auto emotes = JsonParser::parseBttvEmotesData(data);

    emit getChannelBttvEmotesOperationFinished(channel, emotes);

    reply->deleteLater();
}

void NetworkManager::getGlobalBttvEmotes() {
    QString url = QString(BTTV_API) + QString("/emotes");

    qDebug() << "Requesting" << url;

    QNetworkRequest request;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::globalBttvEmotesReply);
}

void NetworkManager::globalBttvEmotesReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    auto emotes = JsonParser::parseBttvEmotesData(data);

    emit getGlobalBttvEmotesOperationFinished(emotes);

    reply->deleteLater();
}

void NetworkManager::editUserFavourite(const quint64 userId, const quint64 channelId, bool add)
{
    QString url = QString(KRAKEN_API) + "/users/" + QString::number(userId)
            + "/follows/channels/" + QString::number(channelId);

    QString auth = "OAuth " + access_token;

    QNetworkRequest request;
    request.setRawHeader("Accept", QString("application/vnd.twitchtv.v5+json").toUtf8());
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));
    request.setRawHeader(QString("Authorization").toUtf8(), auth.toUtf8());

    QNetworkReply *reply = 0;

    if (add)
        reply = operation->put(request, QByteArray());
    else
        reply = operation->deleteResource(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::editUserFavouritesReply);
}

QNetworkAccessManager *NetworkManager::getManager() const
{
    return operation;
}

void NetworkManager::getM3U8Data(const QString &url, M3U8TYPE type)
{
    QNetworkRequest request;
    request.setRawHeader("Client-ID", getClientId().toUtf8());
    request.setUrl(QUrl(url));

    request.setAttribute(QNetworkRequest::User, type);

    QNetworkReply *reply = operation->get(request);

    connect(reply, &QNetworkReply::finished, this, &NetworkManager::m3u8Reply);
}

bool NetworkManager::handleNetworkError(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError){

        if (reply->error() >= 1 && reply->error() <= 199) {

            if (connectionOK == true) {
                connectionOK = false;
                emit networkAccessChanged(false);
            }

            if (!offlinePoller.isActive())
                offlinePoller.start();
        }

        qDebug() << reply->errorString();

        return false;
    }

    if (!connectionOK) {
        connectionOK = true;
        emit networkAccessChanged(true);
    }

    if (offlinePoller.isActive())
        offlinePoller.stop();

    return true;
}

void NetworkManager::handleSslErrors(QNetworkReply * /*reply*/, QList<QSslError> errors)
{
    foreach (const QSslError & e, errors) {
        qDebug() << "Ssl error: " << e.errorString();
    }

    //reply->ignoreSslErrors(errors);
}

void NetworkManager::streamReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }

    QByteArray data = reply->readAll();

    //qDebug() << data;

    Channel *channel = JsonParser::parseStream(data);

    QString channelIdStr = reply->url().toString();
    channelIdStr.remove(0, channelIdStr.lastIndexOf('/') + 1);
    const quint64 channelId = channelIdStr.toULongLong();

    emit streamGetOperationFinished(channelId, channel->isOnline());

    channel->deleteLater();

    reply->deleteLater();
}

void addOfflineChannels(QList<Channel *> & channels, const QList<quint64> & expectedChannelIds) {
    if (channels.count() < expectedChannelIds.count()) {
        QSet<quint64> unseenChannelIds = expectedChannelIds.toSet();

        foreach(const Channel* channel, channels) {
            unseenChannelIds.remove(channel->getId());
        }
        foreach(const quint64 id, unseenChannelIds) {
            channels.append(new Channel(id));
        }
    }
}

template <class U>
void addULongLongStringList(U & modify, const QStringList & newItems) {
    modify.reserve(modify.length() + newItems.length());
    for (const QString & s : newItems) {
        modify.append(s.toULongLong());
    }
}

void NetworkManager::allStreamsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    QList<quint64> queriedChannelIds;

    const QUrlQuery query(reply->url().query());
    if (query.hasQueryItem("channel")) {
        addULongLongStringList(queriedChannelIds, query.queryItemValue("channel").split(","));
    }

    PagedResult<Channel *> out = JsonParser::parseStreams(data);

    addOfflineChannels(out.items, queriedChannelIds);

    emit allStreamsOperationFinished(out.items);

    reply->deleteLater();
}

void NetworkManager::searchGamesReply()
{

    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    emit searchGamesOperationFinished(JsonParser::parseGames(data));

    reply->deleteLater();
}

void NetworkManager::gamesReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    emit gamesOperationFinished(JsonParser::parseGames(data));

    reply->deleteLater();
}

void NetworkManager::gameStreamsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    //qDebug() << data;

    QList<quint64> queriedChannelIds;

    const QUrlQuery query(reply->url().query());
    if (query.hasQueryItem("channel")) {
        addULongLongStringList(queriedChannelIds, query.queryItemValue("channel").split(","));
    }

    PagedResult<Channel *> out = JsonParser::parseStreams(data);

    addOfflineChannels(out.items, queriedChannelIds);

    emit gameStreamsOperationFinished(out.items, out.total);

    reply->deleteLater();
}

void NetworkManager::featuredStreamsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    //qDebug() << data;

    QList<Channel *> channels = JsonParser::parseFeatured(data);
    emit featuredStreamsOperationFinished(channels, channels.count());

    reply->deleteLater();
}

void NetworkManager::searchChannelsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    //qDebug() << data;

    auto result = JsonParser::parseChannels(data);
    emit searchChannelsOperationFinished(result.items, result.total);

    reply->deleteLater();
}

void NetworkManager::streamExtractReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        emit error("token_error");
        return;
    }

    QByteArray data = reply->readAll();
    //qDebug() << data;

    M3U8TYPE type = (M3U8TYPE) reply->request().attribute(QNetworkRequest::User).toInt();

    QString url;

    switch (type) {
    case LIVE:
        url = JsonParser::parseChannelStreamExtractionInfo(data);
        break;

    case VOD:
        url = JsonParser::parseVodExtractionInfo(data);
        break;
    }

    getM3U8Data(url, type);

    reply->deleteLater();
}

void NetworkManager::m3u8Reply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {

        emit error("playlist_error");

        return;
    }

    QByteArray data = reply->readAll();

    switch ((M3U8TYPE) reply->request().attribute(QNetworkRequest::User).toInt()) {
    case LIVE:
        emit m3u8OperationFinished(m3u8::getUrls(data));
        break;

    case VOD:
        emit m3u8OperationBFinished(m3u8::getUrls(data));
        break;
    }
    //qDebug() << data;

    reply->deleteLater();
}

void NetworkManager::broadcastsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }

    QByteArray data = reply->readAll();

    emit broadcastsOperationFinished(JsonParser::parseVods(data));

    reply->deleteLater();
}

void NetworkManager::favouritesReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }

    QByteArray data = reply->readAll();

    int offset = reply->request().attribute(QNetworkRequest::User).toInt();

    auto result = JsonParser::parseFavourites(data);
    emit favouritesReplyFinished(result.items, offset, result.total);

    reply->deleteLater();
}

void NetworkManager::editUserFavouritesReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }

    //Nothing to do
    emit userEditFollowsOperationFinished();

    reply->deleteLater();
}

void NetworkManager::userReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    auto pair = JsonParser::parseUser(data);
    emit userOperationFinished(pair.first, pair.second);

    reply->deleteLater();
}

void NetworkManager::emoteSetsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();
    
    emit getEmoteSetsOperationFinished(JsonParser::parseEmoteSets(data));

    reply->deleteLater();
}

void NetworkManager::channelBadgeUrlsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    QString urlString = reply->url().toString();

    qDebug() << "url was" << urlString;

    if (urlString.startsWith(CHANNEL_BADGES_URL_PREFIX) && urlString.endsWith(CHANNEL_BADGES_URL_SUFFIX)) {
        quint64 channelId = urlString.mid(CHANNEL_BADGES_URL_PREFIX.length(), urlString.length() - CHANNEL_BADGES_URL_PREFIX.length() - CHANNEL_BADGES_URL_SUFFIX.length()).toULongLong();
        qDebug() << "badges for channel" << channelId << "loaded";
        auto badges = JsonParser::parseChannelBadgeUrls(data);

        emit getChannelBadgeUrlsOperationFinished(channelId, badges);
    }
    else {
        qDebug() << "can't determine channel from badges request url";
    }


    reply->deleteLater();
}

void NetworkManager::channelBadgeUrlsBetaReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    QString urlString = reply->url().toString();

    qDebug() << "url was" << urlString;

    if (urlString.startsWith(CHANNEL_BADGES_BETA_URL_PREFIX) && urlString.endsWith(CHANNEL_BADGES_BETA_URL_SUFFIX)) {
        QString channelIDStr = urlString.mid(CHANNEL_BADGES_BETA_URL_PREFIX.length(), urlString.length() - CHANNEL_BADGES_BETA_URL_PREFIX.length() - CHANNEL_BADGES_BETA_URL_SUFFIX.length());
        int channelID = channelIDStr.toInt();
        qDebug() << "beta badges for channel" << channelID << "loaded";
        auto badges = JsonParser::parseBadgeUrlsBetaFormat(data);

        emit getChannelBadgeBetaUrlsOperationFinished(channelID, badges);
    }
    else {
        qDebug() << "can't determine channel from badges request url";
    }

    reply->deleteLater();
}

void NetworkManager::globalBadgeUrlsBetaReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());

    if (!handleNetworkError(reply)) {
        return;
    }
    QByteArray data = reply->readAll();

    QString urlString = reply->url().toString();

    qDebug() << "url was" << urlString;

    qDebug() << "global beta badges loaded";
    auto badges = JsonParser::parseBadgeUrlsBetaFormat(data);

    emit getGlobalBadgeBetaUrlsOperationFinished(badges);

    reply->deleteLater();
}

