#-------------------------------------------------
#
# Project created by QtCreator 2015-04-07T16:50:10
#
#-------------------------------------------------

QT       -= core gui

TARGET = feature_changeset
TEMPLATE = lib
CONFIG += staticlib

SOURCES += featurechangeset.cpp \
    featurechangeset_upload.cpp

HEADERS += featurechangeset.h
unix {
    target.path = /usr/lib
    INSTALLS += target
}
