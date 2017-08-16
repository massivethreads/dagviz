TEMPLATE = lib
TARGET = dagrenderer
INCLUDEPATH += .
INCLUDEPATH += /home/zanton/local/Qt5.9.1/5.9.1/gcc_64/include

LIBS += $$PWD/../dagmodel.so -ldr

# Input
HEADERS += dagrenderer.h 
SOURCES += dagrenderer.cpp
