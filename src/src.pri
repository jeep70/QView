SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/openwith.cpp \
    $$PWD/qvgraphicsview.cpp \
    $$PWD/qvoptionsdialog.cpp \
    $$PWD/qvapplication.cpp \
    $$PWD/qvaboutdialog.cpp \
    $$PWD/qvrenamedialog.cpp \
    $$PWD/qvwelcomedialog.cpp \
    $$PWD/qvinfodialog.cpp \
    $$PWD/qvimagecore.cpp \
    $$PWD/qvshortcutdialog.cpp \
    $$PWD/actionmanager.cpp \
    $$PWD/logicalpixelfitter.cpp \
    $$PWD/settingsmanager.cpp \
    $$PWD/shortcutmanager.cpp

!qv_disable_online_version_check:SOURCES += $$PWD/updatechecker.cpp

macx:!CONFIG(NO_COCOA):SOURCES += $$PWD/qvcocoafunctions.mm
win32:!CONFIG(NO_WIN32):SOURCES += $$PWD/qvwin32functions.cpp
linux:!CONFIG(NO_X11):SOURCES += $$PWD/qvlinuxx11functions.cpp

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/openwith.h \
    $$PWD/qvgraphicsview.h \
    $$PWD/qvoptionsdialog.h \
    $$PWD/qvapplication.h \
    $$PWD/qvaboutdialog.h \
    $$PWD/qvrenamedialog.h \
    $$PWD/qvwelcomedialog.h \
    $$PWD/qvinfodialog.h \
    $$PWD/qvimagecore.h \
    $$PWD/qvshortcutdialog.h \
    $$PWD/actionmanager.h \
    $$PWD/logicalpixelfitter.h \
    $$PWD/settingsmanager.h \
    $$PWD/shortcutmanager.h

!qv_disable_online_version_check:HEADERS += $$PWD/updatechecker.h


macx:!CONFIG(NO_COCOA):HEADERS += $$PWD/qvcocoafunctions.h
win32:!CONFIG(NO_WIN32):HEADERS += $$PWD/qvwin32functions.h
linux:!CONFIG(NO_X11):HEADERS += $$PWD/qvlinuxx11functions.h

FORMS += \
        $$PWD/mainwindow.ui \
    $$PWD/qvopenwithdialog.ui \
    $$PWD/qvoptionsdialog.ui \
    $$PWD/qvaboutdialog.ui \
    $$PWD/qvwelcomedialog.ui \
    $$PWD/qvinfodialog.ui \
    $$PWD/qvshortcutdialog.ui
