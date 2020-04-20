#include "settingsmanager.h"
#include "../network/httpserver.h"
#include <QCoreApplication>

SettingsManager::SettingsManager(QObject *parent) :
    QObject(parent), settings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this)
{
    load();
    //Connections
    connect(HttpServer::getInstance(), &HttpServer::codeReceived, this, &SettingsManager::setAccessToken);
}

SettingsManager *SettingsManager::getInstance()
{
    static SettingsManager *instance = new SettingsManager();
    return instance;
}

void SettingsManager::load()
{
    //Load values from settings, notifying changes as needed
    setAlert(settings.value("alert", mAlert).toBool());
    setAlertPosition(settings.value("alertPosition", mAlertPosition).toInt());
    setMultipleInstances(settings.value("multipleInstances", mMultipleInstances).toBool());
    setMinimizeOnStartup(settings.value("minimizeOnStartup", mMinimizeOnStartup).toBool());
    setOpengl(settings.value("opengl", mOpengl).toString());
    setQuality(settings.value("quality", mQuality).toString());
    setDecoder(settings.value("decoder", mDecoder).toString());
    setBackend(settings.value("backend", mBackend).toString());
    setVolumeLevel(settings.value("volumeLevel", mVolumeLevel).toInt());
    setChatEdge(settings.value("chatEdge", mChatEdge).toInt());
    setTextScaleFactor(settings.value("textScaleFactor", mTextScaleFactor).toDouble());
    setOfflineNotifications(settings.value("offlineNotifications", mOfflineNotifications).toBool());
    setLightTheme(settings.value("lightTheme", mLightTheme).toBool());
    setAccessToken(settings.value("accessToken", mAccessToken).toString());
    setFont(settings.value("font", mFont).toString());
    setKeepOnTop(settings.value("keepOnTop", mKeepOnTop).toBool());
    setPastelColors(settings.value("pastelColors", mPastelColors).toBool());
    setClickTogglePause(settings.value("clickTogglePause", mClickTogglePause).toBool());
}

bool SettingsManager::alert() const
{
    return mAlert;
}

void SettingsManager::setAlert(bool alert)
{
    if (mAlert != alert) {
        mAlert = alert;
        settings.setValue("alert", alert);
        emit alertChanged();
        qDebug() << "alert changed to" << alert;
    }
}

bool SettingsManager::multipleInstances() const
{
    return mMultipleInstances;
}

void SettingsManager::setMultipleInstances(bool multipleInstances)
{
    if (mMultipleInstances != multipleInstances) {
        mMultipleInstances = multipleInstances;
        settings.setValue("multipleInstances", multipleInstances);
        emit multipleInstancesChanged();
        qDebug() << "multipleInstances changed to" << multipleInstances;
    }
}

int SettingsManager::alertPosition() const
{
    return mAlertPosition;
}

void SettingsManager::setAlertPosition(int alertPosition)
{
    if (alertPosition != mAlertPosition) {
        mAlertPosition = alertPosition;
        settings.setValue("alertPosition", alertPosition);
        emit alertPositionChanged();
        qDebug() << "alertPosition changed to" << alertPosition;
    }
}

int SettingsManager::volumeLevel() const
{
    return mVolumeLevel;
}

void SettingsManager::setVolumeLevel(int volumeLevel)
{
    if (mVolumeLevel != volumeLevel) {
        mVolumeLevel = volumeLevel;
        settings.setValue("volumeLevel", volumeLevel);
        emit volumeLevelChanged();
        qDebug() << "volumeLevel changed to" << volumeLevel;
    }
}

bool SettingsManager::minimizeOnStartup() const
{
#ifdef Q_OS_ANDROID
    return false;
#else
    return mMinimizeOnStartup;
#endif
}

void SettingsManager::setMinimizeOnStartup(bool minimizeOnStartup)
{
    if (mMinimizeOnStartup != minimizeOnStartup) {
        mMinimizeOnStartup = minimizeOnStartup;
        settings.setValue("minimizeOnStartup", minimizeOnStartup);
        emit minimizeOnStartupChanged();
        qDebug() << "minimizeOnStartup changed to" << minimizeOnStartup;
    }
}

int SettingsManager::chatEdge() const
{
#ifdef Q_OS_ANDROID
    return 2; // bottom
#else
    return mChatEdge;
#endif
}

void SettingsManager::setChatEdge(int chatEdge)
{
    if (mChatEdge != chatEdge) {
        mChatEdge = chatEdge;
        settings.setValue("chatEdge", chatEdge);
        emit chatEdgeChanged();
        qDebug() << "chatEdge changed to" << chatEdge;
    }
}

bool SettingsManager::offlineNotifications() const
{
    return mOfflineNotifications;
}

void SettingsManager::setOfflineNotifications(bool offlineNotifications)
{
    if (mOfflineNotifications != offlineNotifications) {
        mOfflineNotifications = offlineNotifications;
        settings.setValue("offlineNotifications", offlineNotifications);
        emit offlineNotificationsChanged();
        qDebug() << "offlineNotifications changed to" << offlineNotifications;
    }
}

double SettingsManager::textScaleFactor() const
{
    return mTextScaleFactor;
}

void SettingsManager::setTextScaleFactor(double textScaleFactor)
{
    //Validate min/max
    if (textScaleFactor < 0.5 || textScaleFactor > 3.0)
        return;

    if (mTextScaleFactor != textScaleFactor) {
        mTextScaleFactor = textScaleFactor;
        settings.setValue("textScaleFactor", textScaleFactor);
        emit textScaleFactorChanged();
        qDebug() << "textScaleFactor changed to" << textScaleFactor;
    }
}

QString SettingsManager::opengl() const
{
    return mOpengl;
}

void SettingsManager::setOpengl(const QString &opengl)
{
    if (mOpengl != opengl) {
        mOpengl = opengl;
        settings.setValue("opengl", opengl);
        emit openglChanged();
        qDebug() << "opengl changed to" << opengl;
    }
}

QString SettingsManager::quality() const
{
    return mQuality;
}

void SettingsManager::setQuality(const QString &quality)
{
    if (mQuality != quality) {
        mQuality = quality;
        settings.setValue("quality", quality);
        emit qualityChanged();
    }
}

QString SettingsManager::decoder() const
{
    return mDecoder;
}

void SettingsManager::setDecoder(const QString &decoder)
{
    if (mDecoder != decoder) {
        mDecoder = decoder;
        settings.setValue("decoder", decoder);
        emit decoderChanged();
    }
}

QString SettingsManager::backend() const
{
    return mBackend;
}

void SettingsManager::setBackend(const QString &backend)
{
    if (mBackend != backend) {
        mBackend = backend;
        settings.setValue("backend", backend);
        emit backendChanged();
    }
}

QStringList SettingsManager::backends() const
{
    return mBackends;
}

QString SettingsManager::accessToken() const
{
    return mAccessToken;
}

void SettingsManager::setAccessToken(const QString accessToken)
{
    if (mAccessToken != accessToken) {
        mAccessToken = accessToken;
        settings.setValue("accessToken", accessToken);
        emit accessTokenChanged(accessToken);
        qDebug() << "accessToken changed!";
    }
}

bool SettingsManager::hasAccessToken() const
{
    return !mAccessToken.isEmpty();
}

bool SettingsManager::hiDpi() const {
    return mHiDpi;
}

void SettingsManager::setHiDpi(bool dpi)
{
    mHiDpi = dpi;
    qDebug() << "hiDpi" << mHiDpi;
}

bool SettingsManager::lightTheme() const
{
    return mLightTheme;
}

void SettingsManager::setLightTheme(bool lightTheme)
{
    if (mLightTheme != lightTheme) {
        mLightTheme = lightTheme;
        settings.setValue("lightTheme", lightTheme);
        emit lightThemeChanged();
        qDebug() << "theme changed!";
    }
}

bool SettingsManager::pastelColors() const
{
    return mPastelColors;
}

void SettingsManager::setPastelColors(bool pastelColors)
{
    if (mPastelColors != pastelColors) {
        mPastelColors = pastelColors;
        settings.setValue("pastelColors", pastelColors);
        emit pastelColorsChanged();
    }
}

bool SettingsManager::clickTogglePause() const
{
    return mClickTogglePause;
}

void SettingsManager::setClickTogglePause(bool clickTogglePause)
{
    if (mClickTogglePause != clickTogglePause) {
        mClickTogglePause = clickTogglePause;
        settings.setValue("clickTogglePause", clickTogglePause);
        emit clickTogglePauseChanged();
    }
}

bool SettingsManager::autoScrollSmoothing() const
{
    return mAutoScrollSmoothing;
}

void SettingsManager::setAutoScrollSmoothing(bool autoScrollSmoothing)
{
    if (mAutoScrollSmoothing != autoScrollSmoothing) {
        mAutoScrollSmoothing = autoScrollSmoothing;
        settings.setValue("autoScrollSmoothing", autoScrollSmoothing);
        emit autoScrollSmoothingChanged();
    }
}

#include <QVersionNumber>
bool SettingsManager::isNewerVersion(QString version) const
{
    return QVersionNumber::fromString(version.replace('v',"")) > QVersionNumber::fromString(QString(APP_VERSION).replace('v', ""));
}

QString SettingsManager::font() const
{
    return mFont;
}

void SettingsManager::setFont(const QString &font)
{
    if (mFont != font) {
        mFont = font;
        settings.setValue("font", font);
        emit fontChanged();
    }
}

bool SettingsManager::versionCheckEnabled()
{
#ifdef VERSION_CHECK_ENABLED
    return true;
#else
    return false;
#endif
}

bool SettingsManager::keepOnTop() const
{
    return mKeepOnTop;
}

void SettingsManager::setKeepOnTop(bool keepOnTop)
{
    if (mKeepOnTop != keepOnTop) {
        mKeepOnTop = keepOnTop;
        settings.setValue("keepOnTop", keepOnTop);
        emit keepOnTopChanged();
    }
}
