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


#include <QString>
#include <QUrl>
#include <QGuiApplication>
#include <QtGlobal>
#include <QDebug>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDateTime>
#include "imageprovider.h"

const int ImageProvider::MSEC_PER_DOWNLOAD = 16; // ~ 256kbit/sec for 2k images

ImageProvider::ImageProvider(const QString imageProviderName, const QString extension, const QString cacheDirName) : QObject(),
    _imageProviderName(imageProviderName), _extension(extension) {
    _cacheProvider = new CachedImageProvider(this);

    activeDownloadCount = 0;

    _bulkDownloadTimer.setInterval(MSEC_PER_DOWNLOAD);
    _bulkDownloadTimer.setSingleShot(true);
    connect(&_bulkDownloadTimer, &QTimer::timeout, this, &ImageProvider::bulkDownloadStep);

    QString useCacheDirName = cacheDirName != "" ? cacheDirName : imageProviderName;
    _cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QString("/" + useCacheDirName);
}

ImageProvider::~ImageProvider() {
}

bool ImageProvider::makeAvailable(QString key) {
    key = getCanonicalKey(key);

    if (currentlyDownloading.contains(key)) {
        // download of this emote in progress
        return true;
    }
    else if (download(key)) {
        // if this emote isn't already downloading, it's safe to load the cache file or download if not in the cache
        currentlyDownloading.insert(key);
        activeDownloadCount += 1;
        return true;
    }
    else {
        // we already had the emote locally and don't need to wait for it to download
        return false;
    }
}

bool ImageProvider::download(QString key) {
    if (_imageTable.contains(key)) {
        //qDebug() << "already in the table";
        return false;
    }

    const QUrl url = getUrlForKey(key);
    _cacheDir.mkpath(".");

    QString filename = _cacheDir.absoluteFilePath(key + _extension);

    if (_cacheDir.exists(key + _extension)) {
        //qDebug() << "local file already exists";
        loadImageFile(key, filename);
        return false;
    }
    qDebug() << "downloading " << url.toString();

    QNetworkRequest request(url);
    QNetworkReply* _reply = nullptr;
    _reply = _manager.get(request);

    DownloadHandler * dh = new DownloadHandler(filename, key);

    connect(_reply, &QNetworkReply::readyRead,
        dh, &DownloadHandler::dataAvailable);
    connect(_reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
        dh, &DownloadHandler::error);
    connect(_reply, &QNetworkReply::finished,
        dh, &DownloadHandler::replyFinished);
    connect(dh, &DownloadHandler::downloadComplete,
        this, &ImageProvider::individualDownloadComplete);

    return true;
}

void ImageProvider::bulkDownloadStep() {
    for (; _bulkDownloadPos != _curBulkDownloadKeys.constEnd(); _bulkDownloadPos++) {
        const QString & key = *_bulkDownloadPos;

        if (makeAvailable(key)) {
            // hit us back when the next time interval is up
            _bulkDownloadTimer.start();
            return;
        }
        else {
            qApp->processEvents();
        }
    }

    emit bulkDownloadComplete();
}

void ImageProvider::bulkDownload(const QList<QString> & keys) {
    _curBulkDownloadKeys = keys;

    _bulkDownloadPos = keys.constBegin();

    bulkDownloadStep();
}


void ImageProvider::individualDownloadComplete(QString filename, bool hadError) {
    DownloadHandler * dh = qobject_cast<DownloadHandler*>(sender());
    const QString emoteKey = dh->getKey();
    delete dh;

    if (hadError) {
        // delete partial download if any
        QFile(filename).remove();
    }
    else {
        loadImageFile(emoteKey, filename);
    }

    if (activeDownloadCount > 0) {
        activeDownloadCount--;
        qDebug() << activeDownloadCount << "active downloads remaining";
    }

    currentlyDownloading.remove(emoteKey);

    if (activeDownloadCount == 0) {
        emit downloadComplete();
    }
}

QHash<QString, QImage> ImageProvider::imageTable() {
    return _imageTable;
}

void ImageProvider::loadImageFile(QString emoteKey, QString filename) {
    QImage emoteImg;
    emoteImg.load(filename);
    _imageTable.insert(emoteKey, emoteImg);
}

QQmlImageProviderBase * ImageProvider::getQMLImageProvider() {
    return _cacheProvider;
}

bool ImageProvider::downloadsInProgress() const {
    return activeDownloadCount > 0;
}


URLFormatImageProvider::URLFormatImageProvider(const QString imageProviderName, const QString urlFormat, const QString extension, const QString cacheDir) :
    ImageProvider(imageProviderName, extension, cacheDir), _urlFormat(urlFormat)
{

}

const QUrl URLFormatImageProvider::getUrlForKey(QString & key) {
    return _urlFormat.arg(key);
}

QString ImageProvider::getCanonicalKey(const QString key) {
    return key;
}

// DownloadHandler

DownloadHandler::DownloadHandler(QString filename, QString key) : filename(filename), key(key), hadError(false) {
    _file.setFileName(filename);
    _file.open(QFile::WriteOnly);
    qDebug() << "save to" << filename;
}

void DownloadHandler::dataAvailable() {
    QNetworkReply* _reply = qobject_cast<QNetworkReply*>(sender());
    auto buffer = _reply->readAll();
    _file.write(buffer.data(), buffer.size());
}

void DownloadHandler::error(QNetworkReply::NetworkError /*code*/) {
    hadError = true;
    QNetworkReply* _reply = qobject_cast<QNetworkReply*>(sender());
    qDebug() << "Network error downloading" << _reply->request().url().toString() << ":" << _reply->errorString();
}

void DownloadHandler::replyFinished() {
    QNetworkReply* _reply = qobject_cast<QNetworkReply*>(sender());
    if (_reply) {
        _reply->deleteLater();
        _file.commit();
        //qDebug() << _file.fileName();
        //might need something for windows for the forwardslash..
        qDebug() << "download of" << _file.fileName() << "complete";

        emit downloadComplete(_file.fileName(), hadError);
    }
}


// CachedImageProvider
CachedImageProvider::CachedImageProvider(ImageProvider const* provider) : QQuickImageProvider(QQuickImageProvider::Image), _provider(provider) {

}

QImage CachedImageProvider::requestImage(const QString &id, QSize * size, const QSize & /*requestedSize*/) {
    // TODO figure out something sensible to do re requestedSize
    //qDebug() << "Requested id" << id << "from image provider";

    auto result = _provider->_imageTable.find(id);
    if (result != _provider->_imageTable.end()) {
        *size = result.value().size();
        return result.value();
    }
    return QImage();
}
