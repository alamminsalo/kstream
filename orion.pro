#-------------------------------------------------
#
# Orion: Twitch Client written in C++/QML
#
#-------------------------------------------------

QT     += gui qml network widgets quickcontrols2

QMAKE_CXXFLAGS += -Wall -O2

CONFIG += c++11
#CONFIG += console

TARGET = orion

isEmpty(PREFIX){
    PREFIX = /usr
}

VERSION = 1.6.6
DEFINES += APP_VERSION=\\\"v$$VERSION\\\"
DEFINES += APP_NAME=\\\"Orion\\\"

SOURCES += src/main.cpp\
    src/model/channelmanager.cpp \
    src/model/channel.cpp \
    src/util/fileutils.cpp \
    src/network/networkmanager.cpp \
    src/model/game.cpp \
    src/util/jsonparser.cpp \
    src/model/channellistmodel.cpp \
    src/model/gamelistmodel.cpp \
    src/model/vod.cpp \
    src/model/vodlistmodel.cpp \
    src/model/vodmanager.cpp \
    src/model/ircchat.cpp \
    src/model/imageprovider.cpp \
    src/model/badgeimageprovider.cpp \
    src/model/badgecontainer.cpp \
    src/model/viewersmodel.cpp \
    src/model/settingsmanager.cpp \
    src/network/httpserver.cpp \
    src/power/power.cpp


HEADERS  += src/model/channel.h \
    src/model/channelmanager.h \
    src/util/fileutils.h \
    src/network/networkmanager.h \
    src/model/game.h \
    src/util/jsonparser.h \
    src/model/channellistmodel.h \
    src/model/gamelistmodel.h \
    src/util/m3u8parser.h \
    src/model/vod.h \
    src/model/vodlistmodel.h \
    src/model/vodmanager.h \
    src/network/urls.h \
    src/model/ircchat.h \
    src/model/imageprovider.h \
    src/network/httpserver.h \
    src/model/badgeimageprovider.h \
    src/model/badgecontainer.h \
    src/model/viewersmodel.h \
    src/model/settingsmanager.h \
    src/model/singletonprovider.h \
    src/power/power.h

!android: {
    HEADERS += src/notification/notificationmanager.h
    SOURCES += src/notification/notificationmanager.cpp
}

android: {
    QT += androidextras
    QT -= widgets
    CONFIG += multimedia

    DISTFILES += android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat \
    android/src/com/orion/MainActivity.java

    ANDROID_EXTRA_LIBS = \
        $$PWD/../openssl-1.0.2l/libcrypto.so \
        $$PWD/../openssl-1.0.2l/libssl.so
}

#Backend for player, uses mpv as default
!qtav: !multimedia {
    CONFIG *= mpv
}

mpv {
    message(Selecting mpv as backend)
    #DEFINES += DEBUG_LIBMPV
    DEFINES += MPV_PLAYER
    DEFINES += PLAYER_BACKEND=\\\"mpv\\\"
    SOURCES +=  src/player/mpvobject.cpp
    HEADERS +=  src/player/mpvobject.h

    LIBS += -lmpv
}

qtav {
    message(Selecting qtav as backend)
    QT += av
    DEFINES += PLAYER_BACKEND=\\\"qtav\\\"
}

multimedia {
    message(Selecting qt multimedia as backend)
    QT += multimedia
    DEFINES += PLAYER_BACKEND=\\\"multimedia\\\"
    macx: {
        LIBS += -framework AVFoundation
        INCLUDEPATH += /System/Library/Frameworks/AVFoundation.framework/Versions/Current/Headers
    }
}

DISTFILES += src/qml/icon/orion.svg

linux:!android: {
    QT += dbus

    HEADERS += src/notification/notificationsender.h
    SOURCES +=  src/notification/notificationsender.cpp

    target.path = $$PREFIX/bin
    INSTALLS += target

    _appdata.files = distfiles/Orion.appdata.xml
    _appdata.path = $$PREFIX/share/metainfo

    _desktop.files = distfiles/Orion.desktop
    _desktop.path = $$PREFIX/share/applications

    _icon.files = distfiles/orion.svg
    _icon.path = $$PREFIX/share/icons/hicolor/scalable/apps

    INSTALLS += _appdata _desktop _icon
}

RESOURCES += \
    src/qml/qml.qrc

# Copies the given files to the destination directory
defineTest(copyToDestdir) {
    files = $$1

    for(FILE, files) {
        DDIR = $$DESTDIR

        # Replace slashes in paths with backslashes for Windows
        win32:FILE ~= s,/,\\,g
        win32:DDIR ~= s,/,\\,g

        QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$FILE) $$quote($$DDIR) $$escape_expand(\\n\\t)
    }

    export(QMAKE_POST_LINK)
}

win32: {
    RC_ICONS = distfiles/orion.ico

    EXTRA_BINFILES = $$PWD/libs/ssleay32.dll \
                    $$PWD/libs/libeay32.dll

    mpv: EXTRA_BINFILES += $$PWD/libs/mpv-1.dll

    EXTRA_BINFILES_WIN = $${EXTRA_BINFILES}
    EXTRA_BINFILES_WIN ~= s,/,\\,g
    DESTDIR_WIN = $$OUT_PWD/$(OBJECTS_DIR)
    DESTDIR_WIN ~= s,/,\\,g
    for(FILE,EXTRA_BINFILES_WIN){
                QMAKE_POST_LINK +=$$quote(cmd /c copy /y \"$${FILE}\" \"$${DESTDIR_WIN}\"$$escape_expand(\n\t))
    }

    CONFIG += enableVersionCheck
}

macx: {
    QMAKE_INFO_PLIST = distfiles/Info.plist

    ICON = $$PWD/distfiles/orion.icns
    LIBS += -framework Foundation -framework CoreServices
    LIBS += -framework AppKit

    HEADERS += src/notification/notificationsender.h
    #inc as obj-c
    OBJECTIVE_SOURCES +=

    INCLUDEPATH += /System/Library/Frameworks/Foundation.framework/Versions/C/Headers
    INCLUDEPATH += /System/Library/Frameworks/AppKit.framework/Versions/C/Headers
    INCLUDEPATH += /usr/local/include

    OBJECTIVE_SOURCES += \
        src/notification/notificationsender.mm

    CONFIG += enableVersionCheck
}

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

# Set CONFIG+=enableVersionCheck if you want Orion to check version on startup.
# By default enabled on mac/win
enableVersionCheck {
    DEFINES += VERSION_CHECK_ENABLED
}

