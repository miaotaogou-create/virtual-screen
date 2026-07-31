QT += core gui widgets
CONFIG += c++14
TARGET = VirtualScreen
TEMPLATE = app

# 无控制台黑框
CONFIG -= console
CONFIG += windows

DEFINES += UNICODE _UNICODE
DEFINES += QT_DEPRECATED_WARNINGS
QMAKE_CXXFLAGS += /utf-8
QMAKE_CFLAGS += /utf-8

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/TitleBar.cpp \
    src/ChromeButton.cpp \
    src/PreviewPane.cpp \
    src/SettingsPanel.cpp \
    src/WinDisplay.cpp \
    src/VddService.cpp \
    src/AppConfig.cpp \
    src/Elevate.cpp

HEADERS += \
    src/MainWindow.h \
    src/TitleBar.h \
    src/ChromeButton.h \
    src/PreviewPane.h \
    src/SettingsPanel.h \
    src/WinDisplay.h \
    src/VddService.h \
    src/AppConfig.h \
    src/Elevate.h

LIBS += -luser32 -lgdi32 -lshell32 -ladvapi32

RC_ICONS = resources/VirtualScreen.ico
RESOURCES += resources/app.qrc

DESTDIR = $$PWD/dist
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui
