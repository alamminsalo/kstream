#-------------------------------------------------
#
# Orion: Twitch Client written in C++/QML
#
#-------------------------------------------------

QT     += gui qml quick network widgets

TARGET = orion

VERSION = 1.5.1
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
    src/power/power.cpp \
    src/systray.cpp \
    src/util/runguard.cpp \
    src/customapp.cpp \
    src/model/vod.cpp \
    src/model/vodlistmodel.cpp \
    src/model/vodmanager.cpp \
    src/notification/notificationmanager.cpp \
    src/model/ircchat.cpp \
    src/model/imageprovider.cpp


HEADERS  += src/model/channel.h \
    src/model/channelmanager.h \
    src/util/fileutils.h \
    src/network/networkmanager.h \
    src/model/game.h \
    src/util/jsonparser.h \
    src/model/channellistmodel.h \
    src/model/gamelistmodel.h \
    src/util/m3u8parser.h \
    src/power/power.h \
    src/systray.h \
    src/util/runguard.h \
    src/customapp.h \
    src/model/vod.h \
    src/model/vodlistmodel.h \
    src/model/vodmanager.h \
    src/network/urls.h \
    src/notification/notificationmanager.h \
    src/model/ircchat.h \
    src/model/imageprovider.h \
    src/network/httpserver.h

#Backend for player, uses mpv as default
!qtav: !multimedia {
    CONFIG *= mpv
}

mpv {
    message(Selecting mpv as backend)
    #DEFINES += DEBUG_LIBMPV
    DEFINES += MPV_PLAYER
    SOURCES +=  src/player/mpvrenderer.cpp \
                src/player/mpvobject.cpp

    HEADERS +=  src/player/mpvobject.h \
                src/player/mpvrenderer.h

    unix:!macx: LIBS += -lmpv
    win32: LIBS += -L$$PWD/libs -lmpv
    macx: LIBS += -L$$PWD/../../../../usr/local/Cellar/mpv/0.25.0/lib -lmpv
}

qtav {
    message(Selecting qtav as backend)
    QT += av
    DEFINES += QTAV_PLAYER
}

multimedia {
    message(Selecting qt multimedia as backend)
    QT += multimedia
    DEFINES += MULTIMEDIA_PLAYER
    macx: {
        LIBS += -framework AVFoundation
        INCLUDEPATH += /System/Library/Frameworks/AVFoundation.framework/Versions/Current/Headers
    }
}

QMAKE_CXXFLAGS += -Wall -O2

CONFIG += c++11
#CONFIG += console

DISTFILES += src/qml/icon/orion.svg \
    src/qml/player.html

unix:!macx: {
    QT += dbus

    HEADERS += src/notification/notificationsender.h
    SOURCES +=  src/notification/notificationsender.cpp

    target.path = /usr/bin
    INSTALLS += target

    _desktop.files = distfiles/Orion.desktop
    _desktop.path = /usr/local/share/applications

    _icon.files = distfiles/orion.svg
    _icon.path = /usr/local/share/icons

    INSTALLS += _desktop _icon
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

}

#CONFIG(release): DEFINES += QT_NO_DEBUG_OUTPUT

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
}

OBJECTIVE_SOURCES += \
    src/notification/notificationsender.mm


