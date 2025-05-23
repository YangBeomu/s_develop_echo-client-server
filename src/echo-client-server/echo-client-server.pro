TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lglog
DESTDIR = ../../bin
OBJECTS_DIR = ../../obj
SOURCES += \
        main.cpp
