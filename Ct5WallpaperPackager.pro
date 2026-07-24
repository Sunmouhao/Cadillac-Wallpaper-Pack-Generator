QT += core gui widgets

CONFIG += c++17
CONFIG -= app_bundle

msvc:QMAKE_CXXFLAGS += /utf-8
win32 {
    ASTCENC_NATIVE = $$PWD/bin/x64/Release/astcenc-native.exe
    exists($$ASTCENC_NATIVE) {
        QMAKE_POST_LINK += $$quote(cmd /c copy /Y "$$replace(ASTCENC_NATIVE, /, \\)" "$$replace(DESTDIR, /, \\)\\astcenc-native.exe")
    }
}

TEMPLATE = app
TARGET = Ct5WallpaperPackager

SOURCES += \
    CropPreviewWidget.cpp \
    main.cpp \
    MainWindow.cpp \
    WallpaperPackager.cpp

HEADERS += \
    CropPreviewWidget.h \
    MainWindow.h \
    WallpaperPackager.h
