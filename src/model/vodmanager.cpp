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

#include "vodmanager.h"
#include <QSettings>
#include <QCoreApplication>
#include <cmath>

VodManager::VodManager(QObject *parent) :
    QObject(parent),
    netman(NetworkManager::getInstance())
{
    qmlRegisterInterface<VodListModel>("VodListModel");
    _model = new VodListModel(this);

    connect(netman, &NetworkManager::broadcastsOperationFinished, this, &VodManager::onSearchFinished);
    connect(netman, &NetworkManager::m3u8OperationBFinished, this, &VodManager::streamsGetFinished);
    connect(netman, &NetworkManager::vodChatPieceGetOperationFinished, this, &VodManager::vodChatPieceGetOperationFinished);

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    int numLastPositions = settings.beginReadArray("lastPositions");
    for (int i = 0; i < numLastPositions; i++) {
        settings.setArrayIndex(i);
        const QString channel = settings.value("channel").toString();
        const QString vod = settings.value("vod").toString();
        const quint64 lastPosition = settings.value("position").toULongLong();

        vodLastPlaybackPositionLoaded(channel, vod, lastPosition, i);
    }
    settings.endArray();

    emit modelChanged();
}

VodManager *VodManager::getInstance() {
    static VodManager *instance = new VodManager();
    return instance;
}

VodManager::~VodManager()
{
    saveSettings();
    delete _model;
}

void VodManager::search(const quint64 channelId, const quint32 offset, const quint32 limit)
{
    if (offset == 0) {
        _model->clear();
        emit searchStarted();
    }

    netman->getBroadcasts(channelId, offset, limit);
}

void VodManager::onSearchFinished(QList<Vod *> items)
{
    _model->addAll(items);

    qDeleteAll(items);
    items.clear();

    emit searchFinished();
}

VodListModel *VodManager::getModel() const
{
    return _model;
}

void VodManager::saveSettings() {
    //Save
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    //Write last positions
    int nextLastPositionEntry = settings.beginReadArray("lastPositions");
    settings.endArray();

    settings.beginWriteArray("lastPositions");
    for (auto channelEntry = channelVodLastPositions.begin(); channelEntry != channelVodLastPositions.end(); channelEntry++) {
        auto & vods = channelEntry.value();
        for (auto vodEntry = vods.begin(); vodEntry != vods.end(); vodEntry++) {
            auto & lastPosition = vodEntry.value();
            if (lastPosition.modified) {
                if (lastPosition.settingsIndex == -1) {
                    lastPosition.settingsIndex = nextLastPositionEntry++;
                }

                settings.setArrayIndex(lastPosition.settingsIndex);
                settings.setValue("channel", channelEntry.key());
                settings.setValue("vod", vodEntry.key());
                settings.setValue("position", vodEntry.value().lastPosition);
            }
        }
    }
    settings.endArray();

}

QString VodManager::getGame() const
{
    return game;
}

void VodManager::getBroadcasts(QString vod)
{
    //Remove leading NaN characters
    vod.remove(QRegExp("[^0-9]"));

    netman->getBroadcastPlaybackStream(vod);
}

void VodManager::getVodChatPiece(quint64 vodId, quint64 offset) {
    netman->getVodChatPiece(vodId, offset);
}

void VodManager::getNextVodChatPiece(quint64 vodId, QString cursor) {
    netman->getNextVodChatPiece(vodId, cursor);
}

void VodManager::cancelLastVodChatRequest() {
    netman->cancelLastVodChatRequest();
}

void VodManager::resetVodChat() {
    netman->resetVodChat();
}

void VodManager::setVodLastPlaybackPosition(const QString & channel, const QString & vod, quint64 position) {
    auto channelEntry = channelVodLastPositions.find(channel);
    if (channelEntry == channelVodLastPositions.end()) {
        channelEntry = channelVodLastPositions.insert(channel, QMap<QString, LastPosition>());
    }

    auto & vodMap = channelEntry.value();
    auto vodEntry = vodMap.find(vod);
    if (vodEntry != vodMap.end()) {
        vodEntry.value().lastPosition = position;
        vodEntry.value().modified = true;
        if (std::fabs(static_cast<double>(vodEntry.value().lastPosition) - static_cast<double>(position)) >= 10) {
            saveSettings();
        }
    } else {
        // -1 index to be replaced at settings save time
        vodMap.insert(vod, {position, true, -1});
        saveSettings();
    }

    emit vodLastPositionUpdated(channel, vod, position);
}

void VodManager::vodLastPlaybackPositionLoaded(const QString & channel, const QString & vod, quint64 position, int settingsIndex) {
    auto channelEntry = channelVodLastPositions.find(channel);
    if (channelEntry == channelVodLastPositions.end()) {
        channelEntry = channelVodLastPositions.insert(channel, QMap<QString, LastPosition>());
    }

    auto & vodMap = channelEntry.value();
    vodMap.remove(vod);
    vodMap.insert(vod, {position, false, settingsIndex});
}

QVariant VodManager::getVodLastPlaybackPosition(const QString & channel, const QString & vod) {
    auto channelEntry = channelVodLastPositions.find(channel);
    if (channelEntry == channelVodLastPositions.end()) {
        return QVariant();
    }

    auto & vodMap = channelEntry.value();
    auto vodEntry = vodMap.find(vod);
    if (vodEntry == vodMap.end()) {
        return QVariant();
    }

    return vodEntry.value().lastPosition;
}

QVariantMap VodManager::getChannelVodsLastPlaybackPositions(const QString & channel) {
    QVariantMap out;
    auto channelEntry = channelVodLastPositions.find(channel);
    if (channelEntry != channelVodLastPositions.end()) {
        auto & vodMap = channelEntry.value();
        for (auto vodEntry = vodMap.constBegin(); vodEntry != vodMap.constEnd(); vodEntry++) {
            out.insert(vodEntry.key(), vodEntry.value().lastPosition);
        }
    }
    return out;
}
