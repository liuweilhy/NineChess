/****************************************************************************
** NineChess - 九连棋游戏
** 文件: ninechess_version.h
**
** 版本号唯一来源。
** 只需修改下面 4 个数字，即可同步更新：
**   1. exe 资源中的文件版本/产品版本（NineChess.rc 直接包含本头文件取值）；
**   2. 主窗口标题栏；
**   3. “关于”对话框的标题与版本文字。
**
** 约束：本头文件会被资源编译器 rc.exe 直接包含，因此不能出现仅供 C++
** 使用的语法；rc.exe 支持 # 字符串化运算符与反斜杠续行，已验证可用。
****************************************************************************/

#ifndef NINECHESS_VERSION_H
#define NINECHESS_VERSION_H

// ==================== 版本号（唯一来源） ====================
#define NINECHESS_VERSION_MAJOR 2
#define NINECHESS_VERSION_MINOR 2
#define NINECHESS_VERSION_PATCH 0
#define NINECHESS_VERSION_BUILD 0

// 四段数字，供 VERSIONINFO 的 FILEVERSION / PRODUCTVERSION 使用。
#define NINECHESS_VERSION_QUAD \
    NINECHESS_VERSION_MAJOR, NINECHESS_VERSION_MINOR, \
    NINECHESS_VERSION_PATCH, NINECHESS_VERSION_BUILD

// 字符串化工具：两层宏是为了让参数先展开再字符串化。
#define NINECHESS_STRINGIZE_IMPL(x) #x
#define NINECHESS_STRINGIZE(x) NINECHESS_STRINGIZE_IMPL(x)

// 完整版本号字符串，例如 "2.2.0.0"。
#define NINECHESS_VERSION_STRING \
    NINECHESS_STRINGIZE(NINECHESS_VERSION_MAJOR) "." \
    NINECHESS_STRINGIZE(NINECHESS_VERSION_MINOR) "." \
    NINECHESS_STRINGIZE(NINECHESS_VERSION_PATCH) "." \
    NINECHESS_STRINGIZE(NINECHESS_VERSION_BUILD)

// 用于界面显示的短版本号，例如 "2.2"。
#define NINECHESS_VERSION_SHORT \
    NINECHESS_STRINGIZE(NINECHESS_VERSION_MAJOR) "." \
    NINECHESS_STRINGIZE(NINECHESS_VERSION_MINOR)

#endif // NINECHESS_VERSION_H
