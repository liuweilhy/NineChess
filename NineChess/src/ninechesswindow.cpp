#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QMap>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QDialog>
#include <QFileDialog>
#include <QButtonGroup>
#include <QPushButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QHelpEvent>
#include <QToolTip>
#include <QPicture>
#include <QDebug>
#include <QDesktopWidget>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QLibraryInfo>
#include <QLocale>
#include <QMenu>
#include <QTranslator>
#include <QVector>
#include "ninechesswindow.h"
#include "gamecontroller.h"
#include "gamescene.h"
#include "ninechess.h"
#include "ninechess_version.h"

namespace {

// ==================== 界面语言 ====================
// 说明：语言清单、Locale 归一化、翻译文件查找属于纯查表逻辑，
// 放在文件内（编译单元内）即可，无需暴露到头文件。

// 一种受支持的语言：code 同时决定翻译文件名后缀（ninechess_<code>.qm）；
// nativeName 是该语言自身的写法，菜单中直接显示，不参与翻译。
struct LanguageOption
{
    const char *code;
    const char *nativeName;
};

// 内置支持的语言。顺序即语言菜单顺序：先中文（简/繁），再英文，
// 之后按使用规模与字母顺序排列。
const LanguageOption kLanguages[] = {
    { "zh_CN", "简体中文" },
    { "zh_TW", "繁體中文" },
    { "en",    "English" },
    { "ja",    "日本語" },
    { "ko",    "한국어" },
    { "de",    "Deutsch" },
    { "fr",    "Français" },
    { "ru",    "Русский" },
    { "es",    "Español" },
    { "pt",    "Português" },
};

const int kLanguageCount = static_cast<int>(sizeof(kLanguages) / sizeof(kLanguages[0]));

// 该语言自身的名称；未知代码原样返回。
QString languageNativeName(const QString &code)
{
    for (int i = 0; i < kLanguageCount; ++i) {
        if (code == QLatin1String(kLanguages[i].code))
            return QString::fromUtf8(kLanguages[i].nativeName);
    }
    return code;
}

bool isSupportedLanguage(const QString &code)
{
    for (int i = 0; i < kLanguageCount; ++i) {
        if (code == QLatin1String(kLanguages[i].code))
            return true;
    }
    return false;
}

// 把任意 Locale 名称归一化为受支持的语言代码；无法识别时返回 "en"。
// 中文会区分简体（zh_CN）与繁体（zh_TW）。
QString normalizeLanguageCode(const QString &localeName)
{
    QString name = localeName;
    name.replace(QLatin1Char('-'), QLatin1Char('_'));

    const QStringList parts = name.split(QLatin1Char('_'), QString::SkipEmptyParts);
    if (parts.isEmpty())
        return QStringLiteral("en");

    const QString lang = parts.at(0).toLower();
    const QString region = parts.size() > 1 ? parts.at(1).toUpper() : QString();

    if (lang == QLatin1String("zh")) {
        // 繁体：显式 Hant 标记，或使用台/港/澳区域；
        // 其余（含 Hans、CN、SG 以及无法判定区域）一律按简体处理。
        if (region.contains(QLatin1String("HANT"))
            || region == QLatin1String("TW")
            || region == QLatin1String("HK")
            || region == QLatin1String("MO")) {
            return QStringLiteral("zh_TW");
        }
        return QStringLiteral("zh_CN");
    }

    if (isSupportedLanguage(lang))
        return lang;

    return QStringLiteral("en");
}

// 本机默认语言代码。
//   - Windows：QLocale::system() 读系统区域设置；
//   - Debian/Linux：QLocale 读 LC_ALL / LC_MESSAGES / LANG，另外下面还会
//     在 QLocale 退化为 "C"/"POSIX" 时显式读一次环境变量做兜底。
QString systemLanguageCode()
{
    QString name = QLocale::system().name();

    if (name.isEmpty() || name == QLatin1String("C") || name == QLatin1String("POSIX")) {
        static const char *const kEnvNames[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
        for (const char *envName : kEnvNames) {
            const QByteArray value = qgetenv(envName);
            if (value.isEmpty())
                continue;

            // 去掉 "zh_CN.UTF-8@variant" 中的编码与修饰部分
            QString candidate = QString::fromLocal8Bit(value);
            const int dot = candidate.indexOf(QLatin1Char('.'));
            if (dot >= 0)
                candidate.truncate(dot);
            const int at = candidate.indexOf(QLatin1Char('@'));
            if (at >= 0)
                candidate.truncate(at);
            if (candidate.isEmpty()
                || candidate == QLatin1String("C")
                || candidate == QLatin1String("POSIX")) {
                continue;
            }

            name = candidate;
            break;
        }
    }

    return normalizeLanguageCode(name);
}

// 本程序翻译文件的搜索目录：优先打包进可执行文件的资源（ninechesswindow.qrc），
// 其次可执行文件旁的 translations 目录（便于替换或新增语言而无需重编译）。
QStringList translationSearchDirs()
{
    QStringList dirs;
    dirs << QStringLiteral(":/i18n");
    dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    dirs << QCoreApplication::applicationDirPath();
    return dirs;
}

// 模型层（ninechess.cpp / gamecontroller.cpp）中的规则名、规则说明和状态栏提示
// 不是 tr() 字面量调用（规则表是常量数组，提示是运行期拼装的），lupdate 无法
// 自动提取，因此在这里用 QT_TRANSLATE_NOOP 在 "NineChess" 上下文下登记一次。
// 这里的文本必须与模型层保持一致，否则翻译不会命中。
void registerModelTranslatableTexts()
{
    // ---- 规则名（NineChess::rules[i].name）----
    QT_TRANSLATE_NOOP("NineChess", "成三棋");
    QT_TRANSLATE_NOOP("NineChess", "打三棋(12连棋)");
    QT_TRANSLATE_NOOP("NineChess", "九连棋");
    QT_TRANSLATE_NOOP("NineChess", "莫里斯九子棋");

    // ---- 规则说明（NineChess::rules[i].description）----
    QT_TRANSLATE_NOOP("NineChess",
        "1. 双方各9颗子，开局依次摆子；\n"
        "2. 凡出现三子相连，就提掉对手一子；\n"
        "3. 不能提对手的“三连”子，除非无子可提；\n"
        "4. 同时出现两个“三连”只能提一子；\n"
        "5. 摆完后依次走子，每次只能往相邻位置走一步；\n"
        "6. 把对手棋子提到少于3颗时胜利；\n"
        "7. 走棋阶段不能行动（被“闷”）算负。");
    QT_TRANSLATE_NOOP("NineChess",
        "1. 双方各12颗子，棋盘有斜线；\n"
        "2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；\n"
        "3. 摆棋阶段，摆满棋盘算先手负；\n"
        "4. 走棋阶段，后摆棋的一方先走；\n"
        "5. 一步出现几个“三连”就可以提几个子；\n"
        "6. 其它规则与成三棋基本相同。");
    QT_TRANSLATE_NOOP("NineChess",
        "1. 规则与成三棋基本相同，只是它的棋子有序号，\n"
        "2. 相同序号、位置的“三连”不能重复提子；\n"
        "3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；\n"
        "4. 一步出现几个“三连”就可以提几个子。");
    QT_TRANSLATE_NOOP("NineChess",
        "规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。");

    // ---- 状态栏提示（NineChess::m_tip 模板）----
    QT_TRANSLATE_NOOP("NineChess", "未开局");
    QT_TRANSLATE_NOOP("NineChess", "玩家1");
    QT_TRANSLATE_NOOP("NineChess", "玩家2");
    QT_TRANSLATE_NOOP("NineChess", "轮到%1落子，剩余%2子");
    QT_TRANSLATE_NOOP("NineChess", "轮到%1去子，需去%2子");
    QT_TRANSLATE_NOOP("NineChess", "轮到%1选子移动");
    QT_TRANSLATE_NOOP("NineChess", "轮到%1落子");
    QT_TRANSLATE_NOOP("NineChess", "平局。");
    QT_TRANSLATE_NOOP("NineChess", "恭喜玩家1获胜！");
    QT_TRANSLATE_NOOP("NineChess", "恭喜玩家2获胜！");
    QT_TRANSLATE_NOOP("NineChess", "玩家1认负，恭喜玩家2获胜！");
    QT_TRANSLATE_NOOP("NineChess", "玩家2认负，恭喜玩家1获胜！");
    QT_TRANSLATE_NOOP("NineChess", "摆满棋盘，恭喜玩家2获胜！");
    QT_TRANSLATE_NOOP("NineChess", "摆满棋盘，双方平局。");
    QT_TRANSLATE_NOOP("NineChess", "玩家1无子可走，恭喜玩家2获胜！");
    QT_TRANSLATE_NOOP("NineChess", "玩家2无子可走，恭喜玩家1获胜！");
    QT_TRANSLATE_NOOP("NineChess", "双方均无子可走，平局。");
    QT_TRANSLATE_NOOP("NineChess", "玩家1超时，恭喜玩家2获胜！");
    QT_TRANSLATE_NOOP("NineChess", "玩家2超时，恭喜玩家1获胜！");
}

// ==================== 手工指定路径等辅助 ====================

QString defaultManualDirectory()
{
    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documentsPath.isEmpty())
        return documentsPath;

    return QDir::homePath();
}

QString lastManualDirectory()
{
    QSettings settings(QStringLiteral("NineChess"), QStringLiteral("NineChess"));
    const QString directory = settings.value(QStringLiteral("manual/lastDirectory"), defaultManualDirectory()).toString();
    if (!directory.isEmpty() && QDir(directory).exists())
        return directory;

    return defaultManualDirectory();
}

void saveLastManualDirectory(const QString &path)
{
    const QString directory = QFileInfo(path).absoluteDir().absolutePath();
    if (directory.isEmpty())
        return;

    QSettings settings(QStringLiteral("NineChess"), QStringLiteral("NineChess"));
    settings.setValue(QStringLiteral("manual/lastDirectory"), directory);
}

// ==================== 界面字体 ====================

// .ui 里只写了设计时使用的中文字体 Microsoft YaHei，它是 Windows 独有的，
// 且不含韩文字形：Windows 上日韩文只能靠系统隐式替换，Linux/macOS 上更是完全
// 没有这个字体（缺 CJK 字体时会整片显示方框）。因此在其后追加一份跨平台的
// 回退家族表——首选家族仍是 Microsoft YaHei，Windows 外观不变；找不到字体或
// 缺字形时按表回退（日/韩文即命中各自字体），最后落到通用 sans-serif 交给
// 平台的字体配置（fontconfig）处理。
QStringList uiFontFamilies(const QFont &base)
{
    QStringList families;
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    families = base.families();
#endif
    if (families.isEmpty())
        families << base.family();

    const QStringList candidates{
        QStringLiteral("Microsoft YaHei UI"),   // Windows：雅黑的界面变体
        QStringLiteral("Microsoft JhengHei"),   // Windows：繁体中文
        QStringLiteral("Meiryo"),               // Windows：日文
        QStringLiteral("Malgun Gothic"),        // Windows：韩文
        QStringLiteral("Noto Sans CJK SC"),     // 思源黑体：简体
        QStringLiteral("Noto Sans CJK TC"),     // 思源黑体：繁体
        QStringLiteral("Noto Sans CJK JP"),     // 思源黑体：日文
        QStringLiteral("Noto Sans CJK KR"),     // 思源黑体：韩文
        QStringLiteral("Source Han Sans SC"),
        QStringLiteral("WenQuanYi Micro Hei"),
        QStringLiteral("WenQuanYi Zen Hei"),
        QStringLiteral("Droid Sans Fallback"),
        QStringLiteral("PingFang SC"),          // macOS
        QStringLiteral("Hiragino Sans GB"),
        QStringLiteral("Heiti SC"),
        QStringLiteral("sans-serif"),           // 通用兜底
    };

    for (const QString &candidate : candidates)
    {
        if (!families.contains(candidate))
            families << candidate;
    }

    return families;
}

// 把回退家族写回字体对象。家族列表是 Qt 5.13 才有的 API，更老的版本只能沿用
// .ui 里写死的单一家族（此时字体替换仍交由平台字体库处理，与改动前一致）。
void applyFamiliesToFont(QFont &font, const QStringList &families)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    font.setFamilies(families);
#else
    Q_UNUSED(font)
    Q_UNUSED(families)
#endif
}

// 应用到主窗口：其子控件与子对话框沿父链继承该字体
void applyUiFont(QWidget *widget)
{
    QFont font = widget->font();
    applyFamiliesToFont(font, uiFontFamilies(font));
    widget->setFont(font);
}

}

// 应用到应用级字体：QToolTip 等顶层控件不继承主窗口字体，只有应用字体能覆盖到。
void NineChessWindow::applyApplicationFontFallback()
{
    QFont appFont = QApplication::font();
    applyFamiliesToFont(appFont, uiFontFamilies(appFont));
    QApplication::setFont(appFont);
}

NineChessWindow::NineChessWindow(QWidget *parent)
    : QMainWindow(parent),
    scene(nullptr),
    game(nullptr),
    ruleNo(-1),
    autoRunTimer(this)
{
    // 语言必须在 setupUi 之前装载：否则 .ui 里的初始文本会停留在源语言（中文）
    initializeLanguage();

    ui.setupUi(this);
    // 给 .ui 指定的中文字体补上跨平台回退家族（子控件与子对话框沿父链继承）
    applyUiFont(this);
    // 标题栏显示版本号（版本号唯一来源见 ninechess_version.h）
    setWindowTitle(tr("九连棋 v%1").arg(QString::fromLatin1(NINECHESS_VERSION_SHORT)));
    //去掉标题栏
    //setWindowFlags(Qt::FramelessWindowHint);
    //设置透明(窗体标题栏不透明,背景透明，如果不去掉标题栏，背景就变为黑色)
    //setAttribute(Qt::WA_TranslucentBackground);
    //设置全体透明度系数
    //setWindowOpacity(0.7);

    // 设置场景
    scene = new GameScene(this);
    // 设置场景尺寸大小为棋盘大小的1.08倍
    scene->setSceneRect(-600.0 * 0.54, -600.0 * 0.54, 600.0 * 1.08, 600.0 * 1.08);

    // 初始化各个控件

    // 关联视图和场景
    ui.gameView->setScene(scene);
    // 视图反走样
    ui.gameView->setRenderHint(QPainter::Antialiasing, true);
    // 视图反锯齿
    ui.gameView->setRenderHint(QPainter::Antialiasing);

    // 因功能限制，使部分功能不可用，将来再添加
    ui.actionInternet_I->setDisabled(true);

    // “选项 -> 设置”原本是不可用的占位项，现在把语言子菜单挂在它下面
    createLanguageMenu();

    // 初始化游戏规则菜单
    ui.menu_R->installEventFilter(this);

    // 关联自动运行定时器
    connect(&autoRunTimer, SIGNAL(timeout()),
        this, SLOT(onAutoRunTimeOut()));

    // 主窗口居中显示
    QRect deskTopRect = qApp->desktop()->availableGeometry();
    int unitw = (deskTopRect.width() - width())/2;
    int unith = (deskTopRect.height() - height())/2;
    this->move(unitw,unith);

    // 游戏初始化
    initialize();
}

NineChessWindow::~NineChessWindow()
{
    if (game) {
        game->disconnect();
        game->deleteLater();
    }
    qDeleteAll(ruleActionList);

    // 翻译器由本窗口持有，销毁前先从 QCoreApplication 卸载，
    // 避免应用继续持有已释放的对象
    if (appTranslator) {
        QCoreApplication::removeTranslator(appTranslator);
        delete appTranslator;
        appTranslator = nullptr;
    }
    if (qtTranslator) {
        QCoreApplication::removeTranslator(qtTranslator);
        delete qtTranslator;
        qtTranslator = nullptr;
    }
}

// 装载界面语言：注册模型层文本钩子并决定启动语言。
// 必须在 ui.setupUi() 之前调用，否则 .ui 里的初始文本会是源语言（中文）。
void NineChessWindow::initializeLanguage()
{
    // 登记模型层的规则名/规则说明/状态栏提示，供 lupdate 提取
    registerModelTranslatableTexts();

    // 纯模型层不依赖 Qt，其提示文本通过钩子参与翻译。
    // 钩子无捕获，进程内注册一次即可。
    NineChess::setTextTranslator([](const std::string &source) {
        return QCoreApplication::translate("NineChess", source.c_str()).toStdString();
    });

    // 优先沿用上次选择的语言；首次运行（或未记录）时按本机默认语言启动
    QSettings settings(QStringLiteral("NineChess"), QStringLiteral("NineChess"));
    QString code = settings.value(QStringLiteral("ui/language")).toString();
    if (code.isEmpty())
        code = systemLanguageCode();

    if (!applyLanguage(code) && code != QLatin1String("en"))
        applyLanguage(QStringLiteral("en"));
}

bool NineChessWindow::applyLanguage(const QString &code)
{
    const QString normalized = normalizeLanguageCode(code);
    const QString fileName = QStringLiteral("ninechess_") + normalized;

    // 先把新翻译读进来；全部失败时保持当前语言不动
    QTranslator *newAppTranslator = new QTranslator;
    bool loaded = false;
    const QStringList dirs = translationSearchDirs();
    for (const QString &dir : dirs) {
        if (newAppTranslator->load(fileName, dir)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        // 资源路径或 .qm 缺失都会走到这里；打印出来避免"选了语言却没反应"无从排查
        // （资源内路径应为 :/i18n/ninechess_<code>.qm，见 ninechesswindow.qrc 的 alias）
        qWarning("NineChess: cannot load translation '%s' (searched :/i18n and application dirs)",
                 qPrintable(fileName));
        delete newAppTranslator;
        return false;
    }

    // Qt 自带控件的文本（标准对话框按钮等）由 qtbase 翻译提供；
    // 并非每种语言都有，缺失时忽略即可。
    QTranslator *newQtTranslator = new QTranslator;
    if (!newQtTranslator->load(QStringLiteral("qtbase_") + normalized,
            QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        delete newQtTranslator;
        newQtTranslator = nullptr;
    }

    // 先卸载再销毁旧翻译器（QCoreApplication 不能持有已释放的对象）；
    // removeTranslator 会向所有窗口投递 LanguageChange 事件。
    if (appTranslator) {
        QCoreApplication::removeTranslator(appTranslator);
        delete appTranslator;
    }
    if (qtTranslator) {
        QCoreApplication::removeTranslator(qtTranslator);
        delete qtTranslator;
    }

    appTranslator = newAppTranslator;
    qtTranslator = newQtTranslator;
    languageCode = normalized;

    // 后安装的翻译器优先查找，因此本程序翻译放在最后安装，
    // 以便覆盖 Qt 自带翻译中的同名条目。
    if (qtTranslator)
        QCoreApplication::installTranslator(qtTranslator);
    QCoreApplication::installTranslator(appTranslator);

    return true;
}

// 建立“选项 -> 设置 -> 语言”子菜单：每种支持的语言一个可勾选项，
// 勾选后立即切换界面语言并记入 QSettings，下次启动沿用。
void NineChessWindow::createLanguageMenu()
{
    languageMenu = new QMenu(tr("语言"), this);

    QActionGroup *languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);

    for (int i = 0; i < kLanguageCount; ++i) {
        const QString code = QString::fromLatin1(kLanguages[i].code);
        // 菜单项始终用该语言自身的写法，避免"用当前语言翻译语言名"的歧义
        QAction *action = languageMenu->addAction(languageNativeName(code));
        action->setCheckable(true);
        action->setChecked(code == languageCode);
        action->setData(code);
        languageGroup->addAction(action);
        connect(action, &QAction::triggered, this, &NineChessWindow::actionLanguageChanged);
    }

    // 原“设置”是不可用的占位项，这里让它承载语言子菜单
    ui.actionSetting_O->setMenu(languageMenu);
    ui.actionSetting_O->setEnabled(true);
}

void NineChessWindow::actionLanguageChanged()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    const QString code = action->data().toString();
    if (code == languageCode)
        return;

    if (!applyLanguage(code)) {
        // 翻译文件缺失：恢复勾选到当前语言，不做任何切换
        if (languageMenu) {
            for (QAction *item : languageMenu->actions())
                item->setChecked(item->data().toString() == languageCode);
        }
        return;
    }

    // applyLanguage 已投递 LanguageChange 事件，界面文本由 changeEvent 统一刷新
    QSettings settings(QStringLiteral("NineChess"), QStringLiteral("NineChess"));
    settings.setValue(QStringLiteral("ui/language"), code);
}

// 按当前语言刷新所有界面文本。.ui 中的静态文本由 ui.retranslateUi 处理，
// 这里补充窗口标题、动态规则菜单和状态栏等运行期生成的文本。
void NineChessWindow::retranslateUi()
{
    ui.retranslateUi(this);

    setWindowTitle(tr("九连棋 v%1").arg(QString::fromLatin1(NINECHESS_VERSION_SHORT)));

    if (game) {
        // 规则菜单为动态创建，需按新语言重建名称与提示
        const QMap<int, QStringList> actions = game->getActions();
        for (QAction *ruleAction : ruleActionList) {
            const auto it = actions.constFind(ruleAction->data().toInt());
            if (it != actions.constEnd()) {
                ruleAction->setText(it.value().at(0));
                ruleAction->setToolTip(it.value().at(1));
            }
        }
        // 限时限步标签与规则提示
        ruleInfo();
        // 状态栏提示由模型生成，按新语言重新取一次
        game->refreshText();
    }

    if (languageMenu)
        languageMenu->setTitle(tr("语言"));
}

void NineChessWindow::closeEvent(QCloseEvent *event)
{
    if (file.isOpen())
        file.close();
    // 取消自动运行
    ui.actionAutoRun_A->setChecked(false);
    //qDebug() << "closed";
    QMainWindow::closeEvent(event);
}

void NineChessWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 停靠栏初始宽度由内容控件的sizeHint决定（QListView缺省hint为256x192，偏宽），
    // 显示前已在ui里用maximumWidth=128钳住列表，使首次布局时停靠栏保持较窄。
    // 注意不能在showEvent里立即解除钳制：首次布局激活可能尚未完成（解除动作
    // 本身也会触发重新布局），布局会按256宽的缺省sizeHint把停靠栏重新撑宽，
    // 表现为打开窗口后停靠栏先窄后宽地抖动。
    // 因此推迟到事件队列排空、布局稳定后再解除，并用resizeDocks把当前宽度
    // 固定下来（等效用户手动拖动过一次），后续重新布局不会回弹到缺省hint宽度；
    // 此后停靠栏即可自由拖宽拖窄（单纯setFixedWidth则无法再调节）
    if (listWidthClamped) {
        listWidthClamped = false;
        QTimer::singleShot(0, this, [this] {
            const int width = ui.dockWidget->width();
            ui.listView->setMaximumWidth(QWIDGETSIZE_MAX);
            resizeDocks({ ui.dockWidget }, { width }, Qt::Horizontal);
        });
    }
}

void NineChessWindow::changeEvent(QEvent *event)
{
    // 装载/卸载 QTranslator 时 Qt 会向所有窗口投递 LanguageChange，
    // 界面文本在这里统一重建（含 .ui 生成的 retranslateUi 与动态菜单项）
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();

    QMainWindow::changeEvent(event);
}

bool NineChessWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 重载这个函数只是为了让规则菜单（动态）显示提示
    if (watched == ui.menu_R)
    {
        switch (event->type())
        {
        case QEvent::ToolTip:
            QHelpEvent * he = dynamic_cast <QHelpEvent *> (event);
            QAction *action = ui.menu_R->actionAt(he->pos());
            if (action)
            {
                QToolTip::showText(he->globalPos(), action->toolTip(), this);
                return true;
            }
            break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void NineChessWindow::initialize()
{
    // 初始化函数，仅执行一次
    if (game)
        return;

    // 开辟一个新的游戏控制器
    game = new GameController(*scene, this);

    // 添加新菜单栏动作
    QMap <int, QStringList> actions = game->getActions();
    for (auto i = actions.constBegin(); i != actions.constEnd(); i++) {
        // qDebug() << i.key() << i.value();
        // QMap的key存放int索引值，value存放规则名称和规则提示
        QAction *ruleAction = new QAction(i.value().at(0), this);
        ruleAction->setToolTip(i.value().at(1));
        ruleAction->setCheckable(true);
        // 索引值放在QAction的Data里
        ruleAction->setData(i.key());
        // 添加到动作列表
        ruleActionList.append(ruleAction);
        // 添加到“规则”菜单
        ui.menu_R->addAction(ruleAction);
        connect(ruleAction, SIGNAL(triggered()),
            this, SLOT(actionRules_triggered()));
    }

    // 关联主窗口动作信号和控制器的槽
    connect(ui.actionGiveUp_G, SIGNAL(triggered()),
        game, SLOT(giveUp()));
    connect(ui.actionEngine1_T, SIGNAL(toggled(bool)),
        game, SLOT(setEngine1(bool)));
    connect(ui.actionEngine2_R, SIGNAL(toggled(bool)),
        game, SLOT(setEngine2(bool)));
    connect(ui.actionSound_S, SIGNAL(toggled(bool)),
        game, SLOT(setSound(bool)));
    connect(ui.actionAnimation_A, SIGNAL(toggled(bool)),
        game, SLOT(setAnimation(bool)));

    // 视图上下翻转
    connect(ui.actionFlip_F, &QAction::triggered,
        game, &GameController::flip);
    // 视图左右镜像
    connect(ui.actionMirror_M, &QAction::triggered,
        game, &GameController::mirror);
    // 视图须时针旋转90°
    connect(ui.actionTurnRight_R, &QAction::triggered,
        game, &GameController::turnRight);
    // 视图逆时针旋转90°
    connect(ui.actionTurnLeftt_L, &QAction::triggered,
        game, &GameController::turnLeft);

    // 关联控制器的信号和主窗口控件的槽
    // 更新LCD1，显示玩家1用时
    connect(game, SIGNAL(time1Changed(QString)),
        ui.lcdNumber_1, SLOT(display(QString)));
    // 更新LCD2，显示玩家2用时
    connect(game, SIGNAL(time2Changed(QString)),
        ui.lcdNumber_2, SLOT(display(QString)));
    connect(game, &GameController::pieceCountsChanged, this,
        [this](const QString &player1, const QString &player2) {
            ui.label1->setText(player1);
            ui.label2->setText(player2);
        });

    // 关联场景的信号和控制器的槽
    connect(scene, SIGNAL(mouseReleased(QPointF)),
        game, SLOT(actionPiece(QPointF)));

    // 为状态栏添加一个正常显示的标签
    QLabel *statusBarlabel = new QLabel(this);
    ui.statusBar->addWidget(statusBarlabel);
    // 更新状态栏
    connect(game, SIGNAL(statusBarChanged(QString)),
        statusBarlabel, SLOT(setText(QString)));

    // 默认第2号规则
    ruleNo = 2;
    ruleActionList.at(ruleNo)->setChecked(true);
    // 重置游戏规则
    game->setRule(ruleNo);
    // 更新规则显示
    ruleInfo();

    // 关联列表视图和字符串列表模型
    ui.listView->setModel(game->getManualListModel());
    // 追加新招后自动选中最后一行：rowsInserted置标志，dataChanged落到末行时选中并滚到底
    // （等价于原先在派生视图里重载rowsInserted/dataChanged的做法）
    QAbstractItemModel *manualModel = game->getManualListModel();
    connect(manualModel, &QAbstractItemModel::rowsInserted, this, [this] {
        newManualRow = true;
    });
    connect(manualModel, &QAbstractItemModel::dataChanged, this,
        [this, manualModel](const QModelIndex &, const QModelIndex &bottomRight) {
            if (!newManualRow)
                return;
            const QModelIndex last = manualModel->index(manualModel->rowCount() - 1, 0);
            if (bottomRight == last) {
                ui.listView->setCurrentIndex(last);
                ui.listView->scrollToBottom();
                newManualRow = false;
            }
        });
    // 因为QListView的rowsInserted在setModel之后才能启动，
    // 第一次需手动初始化选中listView第一项
    //qDebug() << ui.listView->model();
    ui.listView->setCurrentIndex(ui.listView->model()->index(0, 0));
    // 浏览行号以控制器 currentRow 为唯一基准：
    // 控制器发出 browseRowChanged 后，由窗口统一回刷选中行与导航键状态
    connect(game, &GameController::browseRowChanged,
        this, &NineChessWindow::onBrowseRowChanged);
    // 初始局面、前一步、后一步、最终局面的槽
    connect(ui.actionBegin_S, &QAction::triggered,
        this, &NineChessWindow::on_actionRowChange);
    connect(ui.actionPrevious_B, &QAction::triggered,
        this, &NineChessWindow::on_actionRowChange);
    connect(ui.actionNext_F, &QAction::triggered,
        this, &NineChessWindow::on_actionRowChange);
    connect(ui.actionEnd_E, &QAction::triggered,
        this, &NineChessWindow::on_actionRowChange);
    // 手动在listView里选择招法后更新的槽
    // QItemSelectionModel自带currentChanged信号，无需派生视图转发
    connect(ui.listView->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &NineChessWindow::on_actionRowChange);
    // 初始化导航键状态
    onBrowseRowChanged(game->browseRow());
}

void NineChessWindow::ruleInfo()
{
    if (ruleNo < 0 || ruleNo >= NineChess::RULE_COUNT)
        return;

    const int s = game->getStepsLimit();
    const int t = game->getTimeLimit();
    const QString stepsText = s > 0 ? tr(" 限%1步").arg(s) : tr(" 不限步");
    const QString timeText = t > 0 ? tr(" 限时%1分").arg(t) : tr(" 不限时");

    // 规则显示
    ui.labelRule->setText(stepsText + timeText);
    // 规则提示：规则名与说明是模型层的常量文本，统一使用 "NineChess" 上下文翻译
    const QString ruleName = QCoreApplication::translate("NineChess", NineChess::rules[ruleNo].name);
    const QString ruleDescription = QCoreApplication::translate("NineChess", NineChess::rules[ruleNo].description);
    ui.labelInfo->setToolTip(ruleName + "\n" + ruleDescription);
    ui.labelRule->setToolTip(ui.labelInfo->toolTip());
}

void NineChessWindow::on_actionLimited_T_triggered()
{
    /* 其实本来可以用设计器做个ui，然后从QDialog派生个自己的对话框
    * 但我不想再派生新类了，又要多出一个类和两个文件
    * 还要写与主窗口的接口，费劲
    * 于是手写QDialog界面
    */
    int gStep = game->getStepsLimit();
    int gTime = game->getTimeLimit();

    // 定义新对话框
    QDialog *dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    dialog->setObjectName(QStringLiteral("Dialog"));
    dialog->setWindowTitle(tr("步数和时间限制"));
    dialog->resize(256, 108);
    dialog->setModal(true);
    // 生成各个控件
    QFormLayout *formLayout = new QFormLayout(dialog);
    QLabel *label_step = new QLabel(dialog);
    QLabel *label_time = new QLabel(dialog);
    QComboBox *comboBox_step = new QComboBox(dialog);
    QComboBox *comboBox_time = new QComboBox(dialog);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);
    // 设置各个控件ObjectName，不设也没关系
    /*formLayout->setObjectName(QStringLiteral("formLayout"));
    label_step->setObjectName(QStringLiteral("label_step"));
    label_time->setObjectName(QStringLiteral("label_time"));
    comboBox_step->setObjectName(QStringLiteral("comboBox_step"));
    comboBox_time->setObjectName(QStringLiteral("comboBox_time"));
    buttonBox->setObjectName(QStringLiteral("buttonBox"));*/
    // 设置各个控件数据
    label_step->setText(tr("超出限制步数判和："));
    label_time->setText(tr("任意一方超时判负："));
    comboBox_step->addItem(tr("无限制"), 0);
    comboBox_step->addItem(tr("50步"), 50);
    comboBox_step->addItem(tr("100步"), 100);
    comboBox_step->addItem(tr("200步"), 200);
    comboBox_time->addItem(tr("无限制"), 0);
    comboBox_time->addItem(tr("5分钟"), 5);
    comboBox_time->addItem(tr("10分钟"), 10);
    comboBox_time->addItem(tr("20分钟"), 20);
    comboBox_step->setCurrentIndex(comboBox_step->findData(gStep));
    comboBox_time->setCurrentIndex(comboBox_time->findData(gTime));
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttonBox->setCenterButtons(true);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    // 布局
    formLayout->setSpacing(6);
    formLayout->setContentsMargins(11, 11, 11, 11);
    formLayout->setWidget(0, QFormLayout::LabelRole, label_step);
    formLayout->setWidget(0, QFormLayout::FieldRole, comboBox_step);
    formLayout->setWidget(1, QFormLayout::LabelRole, label_time);
    formLayout->setWidget(1, QFormLayout::FieldRole, comboBox_time);
    formLayout->setWidget(2, QFormLayout::SpanningRole, buttonBox);
    // 关联信号和槽函数
    connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));
    // 收集数据
    if (dialog->exec() == QDialog::Accepted) {
        int dStep = comboBox_step->currentData().toInt();
        int dTime = comboBox_time->currentData().toInt();
        if (gStep != dStep || gTime != dTime) {
            // 重置游戏规则
            game->setRule(ruleNo, dStep, dTime);
        }
    }

    // 删除对话框，子控件会一并删除
    dialog->disconnect();
    delete dialog;

    // 更新规则显示
    ruleInfo();
}

void NineChessWindow::actionRules_triggered()
{
    // 取消自动运行
    ui.actionAutoRun_A->setChecked(false);

    // 取消其它规则的选择
    for(QAction *action: ruleActionList)
        action->setChecked(false);

    // 选择当前规则
    QAction *action = dynamic_cast<QAction *>(sender());
    action->setChecked(true);
    ruleNo = action->data().toInt();

    // 如果游戏规则没变化，则返回
    if (ruleNo == game->getRuleNo())
        return;

    // 取消AI设定
    ui.actionEngine1_T->setChecked(false);
    ui.actionEngine2_R->setChecked(false);

    // 重置游戏规则
    game->setRule(ruleNo);
    // 更新规则显示
    ruleInfo();
}

void NineChessWindow::on_actionNew_N_triggered()
{
    if (file.isOpen())
        file.close();
    // 取消自动运行
    ui.actionAutoRun_A->setChecked(false);
    // 取消AI设定
    ui.actionEngine1_T->setChecked(false);
    ui.actionEngine2_R->setChecked(false);
    // 重置游戏规则
    game->gameReset();
}

void NineChessWindow::on_actionOpen_O_triggered()
{
    QString path = QFileDialog::getOpenFileName(this, tr("打开棋谱文件"), lastManualDirectory(), "TXT(*.txt)");
    if (path.isEmpty() == false)
    {
        saveLastManualDirectory(path);
        if (file.isOpen())
            file.close();
        // 文件对象
        file.setFileName(path);
        // 不支持1MB以上的文件
        if (file.size() > 0x100000 )
        {
            // 定义新对话框
            QMessageBox msgBox(QMessageBox::Warning, tr("文件过大"), tr("不支持1MB以上文件"), QMessageBox::Ok);
            msgBox.exec();
            file.setFileName(QString());
            return;
        }

        // 打开文件,只读方式打开
        bool isok = file.open(QFileDevice::ReadOnly | QFileDevice::Text);
        if (isok)
        {
            // 先把棋谱整体读入内存再回放，避免半路失败留下两盘棋混合的局面。
            // 保持 file 处于打开状态，“保存”时仍写回同一文件（与原行为一致）。
            QStringList commands;
            QTextStream textStream(&file);
            textStream.setCodec("UTF-8");
            while (!textStream.atEnd())
                commands.append(textStream.readLine());

            // 取消AI设定
            ui.actionEngine1_T->setChecked(false);
            ui.actionEngine2_R->setChecked(false);

            // 棋谱是命令流，逐条执行即可：首行可为对局配置命令 r<s<t<
            //（由控制器识别，切换规则并恢复限时限步），老格式棋谱没有配置行，
            // 按当前规则回放。无论哪种格式都先重开一局再回放。
            // 回放前后按实际规则同步菜单勾选与限时限步标签
            const auto syncRuleUi = [this]() {
                ruleNo = game->getRuleNo();
                for (QAction *action : ruleActionList)
                    action->setChecked(false);
                ruleActionList.at(ruleNo)->setChecked(true);
                ruleInfo();
            };

            game->gameReset();
            for (const QString &cmd : commands)
            {
                if (!cmd.trimmed().isEmpty() && !game->command(cmd, false))
                {
                    // 定义新对话框
                    QMessageBox msgBox(QMessageBox::Warning, tr("文件错误"), tr("不是正确的棋谱文件"), QMessageBox::Ok);
                    msgBox.exec();
                    // 回放失败不留残局，也不关联这个坏文件
                    game->gameReset();
                    syncRuleUi();
                    file.close();
                    file.setFileName(QString());
                    return;
                }
            }
            syncRuleUi();

            // 最后刷新棋局场景；浏览行号已随命令推进到末行，
            // 列表选中行与导航键状态由 browseRowChanged 统一回刷
            game->updateScence();
        }
        else
        {
            file.setFileName(QString());
        }
    }
}

// 把当前对局写入已关联的棋谱文件：棋谱是命令流，
// 首行为对局配置命令 r<s<t<（规则与限时限步，0 表示不限制），
// 之后每行一条走子命令；本局由外部裁定结束（超时判负）时
// 末尾补写负方认输命令，使回放能到达同一终局。
bool NineChessWindow::writeGameRecord()
{
    // 打开文件,只写方式打开
    bool isok = file.open(QFileDevice::WriteOnly | QFileDevice::Text);
    if (!isok)
        return false;

    // 写文件；统一按 UTF-8（无 BOM）写出
    QTextStream textStream(&file);
    textStream.setCodec("UTF-8");
    // 首行：对局配置命令（负值没有意义，钳为 0=不限）
    textStream << QStringLiteral("r%1s%2t%3")
        .arg(game->getRuleNo())
        .arg(qMax(0, game->getStepsLimit()))
        .arg(qMax(0, game->getTimeLimit()))
        << "\n";
    // 招法命令行
    QStringListModel *strlist = qobject_cast<QStringListModel *>(ui.listView->model());
    for (QString cmd : strlist->stringList())
        textStream << cmd << "\n";
    // 超时判负没有命令记录，补写负方认输命令（-0 先手负 / -1 后手负）
    if (game->isEndedByAdjudication()) {
        const NineChess::Players winner = game->getWinner();
        if (winner == NineChess::PLAYER1)
            textStream << "-1" << "\n";
        else if (winner == NineChess::PLAYER2)
            textStream << "-0" << "\n";
    }
    textStream.flush();
    return true;
}

void NineChessWindow::on_actionSave_S_triggered()
{
    if (file.isOpen())
    {
        file.close();
        writeGameRecord();
    }
    else
        on_actionSaveAs_A_triggered();
}

void NineChessWindow::on_actionSaveAs_A_triggered()
{
    QString initialPath = file.fileName();
    if (initialPath.isEmpty())
        initialPath = QDir(lastManualDirectory()).filePath(tr("棋谱.txt"));

    QString path = QFileDialog::getSaveFileName(this, tr("保存棋谱文件"), initialPath, "TXT(*.txt)");
    if (path.isEmpty() == false)
    {
        saveLastManualDirectory(path);
        if (file.isOpen())
            file.close();
        //文件对象
        file.setFileName(path);
        writeGameRecord();
    }
}

void NineChessWindow::on_actionEdit_E_toggled(bool arg1)
{
    Q_UNUSED(arg1)
}

void NineChessWindow::on_actionInvert_I_toggled(bool arg1)
{
    // 如果黑白反转
    if (arg1)
    {
        // 设置玩家1和玩家2的标识图
        ui.actionEngine1_T->setIcon(QIcon(":/icon/Resources/icon/White.png"));
        ui.actionEngine2_R->setIcon(QIcon(":/icon/Resources/icon/Black.png"));
        ui.picLabel1->setPixmap(QPixmap(":/icon/Resources/icon/White.png"));
        ui.picLabel2->setPixmap(QPixmap(":/icon/Resources/icon/Black.png"));
    }
    else
    {
        // 设置玩家1和玩家2的标识图
        ui.actionEngine1_T->setIcon(QIcon(":/icon/Resources/icon/Black.png"));
        ui.actionEngine2_R->setIcon(QIcon(":/icon/Resources/icon/White.png"));
        ui.picLabel1->setPixmap(QPixmap(":/icon/Resources/icon/Black.png"));
        ui.picLabel2->setPixmap(QPixmap(":/icon/Resources/icon/White.png"));
    }
    // 让控制器改变棋子颜色
    game->setInvert(arg1);
}

// 前后招的公共槽：把目标行号交给控制器，界面同步由 browseRowChanged 统一回刷
void NineChessWindow::on_actionRowChange()
{
    QAbstractItemModel * model = ui.listView->model();
    if (!model)
        return;
    int rows = model->rowCount();
    int targetRow = ui.listView->currentIndex().row();

    QObject * const obsender = sender();
    if (obsender == ui.actionBegin_S) {
        targetRow = 0;
    }
    else if (obsender == ui.actionPrevious_B) {
        targetRow = qMax(0, targetRow - 1);
    }
    else if (obsender == ui.actionNext_F) {
        targetRow = qMin(rows - 1, targetRow + 1);
    }
    else if (obsender == ui.actionEnd_E) {
        targetRow = rows - 1;
    }

    if (targetRow >= 0 && targetRow < rows)
        game->browseTo(targetRow);
}

// 控制器浏览行号/棋谱行数变化后的统一同步：
// 选中 listView 对应行，并按行号位置更新导航键与自动运行键的可用状态
void NineChessWindow::onBrowseRowChanged(int row)
{
    QAbstractItemModel * model = ui.listView->model();
    if (!model)
        return;
    int rows = model->rowCount();

    if (row >= 0 && row < rows && ui.listView->currentIndex().row() != row)
        ui.listView->setCurrentIndex(model->index(row, 0));

    const bool canBrowse = rows > 1;
    ui.actionBegin_S->setEnabled(canBrowse && row > 0);
    ui.actionPrevious_B->setEnabled(canBrowse && row > 0);
    ui.actionNext_F->setEnabled(canBrowse && row < rows - 1);
    ui.actionEnd_E->setEnabled(canBrowse && row < rows - 1);
    ui.actionAutoRun_A->setEnabled(canBrowse && row < rows - 1);
}

// 自动运行定时处理函数
void NineChessWindow::onAutoRunTimeOut(QPrivateSignal signal)
{
    Q_UNUSED(signal)
    int rows = ui.listView->model()->rowCount();
    int currentRow = ui.listView->currentIndex().row();

    if (rows <= 1 || currentRow >= rows - 1) {
        ui.actionAutoRun_A->setChecked(false);
        return;
    }

    // 执行“下一招”，选中行与导航键状态由 browseRowChanged 统一刷新
    game->browseTo(currentRow + 1);
}

// 自动运行
void NineChessWindow::on_actionAutoRun_A_toggled(bool arg1)
{
    if (arg1) {
        // 自动运行前禁用控件
        ui.dockWidget->setEnabled(false);
        ui.gameView->setEnabled(false);
        // 启动定时器；间隔=动画时长+50ms余量，但至少0.5秒走一步：
        // 取消"落子动画"时动画时长为0，不设下限就只有50ms一步，快得看不清
        const int minAutoRunIntervalMS = 500;
        autoRunTimer.start(qMax(minAutoRunIntervalMS, game->getDurationTime() + 50));
    }
    else {
        // 关闭定时器
        autoRunTimer.stop();
        // 自动运行结束后启用控件
        ui.dockWidget->setEnabled(true);
        ui.gameView->setEnabled(true);
    }
}

void NineChessWindow::on_actionLocal_L_triggered()
{
    ui.actionLocal_L->setChecked(true);
    ui.actionInternet_I->setChecked(false);
}

void NineChessWindow::on_actionInternet_I_triggered()
{
    ui.actionLocal_L->setChecked(false);
    ui.actionInternet_I->setChecked(true);
}

void NineChessWindow::on_actionEngine_E_triggered()
{
    // 定义新对话框
    QDialog *dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    dialog->setObjectName(QStringLiteral("Dialog"));
    dialog->setWindowTitle(tr("AI设置"));
    dialog->resize(256, 188);
    dialog->setModal(true);

    // 生成各个控件
    QVBoxLayout *vLayout = new QVBoxLayout(dialog);
    QGroupBox *groupBox1 = new QGroupBox(dialog);
    QGroupBox *groupBox2 = new QGroupBox(dialog);

    QHBoxLayout *hLayout1 = new QHBoxLayout;
    QLabel *label_depth1 = new QLabel(dialog);
    QSpinBox *spinBox_depth1 = new QSpinBox(dialog);
    QLabel *label_time1 = new QLabel(dialog);
    QSpinBox *spinBox_time1 = new QSpinBox(dialog);

    QHBoxLayout *hLayout2 = new QHBoxLayout;
    QLabel *label_depth2 = new QLabel(dialog);
    QSpinBox *spinBox_depth2 = new QSpinBox(dialog);
    QLabel *label_time2 = new QLabel(dialog);
    QSpinBox *spinBox_time2 = new QSpinBox(dialog);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);

    // 设置各个控件数据
    groupBox1->setTitle(tr("玩家1 AI设置"));
    label_depth1->setText(tr("深度"));
    spinBox_depth1->setMinimum(1);
    spinBox_depth1->setMaximum(20);
    label_time1->setText(tr("限时(秒)"));
    spinBox_time1->setMinimum(1);
    spinBox_time1->setMaximum(60);

    groupBox2->setTitle(tr("玩家2 AI设置"));
    label_depth2->setText(tr("深度"));
    spinBox_depth2->setMinimum(1);
    spinBox_depth2->setMaximum(20);
    label_time2->setText(tr("限时(秒)"));
    spinBox_time2->setMinimum(1);
    spinBox_time2->setMaximum(60);

    buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttonBox->setCenterButtons(true);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));

    // 布局控件
    vLayout->addWidget(groupBox1);
    vLayout->addWidget(groupBox2);
    vLayout->addWidget(buttonBox);
    groupBox1->setLayout(hLayout1);
    groupBox2->setLayout(hLayout2);
    hLayout1->addWidget(label_depth1);
    hLayout1->addWidget(spinBox_depth1);
    hLayout1->addWidget(label_time1);
    hLayout1->addWidget(spinBox_time1);
    hLayout2->addWidget(label_depth2);
    hLayout2->addWidget(spinBox_depth2);
    hLayout2->addWidget(label_time2);
    hLayout2->addWidget(spinBox_time2);

    // 关联信号和槽函数
    connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));

    // 目前数据
    int depth1, depth2, time1, time2;
    game->getAiDepthTime(depth1, time1, depth2, time2);
    spinBox_depth1->setValue(depth1);
    spinBox_depth2->setValue(depth2);
    spinBox_time1->setValue(time1);
    spinBox_time2->setValue(time2);

    // 新设数据
    if (dialog->exec() == QDialog::Accepted) {
        int depth1_new, depth2_new, time1_new, time2_new;
        depth1_new = spinBox_depth1->value();
        depth2_new = spinBox_depth2->value();
        time1_new = spinBox_time1->value();
        time2_new = spinBox_time2->value();

        if (depth1 != depth1_new || depth2 != depth2_new || time1 != time1_new || time2 != time2_new) {
            // 重置AI
            game->setAiDepthTime(depth1_new, time1_new, depth2_new, time2_new);
        }
    }

    // 删除对话框，子控件会一并删除
    dialog->disconnect();
    delete dialog;
}

void NineChessWindow::on_actionViewHelp_V_triggered()
{
    QDesktopServices::openUrl(QUrl("https://gitee.com/liuweilhy/NineChess"));
}

void NineChessWindow::on_actionWeb_W_triggered()
{
    QDesktopServices::openUrl(QUrl("https://www.cnblogs.com/liuweilhy"));
}

void NineChessWindow::on_actionAbout_A_triggered()
{
    QDialog * dialog = new QDialog;

    dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    dialog->setObjectName(QStringLiteral("aboutDialog"));
    dialog->setWindowTitle(tr("九连棋 v%1").arg(QString::fromLatin1(NINECHESS_VERSION_SHORT)));
    dialog->setModal(true);
    // 生成各个控件
    QVBoxLayout *vLayout = new QVBoxLayout(dialog);
    QHBoxLayout *hLayout = new QHBoxLayout;
    QLabel *label_icon1 = new QLabel(dialog);
    QLabel *label_icon2 = new QLabel(dialog);
    QLabel *label_text = new QLabel(dialog);
    QLabel* label_text1 = new QLabel(dialog);
    QLabel *label_text2 = new QLabel(dialog);
    // 设置各个控件数据
    label_icon1->setPixmap(QPixmap(QString::fromUtf8(":/image/resources/image/black_piece.png")));
    label_icon2->setPixmap(QPixmap(QString::fromUtf8(":/image/resources/image/white_piece.png")));
    label_icon1->setAlignment(Qt::AlignCenter);
    label_icon2->setAlignment(Qt::AlignCenter);
    label_icon1->setFixedSize(32, 32);
    label_icon2->setFixedSize(32, 32);
    label_icon1->setScaledContents(true);
    label_icon2->setScaledContents(true);

    label_text->setText(tr("NineChess v%1").arg(QString::fromLatin1(NINECHESS_VERSION_SHORT)));
    label_text->setAlignment(Qt::AlignCenter);
    // 地址与邮箱做成可点击链接：默认由系统浏览器/邮件客户端打开
    label_text1->setTextFormat(Qt::RichText);
    label_text1->setOpenExternalLinks(true);
    label_text1->setText(QStringLiteral("<a href=\"https://www.cnblogs.com/liuweilhy\">https://www.cnblogs.com/liuweilhy</a>"));
    label_text1->setAlignment(Qt::AlignRight);
    label_text2->setTextFormat(Qt::RichText);
    label_text2->setOpenExternalLinks(true);
    label_text2->setText(QStringLiteral("<a href=\"mailto:liuweilhy@163.com\">-- by liuweilhy@163.com</a>"));
    label_text2->setAlignment(Qt::AlignRight);

    // 布局
    vLayout->addLayout(hLayout);
    hLayout->addWidget(label_icon1);
    hLayout->addWidget(label_icon2);
    hLayout->addWidget(label_text);
    vLayout->addWidget(label_text1);
    vLayout->addWidget(label_text2);
    // 运行对话框
    dialog->exec();

    // 删除对话框
    dialog->disconnect();
    delete dialog;
}

