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

#include <QObject>
#include <QSaveFile>
#include <QString>
#include <QDir>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QQuickImageProvider>
#include <QNetworkReply>
#include <QTimer>

// Handles state for an individual download
class DownloadHandler : public QObject
{
    Q_OBJECT
public:
    DownloadHandler(const QString filename, const QString key);
private:
    const QString filename;
    const QString key;
    QSaveFile _file;
    bool hadError;

signals:
    void downloadComplete(QString filename, bool hadError);

public slots:
    void dataAvailable();
    void replyFinished();
    void error(QNetworkReply::NetworkError code);
    const QString getKey() { return key; }
};

class ImageProvider;
// provides QImages to QML frontend
class CachedImageProvider : public QQuickImageProvider {
public:
    CachedImageProvider(ImageProvider const* provider);
    QImage requestImage(const QString &id, QSize * size, const QSize & requestedSize);
private:
    ImageProvider const* _provider;
};

// interface to QML
class ImageProvider : public QObject {
    Q_OBJECT
public:
    ImageProvider(const QString imageProviderName, const QString extension, const QString cacheDirName = "");
    ~ImageProvider();

    QString getImageProviderName() { return _imageProviderName;  }
    QString getBaseUrl() { return "image://" + _imageProviderName; }
    /* Make emote available by downloading it or loading it from cache if not already loaded.
    * Returns true if caller should wait for a downloadComplete event before using the emote */
    bool makeAvailable(QString key);
    bool download(QString key);
    bool downloadsInProgress() const;

    QHash<QString, QImage> imageTable();
    void loadImageFile(QString key, QString filename);

    QQmlImageProviderBase * getQMLImageProvider();
    /** Take an image key specific to the current context (e.g. channel), and determine a 
    service-wide unique key that we can use as an image provider key and cache filename */
    virtual QString getCanonicalKey(QString key);

signals:
    void downloadComplete();
    void bulkDownloadComplete();

public slots:
    void bulkDownload(const QList<QString> & keys);
    void individualDownloadComplete(QString filename, bool hadError);

protected slots:
    void bulkDownloadStep();

protected:
    virtual const QUrl getUrlForKey(QString & key) = 0;

private:
    static const int MSEC_PER_DOWNLOAD;

    QNetworkAccessManager _manager;

    friend class CachedImageProvider;
    CachedImageProvider *_cacheProvider;

    QHash<QString, QImage> _imageTable;
    QString _imageProviderName;
    QDir _cacheDir;
    int activeDownloadCount;
    QString _extension;
    QSet<QString> currentlyDownloading;
    QTimer _bulkDownloadTimer;
    QList<QString> _curBulkDownloadKeys;
    QList<QString>::const_iterator _bulkDownloadPos;
};

class URLFormatImageProvider : public ImageProvider {
    Q_OBJECT
public:
    URLFormatImageProvider(const QString imageProviderName, const QString urlFormat, const QString extension, const QString cacheDirName = "");
protected:
    virtual const QUrl getUrlForKey(QString & key);
private:
    QString _urlFormat;
};
