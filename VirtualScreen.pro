QT += core gui widgets svg
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
    src/TopologyCanvas.cpp \
    src/PropertiesDrawer.cpp \
    src/SwitchButton.cpp \
    src/CastWindowDialog.cpp \
    src/PresetHubDialog.cpp \
    src/AddDisplayDialog.cpp \
    src/AppAlertDialog.cpp \
    src/SchemeComboBox.cpp \
    src/SettingsPanel.cpp \
    src/WinDisplay.cpp \
    src/VddService.cpp \
    src/AppConfig.cpp \
    src/Elevate.cpp

HEADERS += \
    src/MainWindow.h \
    src/TitleBar.h \
    src/ChromeButton.h \
    src/TopologyCanvas.h \
    src/PropertiesDrawer.h \
    src/SwitchButton.h \
    src/CastWindowDialog.h \
    src/PresetHubDialog.h \
    src/AddDisplayDialog.h \
    src/AppAlertDialog.h \
    src/SchemeComboBox.h \
    src/SettingsPanel.h \
    src/WinDisplay.h \
    src/VddService.h \
    src/AppConfig.h \
    src/Elevate.h

LIBS += -luser32 -lgdi32 -lshell32 -ladvapi32 -lsetupapi -lcfgmgr32

RC_ICONS = resources/VirtualScreen.ico
RESOURCES += resources/app.qrc

DESTDIR = $$PWD/dist
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui
