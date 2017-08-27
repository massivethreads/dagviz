TEMPLATE = lib
TARGET = dagrenderer
INCLUDEPATH += .

# Qt Core and Qt GUI are include by default
QT += widgets
#CONFIG += debug

LIBS += $$PWD/../dagmodel.so -ldr

# Input
HEADERS += dagrenderer.h 
SOURCES += dagrenderer.cpp
