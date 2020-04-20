#include "badgecontainer.h"
#include "settingsmanager.h"

BadgeContainer::BadgeContainer() : netman(NetworkManager::getInstance())
{
    connect(netman, &NetworkManager::getEmoteSetsOperationFinished, this, &BadgeContainer::onEmoteSetsUpdated);
    connect(netman, &NetworkManager::getChannelBadgeUrlsOperationFinished, this, &BadgeContainer::innerChannelBadgeUrlsLoaded);
    connect(netman, &NetworkManager::getChannelBadgeBetaUrlsOperationFinished, this, &BadgeContainer::innerChannelBadgeBetaUrlsLoaded);
    connect(netman, &NetworkManager::getGlobalBadgeBetaUrlsOperationFinished, this, &BadgeContainer::innerGlobalBadgeBetaUrlsLoaded);

    connect(netman, &NetworkManager::getChannelBitsUrlsOperationFinished, this, &BadgeContainer::innerChannelBitsDataLoaded);
    connect(netman, &NetworkManager::getGlobalBitsUrlsOperationFinished, this, &BadgeContainer::innerGlobalBitsDataLoaded);

    connect(netman, &NetworkManager::getGlobalBttvEmotesOperationFinished, this, &BadgeContainer::innerGlobalBttvEmotesLoaded);
    connect(netman, &NetworkManager::getChannelBttvEmotesOperationFinished, this, &BadgeContainer::innerChannelBttvEmotesLoaded);
}

BadgeContainer *BadgeContainer::getInstance()
{
    static BadgeContainer *instance = new BadgeContainer();
    return instance;
}

bool BadgeContainer::getChannelBadgeUrl(const QString channelId, const QString badgeName, const QString imageFormat, QString &outUrl) const {
    auto channelEntry = channelBadgeUrls.find(channelId);
    if (channelEntry != channelBadgeUrls.end()) {
        auto badgeEntry = channelEntry.value().find(badgeName);
        if (badgeEntry != channelEntry.value().end()) {
            auto urlEntry = badgeEntry.value().find(imageFormat);
            if (urlEntry != badgeEntry.value().end()) {
                outUrl = urlEntry.value();
                return true;
            }
        }
    }
    return false;
}

bool BadgeContainer::getChannelBadgeBetaUrl(const QString channel, const QString badgeName, const QString version, const QString imageFormat, QString &outUrl) const {
    auto channelEntry = channelBadgeBetaUrls.find(channel);
    if (channelEntry != channelBadgeBetaUrls.end()) {
        auto badgeEntry = channelEntry.value().find(badgeName);
        if (badgeEntry != channelEntry.value().end()) {
            auto versionEntry = badgeEntry.value().find(version);
            if (versionEntry != badgeEntry.value().end()) {
                auto urlEntry = versionEntry.value().find(imageFormat);
                if (urlEntry != versionEntry.value().end()) {
                    outUrl = urlEntry.value();
                    return true;
                }
            }
        }
    }
    return false;
}

bool BadgeContainer::getChannelBitsUrl(const int channelId, const QString &prefix, const QString &minBits, QString &outUrl) const {
    auto channelEntry = channelBitsUrls.find(channelId);
    if (channelEntry != channelBitsUrls.end()) {
        auto actionEntry = channelEntry.value().find(prefix);
        if (actionEntry != channelEntry.value().end()) {
            auto tierEntry = actionEntry.value().find(minBits);
            if (tierEntry != actionEntry.value().end()) {
                outUrl = tierEntry.value();
                return true;
            }
        }
    }
    return false;
}

bool BadgeContainer::getChannelBitsColor(const int channelId, const QString &prefix, const QString &minBits, QString &outColor) {
    auto channelEntry = channelBitsColors.find(channelId);
    if (channelEntry != channelBitsColors.end()) {
        auto actionEntry = channelEntry.value().find(prefix);
        if (actionEntry != channelEntry.value().end()) {
            auto tierEntry = actionEntry.value().find(minBits);
            if (tierEntry != actionEntry.value().end()) {
                outColor = tierEntry.value();
                return true;
            }
        }
    }
    return false;
}

QVariantMap convertEmoteSets(const QMap<int, QMap<int, QString>> emoteSets) {
    QVariantMap out;
    for (auto setEntry = emoteSets.begin(); setEntry != emoteSets.end(); setEntry++) {
        QVariantMap cur;
        auto set = setEntry.value();
        for (auto emote = set.begin(); emote != set.end(); emote++) {
            cur.insert(QString::number(emote.key()), emote.value());
        }
        out.insert(QString::number(setEntry.key()), cur);
    }
    return out;
}

bool BadgeContainer::loadEmoteSets(bool reload, const QList<int> &emoteSetIDs) {
    if (!haveEmoteSets || (emoteSetIDs != lastRequestedEmoteSetIDs)) {
        reload = true;
    }

    if (reload) {
        if (SettingsManager::getInstance()->hasAccessToken()) {
            haveEmoteSets = false;
            lastRequestedEmoteSetIDs = emoteSetIDs;
            netman->getEmoteSets(emoteSetIDs);
            return true;
        }
        else {
            return false;
        }
    }
    else {
        // ok to deliver cached emote sets
        emit emoteSetsLoaded(convertEmoteSets(lastEmoteSets));
        return true;
    }
}

QVariantMap convertBadges(const QMap<QString, QMap<QString, QString>> &badges) {
    QVariantMap out;
    for (auto x = badges.constBegin(); x != badges.constEnd(); x++) {
        QVariantMap cur;
        auto badgeEntries = x.value();
        for (auto y = badgeEntries.constBegin(); y != badgeEntries.constEnd(); y++) {
            cur.insert(y.key(), y.value());
        }
        out.insert(x.key(), cur);
    }
    return out;
}

QVariantMap convertBetaBadges(const QMap<QString, QMap<QString, QMap<QString, QString>>> &badges) {
    QVariantMap out;
    for (auto x = badges.constBegin(); x != badges.constEnd(); x++) {
        out.insert(x.key(), convertBadges(x.value()));
    }
    return out;
}

bool BadgeContainer::loadChannelBadgeUrls(const quint64 channelId) {
    auto result = channelBadgeUrls.find(QString::number(channelId));
    if (result != channelBadgeUrls.end()) {
        // deliver cached channel badge URLs
        emit channelBadgeUrlsLoaded(channelId, convertBadges(result.value()));
        return false;
    }
    else {
        netman->getChannelBadgeUrls(channelId);
        return true;
    }
}

bool BadgeContainer::loadChannelBetaBadgeUrls(int channel) {
    bool out = false;

    const QString channelKey = QString::number(channel);
    auto result = channelBadgeBetaUrls.constFind(channelKey);
    if (result != channelBadgeBetaUrls.constEnd()) {
        // deliver cached channel beta badge URLS
        emit channelBadgeBetaUrlsLoaded(channelKey, convertBetaBadges(result.value()));
    }
    else {
        netman->getChannelBadgeUrlsBeta(channel);
        out = true;
    }

    const QString GLOBAL_BADGES_IDENTIFIER = "GLOBAL";
    result = channelBadgeBetaUrls.find(GLOBAL_BADGES_IDENTIFIER);
    if (result != channelBadgeBetaUrls.end()) {
        // deliver cached channel beta badge URLS
        emit channelBadgeBetaUrlsLoaded(GLOBAL_BADGES_IDENTIFIER, convertBetaBadges(result.value()));
    }
    else {
        netman->getGlobalBadgesUrlsBeta();
        out = true;
    }

    return out;
}

bool BadgeContainer::loadChannelBttvEmotes(const QString channel) {
    bool out = false;

    auto result = channelBttvEmotes.constFind(channel);
    if (result != channelBttvEmotes.constEnd()) {
        // deliver cached channel bttv emotes
        emit channelBttvEmotesLoaded(channel, result.value());
    }
    else {
        netman->getChannelBttvEmotes(channel);
        out = true;
    }

    const QString GLOBAL_EMOTES_IDENTIFIER = "GLOBAL";
    result = channelBttvEmotes.constFind(GLOBAL_EMOTES_IDENTIFIER);
    if (result != channelBttvEmotes.constEnd()) {
        emit channelBttvEmotesLoaded(GLOBAL_EMOTES_IDENTIFIER, result.value());
    }
    else {
        netman->getGlobalBttvEmotes();
        out = true;
    }

    return out;
}

const QUrl BadgeContainer::getBitsUrlForKey(const QString & key) const {
    QString url;

    QList<QString> parts = key.split("-");
    if (parts.length() == 6) {
        const QString & channelIdStr = parts[0];
        const QString & prefix = parts[4];
        const QString & minBits = parts[5];
        const int channelId = channelIdStr == "GLOBAL" ? -1 : channelIdStr.toInt();

        if (BadgeContainer::getInstance()->getChannelBitsUrl(channelId, prefix, minBits, url)) {
            return url;
        }
    }
    qDebug() << "Invalid bits cache key" << key;
    return QUrl();
}

bool BadgeContainer::loadChannelBitsUrls(int channel) {
    bool out = false;

    const int GLOBAL_BITS_IDENTIFIER = -1;

    auto result = channelBitsUrls.find(channel);
    if (result != channelBitsUrls.end()) {
        // deliver cached channel bits URLS
        auto colors = channelBitsColors.find(channel);

        emit channelBitsUrlsLoaded(channel, result.value(), colors.value());
    }
    else {
        netman->getChannelBitsUrls(channel);
        out = true;
    }

    result = channelBitsUrls.find(GLOBAL_BITS_IDENTIFIER);
    if (result != channelBitsUrls.end()) {
        // deliver cached channel bits URLS
        auto colors = channelBitsColors.find(GLOBAL_BITS_IDENTIFIER);

        emit channelBitsUrlsLoaded(GLOBAL_BITS_IDENTIFIER, result.value(), colors.value());
    }
    else {
        netman->getGlobalBitsUrls();
        out = true;
    }

    return out;
}

void BadgeContainer::onEmoteSetsUpdated(const QMap<int, QMap<int, QString>> updatedEmoteSets)
{
    lastEmoteSets = updatedEmoteSets;
    haveEmoteSets = true;

    //qDebug() << "emitting updated emote set" << updatedEmoteSets;

    emit emoteSetsLoaded(convertEmoteSets(updatedEmoteSets));
}

void BadgeContainer::innerChannelBadgeUrlsLoaded(const quint64 channelId, const QMap<QString, QMap<QString, QString>> badgeUrls)
{
    const QString channelIdStr = QString::number(channelId);
    channelBadgeUrls.remove(channelIdStr);
    channelBadgeUrls.insert(channelIdStr, badgeUrls);

    emit channelBadgeUrlsLoaded(channelId, convertBadges(badgeUrls));
}

void BadgeContainer::innerChannelBadgeBetaUrlsLoaded(const int channelId, const QMap<QString, QMap<QString, QMap<QString, QString>>> badgeData)
{
    QString channelKey = QString::number(channelId);
    channelBadgeBetaUrls.remove(channelKey);
    channelBadgeBetaUrls.insert(channelKey, badgeData);

    emit channelBadgeBetaUrlsLoaded(channelKey, convertBetaBadges(badgeData));
}

void BadgeContainer::innerGlobalBadgeBetaUrlsLoaded(const QMap<QString, QMap<QString, QMap<QString, QString>>> badgeData)
{
    const QString GLOBAL_BADGES_KEY = "GLOBAL";
    channelBadgeBetaUrls.remove(GLOBAL_BADGES_KEY);
    channelBadgeBetaUrls.insert(GLOBAL_BADGES_KEY, badgeData);

    emit channelBadgeBetaUrlsLoaded(GLOBAL_BADGES_KEY, convertBetaBadges(badgeData));
}

void BadgeContainer::innerChannelBitsDataLoaded(int channelID, QMap<QString, QMap<QString, QString>> curChannelBitsUrls, QMap<QString, QMap<QString, QString>> curChannelBitsColors) {
    channelBitsUrls.remove(channelID);
    channelBitsUrls.insert(channelID, curChannelBitsUrls);

    channelBitsColors.remove(channelID);
    channelBitsColors.insert(channelID, curChannelBitsColors);

    emit channelBitsUrlsLoaded(channelID, curChannelBitsUrls, curChannelBitsColors);
}

void BadgeContainer::innerGlobalBitsDataLoaded(QMap<QString, QMap<QString, QString>> globalBitsUrls, QMap<QString, QMap<QString, QString>> globalBitsColors) {
    const int GLOBAL_BITS_KEY = -1;
    channelBitsUrls.remove(GLOBAL_BITS_KEY);
    channelBitsUrls.insert(GLOBAL_BITS_KEY, globalBitsUrls);

    channelBitsColors.remove(GLOBAL_BITS_KEY);
    channelBitsColors.insert(GLOBAL_BITS_KEY, globalBitsColors);

    emit channelBitsUrlsLoaded(GLOBAL_BITS_KEY, globalBitsUrls, globalBitsColors);
}

void BadgeContainer::innerChannelBttvEmotesLoaded(const QString channel, QMap<QString, QString> & emotesByCode) {
    channelBttvEmotes.remove(channel);
    channelBttvEmotes.insert(channel, emotesByCode);
    emit channelBttvEmotesLoaded(channel, emotesByCode);
}

void BadgeContainer::innerGlobalBttvEmotesLoaded(QMap<QString, QString> & emotesByCode) {
    const QString GLOBAL_EMOTES_KEY = "GLOBAL";
    channelBttvEmotes.remove(GLOBAL_EMOTES_KEY);
    channelBttvEmotes.insert(GLOBAL_EMOTES_KEY, emotesByCode);
    emit channelBttvEmotesLoaded(GLOBAL_EMOTES_KEY, emotesByCode);
}
