TEMPLATE = lib
TARGET = dagrenderer
INCLUDEPATH += .
INCLUDEPATH += /home/zanton/local/Qt5.9.1/5.9.1/gcc_64/include

# Qt Core and Qt GUI are include by default
QT += widgets
#CONFIG += debug

LIBS += $$PWD/../dagmodel.so -ldr

# Input
HEADERS += dagrenderer.h 
SOURCES += dagrenderer.cpp
