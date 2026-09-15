#pragma once

#include <QtWidgets/QMainWindow>
#include <QTextStream>
#include <QStringListModel>
#include <QFile>
#include <QTimer>
#include "ui_ninechesswindow.h"

class GameScene;
class GameController;
class QMenu;
class QTranslator;

class NineChessWindow : public QMainWindow
{
    Q_OBJECT

public:
    NineChessWindow(QWidget *parent = nullptr);
    ~NineChessWindow();

    // 界面字体：为 .ui 里写死的中文字体补上跨平台的回退家族。
    // 应用级字体必须在创建窗口之前设置——QToolTip 之类的顶层控件不继承主窗口
    // 字体，只有应用字体能覆盖到，因此由 main() 调用本静态方法。
    static void applyApplicationFontFallback();

protected:
    bool eventFilter(QObject * watched, QEvent * event);
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);
    // 语言切换时 Qt 会投递 LanguageChange 事件，在这里统一刷新界面文本
    void changeEvent(QEvent *event);

private slots:
    // 初始化
    void initialize();
    // 动态增加的菜单栏动作的槽函数
    void actionRules_triggered();
    // 更新规则标签
    void ruleInfo();
    // 语言菜单项被选中
    void actionLanguageChanged();
    // 自动运行定时处理函数
    void onAutoRunTimeOut(QPrivateSignal signal);

    // 下面是各动作的槽函数
    // 注释掉的是已在UI管理器或主窗口初始化函数中连接好的
    void on_actionNew_N_triggered();
    void on_actionOpen_O_triggered();
    void on_actionSave_S_triggered();
    void on_actionSaveAs_A_triggered();
    //void on_actionExit_X_triggered();
    void on_actionEdit_E_toggled(bool arg1);
    //void on_actionFlip_F_triggered();
    //void on_actionMirror_M_triggered();
    //void on_actionTurnRight_R_triggered();
    //void on_actionTurnLeftt_L_triggered();
    void on_actionInvert_I_toggled(bool arg1);
    // 前后招的公共槽
    void on_actionRowChange();
    // 控制器浏览行号/棋谱行数变化后的统一同步槽（选中行 + 导航键状态）
    void onBrowseRowChanged(int row);
    void on_actionAutoRun_A_toggled(bool arg1);
    //void on_actionGiveUp_G_triggered();
    void on_actionLimited_T_triggered();
    void on_actionLocal_L_triggered();
    void on_actionInternet_I_triggered();
    void on_actionEngine_E_triggered();
    //void on_actionEngine1_R_toggled(bool arg1);
    //void on_actionEngine2_T_toggled(bool arg1);
    //void on_actionSetting_O_triggered();
    //void on_actionToolBar_T_toggled(bool arg1);
    //void on_actionDockBar_D_toggled(bool arg1);
    //void on_actionSound_S_toggled(bool arg1);
    //void on_actionAnimation_A_toggled(bool arg1);
    void on_actionViewHelp_V_triggered();
    void on_actionWeb_W_triggered();
    void on_actionAbout_A_triggered();

private:
    // ==================== 界面语言 ====================
    // 语言相关的状态与行为都收在本类内：翻译器由本窗口持有并随窗口销毁，
    // 不再有模块级全局状态。语言清单、Locale 归一化等纯查表函数放在
    // ninechesswindow.cpp 的文件内静态函数中（不进入头文件）。
    //
    // 装载/切换界面语言。必须在 ui.setupUi() 之前调用：
    // 优先沿用上次选择（QSettings），首次运行按本机默认语言。
    void initializeLanguage();
    // 装载指定语言；找不到翻译文件时返回 false 并保持当前语言不变
    bool applyLanguage(const QString &code);
    // 建立“选项 -> 设置 -> 语言”子菜单
    void createLanguageMenu();
    // 按当前语言刷新所有界面文本（含动态创建的菜单项）
    void retranslateUi();

    // 界面文件
    Ui::NineChessWindowClass ui;
    // 把当前对局写入已关联的棋谱文件（含规则/限时限步头部；终局时含胜负平结果）
    bool writeGameRecord();
    // 视图场景
    GameScene *scene;
    // 控制器
    GameController *game;
    // 动态增加的菜单栏动作列表
    QList <QAction *> ruleActionList;
    // 游戏的规则号，涉及菜单项和对话框，所以要有
    int ruleNo;
    // 文件
    QFile file;
    // 定时器
    QTimer autoRunTimer;
    // 首次显示前用最大宽度钳住棋谱列表的sizeHint（决定停靠栏初始宽度），显示后解除
    bool listWidthClamped = true;
    // 棋谱模型新插入行尚未写入数据的标志（追加招法后自动选中最后一行用）
    bool newManualRow = false;
    // 语言子菜单（挂在“选项 -> 设置”项下）
    QMenu *languageMenu = nullptr;
    // 本程序翻译与 Qt 自带翻译（qtbase_*.qm）；随窗口一起销毁
    QTranslator *appTranslator = nullptr;
    QTranslator *qtTranslator = nullptr;
    // 当前生效的语言代码（如 "zh_CN"、"en"）
    QString languageCode;
};

