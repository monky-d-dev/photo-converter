QT += core gui qml quick quickcontrols2 concurrent
CONFIG += c++17 release
TARGET = photo-converter
TEMPLATE = app
HEADERS += src/imaging.h src/batch.h src/backend.h src/previewprovider.h src/appearance.h
SOURCES += src/main.cpp src/imaging.cpp src/batch.cpp src/backend.cpp src/previewprovider.cpp src/appearance.cpp
RESOURCES += src/resources.qrc
win32-msvc*:QMAKE_CXXFLAGS += /utf-8
win32 {
    RC_ICONS = $$PWD/src/photo-converter.ico
    CONFIG -= console
    QMAKE_TARGET_PRODUCT = Photo Converter
    QMAKE_TARGET_DESCRIPTION = Composiciones de graduacion por lotes
    QMAKE_TARGET_COMPANY = Photo Converter
    QMAKE_TARGET_COPYRIGHT = Copyright 2026
    VERSION = 0.1.0
}
