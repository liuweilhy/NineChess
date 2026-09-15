#-------------------------------------------------
#
# Project created by QtCreator 2015-11-03T22:30:34
#
#-------------------------------------------------

QT       += core gui \
            multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = NineChess
TEMPLATE = app

# c++17：核心用到 std::make_unique（C++14），且 qmake 只识别小写的 c++NN，
# 原来的 C++11 既不会被识别、又会与 MSVC/GCC 的默认标准产生歧义，故作显式指定。
CONFIG += c++17 \
    warn_off
INCLUDEPATH += src

win32-msvc* {
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
}

SOURCES += \
    src/main.cpp \
    src/boarditem.cpp \
    src/gamecontroller.cpp \
    src/gamescene.cpp \
    src/gameview.cpp \
    src/ninechess.cpp \
    src/ninechess_ai_ab.cpp \
    src/ninechess_book.cpp \
    src/ninechess_symmetry.cpp \
    src/ninechesswindow.cpp \
    src/pieceitem.cpp \
    src/aithread.cpp
HEADERS  += \
    src/boarditem.h \
    src/gamecontroller.h \
    src/gamescene.h \
    src/gameview.h \
    src/graphicsconst.h \
    src/ninechess_common.h \
    src/ninechess.h \
    src/ninechess_ai_ab.h \
    src/ninechess_book.h \
    src/ninechess_symmetry.h \
    src/ninechess_version.h \
    src/ninechesswindow.h \
    src/pieceitem.h \
    src/aithread.h
FORMS    += \
    ninechesswindow.ui

RESOURCES += \
    ninechesswindow.qrc

# 界面翻译：用 lupdate 生成/更新 .ts，用 lrelease 生成 .qm。
# 生成的 .qm 已随 ninechesswindow.qrc 打包进可执行文件（前缀 /i18n），
# 修改 .ts 后需重新执行 lrelease 才会生效。
TRANSLATIONS += \
    translations/ninechess_zh_CN.ts \
    translations/ninechess_zh_TW.ts \
    translations/ninechess_en.ts \
    translations/ninechess_ja.ts \
    translations/ninechess_ko.ts \
    translations/ninechess_de.ts \
    translations/ninechess_fr.ts \
    translations/ninechess_ru.ts \
    translations/ninechess_es.ts \
    translations/ninechess_pt.ts

DISTFILES += \
    NineChess.rc

RC_FILE += NineChess.rc
