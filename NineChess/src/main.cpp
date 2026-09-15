#include "ninechesswindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 应用级字体回退：QToolTip 等顶层控件不继承主窗口字体，须在创建窗口前设置
    NineChessWindow::applyApplicationFontFallback();
    NineChessWindow w;
    w.show();
    return a.exec();
}
