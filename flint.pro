#-------------------------------------------------
#
# Project created by QtCreator 2014-08-25T17:03:21
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = flint
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    oscilloscope.cpp

HEADERS  += mainwindow.h \
    oscilloscope.h

FORMS    += mainwindow.ui

RESOURCES += \
    resources.qrc
