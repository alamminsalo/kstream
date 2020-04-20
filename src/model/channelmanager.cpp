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

#include "channelmanager.h"
#include <QCoreApplication>

ChannelManager::ChannelManager() :
    netman(NetworkManager::getInstance()),
    settingsManager(SettingsManager::getInstance())
{
    user_id = 0;
    tempFavourites = 0;

    resultsModel = new ChannelListModel();
    gamesModel = new GameListModel();

    //Setup followed channels model and it's signal chain
    favouritesModel = createFollowedChannelsModel();

    favouritesProxy = new QSortFilterProxyModel();

    favouritesProxy->setSortRole(ChannelListModel::Roles::ViewersRole);
    favouritesProxy->sort(0, Qt::DescendingOrder);
    favouritesProxy->setSourceModel(favouritesModel);

    connect(netman, &NetworkManager::allStreamsOperationFinished, this, &ChannelManager::updateStreams);
    connect(netman, &NetworkManager::featuredStreamsOperationFinished, this, &ChannelManager::addSearchResults);
    connect(netman, &NetworkManager::gamesOperationFinished, this, &ChannelManager::addGames);
    connect(netman, &NetworkManager::gameStreamsOperationFinished, this, &ChannelManager::addSearchResults);
    connect(netman, &NetworkManager::searchChannelsOperationFinished, this, &ChannelManager::addSearchResults);
    connect(netman, &NetworkManager::m3u8OperationFinished, this, &ChannelManager::foundPlaybackStream);
    connect(netman, &NetworkManager::searchGamesOperationFinished, this, &ChannelManager::addGames);

    connect(netman, &NetworkManager::userOperationFinished, this, &ChannelManager::onUserUpdated);

    connect(netman, &NetworkManager::favouritesReplyFinished, this, &ChannelManager::addFollowedResults);

    connect(netman, &NetworkManager::networkAccessChanged, this, &ChannelManager::slotNetworkAccessChanged);
    connect(settingsManager, &SettingsManager::accessTokenChanged, this, &ChannelManager::updateAccessToken);

    load();

    //Start polling timer, setting id as property
    setProperty("pollTimer", startTimer(30000, Qt::VeryCoarseTimer));


    if (SettingsManager::getInstance()->hasAccessToken()) {
        updateAccessToken(SettingsManager::getInstance()->accessToken());
    }
}

ChannelManager *ChannelManager::getInstance() {
    static ChannelManager *instance = new ChannelManager();
    return instance;
}

ChannelManager::~ChannelManager(){
    qDebug() << "Destroyer: ChannelManager";

    save();

    delete favouritesModel;
    delete resultsModel;
    delete gamesModel;
    delete favouritesProxy;
}

void ChannelManager::addToFavourites(const quint32 &id, const QString &serviceName, const QString &title,
                                     const QString &info, const QString &logo, const QString &preview,
                                     const QString &game, const qint32 &viewers, bool online)
{
    if (!favouritesModel->find(id)){
        Channel *channel = new Channel();
        channel->setId(id);
        channel->setServiceName(serviceName);
        channel->setName(title);
        channel->setInfo(info);
        channel->setLogourl(logo);
        channel->setPreviewurl(preview);
        channel->setGame(game);
        channel->setOnline(online);
        channel->setViewers(viewers);
        channel->setFavourite(true);

        if (isAccessTokenAvailable() && user_id != 0) {
            netman->editUserFavourite(user_id, channel->getId(), true);
        }

        favouritesModel->addChannel(channel);

        emit addedChannel(channel->getId());

        Channel *chan = resultsModel->find(channel->getId());
        if (chan){
            chan->setFavourite(true);
            resultsModel->updateChannelForView(chan);
        }

        if (!isAccessTokenAvailable())
            save();
    }
}

void ChannelManager::searchGames(QString q, const quint32 &offset, const quint32 &limit)
{
    if (offset == 0 || !q.isEmpty())
        gamesModel->clear();

    //If query is empty, search games by viewercount
    if (q.isEmpty())
        netman->getGames(offset, limit);

    //Else by queryword
    else if (offset == 0)
        netman->searchGames(q);

    emit gamesSearchStarted();
}

QString ChannelManager::username() const
{
    return user_name;
}

void ChannelManager::updateAccessToken(QString /*accessToken*/)
{
    if (isAccessTokenAvailable()) {
        //Fetch display name for logged in user
        netman->getUser();

        //move favs to tempfavs
        if (!tempFavourites) {
            tempFavourites = favouritesModel;

            favouritesModel = createFollowedChannelsModel();
            favouritesProxy->setSourceModel(favouritesModel);
        }
    }

    else {
        // if we just logged out there are user settings to clear
        user_id = 0;
        user_name = "";

        //Reload local favourites from memory
        if (tempFavourites) {
            delete favouritesModel;
            favouritesModel = tempFavourites;
            tempFavourites = 0;
            favouritesProxy->setSourceModel(favouritesModel);
        }

        emit login("", "");
    }

    emit accessTokenUpdated();
}

void ChannelManager::timerEvent(QTimerEvent *event)
{
    checkFavourites();
}

ChannelListModel *ChannelManager::getFavouritesModel() const
{
    return favouritesModel;
}

QSortFilterProxyModel *ChannelManager::getFavouritesProxy() const
{
    return favouritesProxy;
}

GameListModel *ChannelManager::getGamesModel() const
{
    return gamesModel;
}

ChannelListModel *ChannelManager::getResultsModel() const
{
    return resultsModel;
}

void ChannelManager::load(){
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    int size = settings.beginReadArray("channels");
    if (size > 0) {
        QList<Channel*> _channels;

        for (int i = 0; i < size; i++) {
            settings.setArrayIndex(i);
            Channel * channel = new Channel(settings);
            channel->setFavourite(true);
            _channels.append(channel);
        }

        favouritesModel->addAll(_channels);

        qDeleteAll(_channels);
    }
    settings.endArray();
}

void ChannelManager::save()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    if (!settings.isWritable())
        qDebug() << "Error: settings file not writable";

    if (tempFavourites) {
        delete favouritesModel;
        favouritesModel = tempFavourites;
        tempFavourites = 0;
    }

    //Write channels
    settings.beginWriteArray("channels");
    for (int i=0; i < favouritesModel->count(); i++){
        settings.setArrayIndex(i);
        favouritesModel->getChannels().at(i)->writeToSettings(settings);
    }
    settings.endArray();
}


void ChannelManager::addToFavourites(const quint32 &id){
    Channel *channel = resultsModel->find(id);

    if (channel){

        if (isAccessTokenAvailable() && user_id != 0) {
            netman->editUserFavourite(user_id, channel->getId(), true);
        }

        channel->setFavourite(true);
        favouritesModel->addChannel(new Channel(*channel));

        emit addedChannel(channel->getId());

        resultsModel->updateChannelForView(channel);

        if (!isAccessTokenAvailable())
            save();
    }
}

void ChannelManager::removeFromFavourites(const quint32 &id){
    Channel *chan = favouritesModel->find(id);

    emit deletedChannel(chan->getId());

    if (isAccessTokenAvailable() && user_id != 0) {
        netman->editUserFavourite(user_id, chan->getId(), false);
    }

    favouritesModel->removeChannel(chan);

    chan = 0;

    //Update results
    Channel* channel = resultsModel->find(id);
    if (channel){

        channel->setFavourite(false);
        resultsModel->updateChannelForView(channel);
    }

    if (!isAccessTokenAvailable())
        save();
}

QString commaSeparatedChannelIds(const QList<Channel *> & channels) {
    QStringList channelIdStrs;
    foreach(Channel* channel, channels) {
        channelIdStrs.append(QString::number(channel->getId()));
    }
    return channelIdStrs.join(',');
}

void ChannelManager::checkStreams(const QList<Channel *> &list)
{
    //Divide list to sublists for sanity
    int pos = 0;

    while(pos < list.length()) {

        //Take sublist, max 50 items
        QList<Channel*> sublist = list.mid(pos, 50);

        //Fetch channels
        QString url = KRAKEN_API
                + QString("/streams?")
                + QString("limit=%1").arg(50) //Important!
                + QString("&channel=") + commaSeparatedChannelIds(sublist);
        netman->getStreams(url);

        //Shift pos by 50
        pos += sublist.length();
    }
}

void ChannelManager::checkFavourites()
{
    checkStreams(favouritesModel->getChannels());
}

void ChannelManager::searchChannels(QString q, const quint32 &offset, const quint32 &limit, bool clear)
{
    if (clear)
        resultsModel->clear();

    if (q.isEmpty()) {
        netman->getFeaturedStreams();
    }
    else if (q.startsWith("/game ")){
        q.replace("/game ", "");
        netman->getStreamsForGame(q, offset, limit);

    } else {
        netman->searchChannels(q, offset, limit);
    }

    emit searchingStarted();
}

void ChannelManager::addSearchResults(const QList<Channel*> &list, const int total)
{
    bool needsStreamCheck = false;

    foreach (Channel *channel, list){
        if (favouritesModel->find(channel->getId()))
            channel->setFavourite(true);

        if (!channel->isOnline())
            needsStreamCheck = true;
    }

    int numAdded = resultsModel->addAll(list);

    if (needsStreamCheck)
        checkStreams(list);

    qDeleteAll(list);

    emit resultsUpdated(numAdded, total);
}

void ChannelManager::findPlaybackStream(const QString &serviceName)
{
    netman->getChannelPlaybackStream(serviceName);
}

void ChannelManager::updateFavourites(const QList<Channel*> &list)
{
    foreach (Channel *c, list)
        c->setFavourite(true);

    favouritesModel->updateChannels(list);
    qDeleteAll(list);
}

bool ChannelManager::containsFavourite(const quint32 &q)
{
    return favouritesModel->find(q) != nullptr;
}

//Updates channel streams in all models
void ChannelManager::updateStreams(const QList<Channel*> &list)
{
    favouritesModel->updateStreams(list);
    resultsModel->updateStreams(list);
    qDeleteAll(list);
}

void ChannelManager::addGames(const QList<Game*> &list)
{
    gamesModel->addAll(list);

    qDeleteAll(list);

    emit gamesUpdated();
}

void ChannelManager::notify(Channel *channel)
{
    if (settingsManager->alert() && channel){

        if (!channel->isOnline() && !settingsManager->offlineNotifications())
            //Skip offline notifications if set
            return;

        emit pushNotification(channel->getName() + (channel->isOnline() ? " is now streaming" : " has gone offline"),
                              channel->getInfo(),
                              channel->getLogourl());
    }
}

void ChannelManager::notifyMultipleChannelsOnline(const QList<Channel*> &channels)
{
    if (channels.size() == 1) {
        //Only one channel, send the usual notification
        notify(channels.at(0));
    }

    else if (settingsManager->alert()) {
        //Send multi-notification
        QString str;

        foreach (Channel *c, channels) {

            //Omit channels after enough characters in message body
            if (str.size() > 80) {
                str.append("...");
                break;
            }

            str.append(!str.isEmpty() ? ", " : "");
            str.append(c->getName());
        }

        emit pushNotification("Channels are streaming", str, DEFAULT_LOGO_URL);
    }
}

//Login function
void ChannelManager::onUserUpdated(const QString &name, const quint64 userId)
{
    user_name = name;
    user_id = userId;
    emit userNameUpdated(user_name);

    if (isAccessTokenAvailable()) {
        emit login(user_name, settingsManager->accessToken());

        //Start using user followed channels
        getFollowedChannels(FOLLOWED_FETCH_LIMIT, 0);
    }
}

void ChannelManager::getFollowedChannels(const quint32& limit, const quint32& offset)
{
    //if (offset == 0)
    //favouritesModel->clear();

    netman->getUserFavourites(user_id, offset, limit);
}

void ChannelManager::addFollowedResults(const QList<Channel *> &list, const quint32 offset, const quint32 total)
{
    //    qDebug() << "Merging channel data for " << list.size()
    //             << " items with " << offset << " offset.";

    foreach (Channel *c, list)
        c->setFavourite(true);

    favouritesModel->mergeAll(list);

    if (offset < total)
        getFollowedChannels(FOLLOWED_FETCH_LIMIT, offset);

    checkStreams(list);

    qDeleteAll(list);

    emit followedUpdated();
}

void ChannelManager::slotNetworkAccessChanged(bool up)
{
    if (up) {
        if (isAccessTokenAvailable()) {
            //Relogin
            favouritesModel->clear();
            netman->getUser();
        }
    } else {
        qDebug() << "Network went down";
        favouritesModel->setAllChannelsOffline();
        resultsModel->setAllChannelsOffline();
    }
}

quint64 ChannelManager::getUser_id() const
{
    return user_id;
}

ChannelListModel *ChannelManager::createFollowedChannelsModel()
{
    ChannelListModel *model = new ChannelListModel();

    connect(model, &ChannelListModel::channelOnlineStateChanged, this, &ChannelManager::notify);
    connect(model, &ChannelListModel::multipleChannelsChangedOnline, this, &ChannelManager::notifyMultipleChannelsOnline);

    return model;
}
