/****************************************************************************
** benchmark/linux/windows.h — Windows → POSIX 兼容垫片
**
** 目的：让 benchmark/*.cpp（为 MSVC 编写）在 Linux/GCC 下原样编译运行，
**       不改动任何算法源码，也不改动 benchmark 驱动中的对局逻辑。
**
** 用法：编译时加 -I benchmark/linux，使 `#include <windows.h>` 命中本文件。
**
** 覆盖的 Windows 专有面（已对全部驱动与引擎源码穷举核对）：
**   - 宏/常量：MAX_PATH、CP_UTF8
**   - 控制台：SetConsoleOutputCP / SetConsoleCP（无操作）
**   - 目录  ：CreateDirectoryA
**   - CRT   ：fopen_s（安全版，含反斜杠→斜杠路径转换）、
**             _time64、__time64_t、localtime_s
**
** 反斜杠转换是必要的：驱动把结果写到 "results_may\\rule0_g01_...txt"，
** 在 Linux 上反斜杠是合法文件名字符而非分隔符，不转换会生成怪文件名。
****************************************************************************/

#pragma once

#ifndef _WIN32

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

// 路径分隔符转换：Windows 的 '\\' 在 POSIX 下改为 '/'。
inline std::string nc_compat_slashed(const char* path)
{
    std::string s(path != nullptr ? path : "");
    for (char& c : s) {
        if (c == '\\') {
            c = '/';
        }
    }
    return s;
}

inline int SetConsoleOutputCP(unsigned int /*codePage*/) { return 0; }
inline int SetConsoleCP(unsigned int /*codePage*/) { return 0; }

// MSVC 安全版 fopen：成功返回 0，失败返回 errno。
inline int fopen_s(FILE** fp, const char* filename, const char* mode)
{
    if (fp == nullptr || filename == nullptr || mode == nullptr) {
        return EINVAL;
    }
    *fp = std::fopen(nc_compat_slashed(filename).c_str(), mode);
    return *fp != nullptr ? 0 : errno;
}

// 单层建目录（等价于 Windows 的 CreateDirectoryA 语义：已存在视为失败）。
inline int CreateDirectoryA(const char* path, void* /*securityAttributes*/)
{
    return mkdir(nc_compat_slashed(path).c_str(), 0777) == 0 ? 0 : 1;
}

// MSVC 的 64 位 time_t 及其实参形式。
using __time64_t = long long;

inline __time64_t _time64(__time64_t* out)
{
    const __time64_t now = static_cast<__time64_t>(std::time(nullptr));
    if (out != nullptr) {
        *out = now;
    }
    return now;
}

// 成功返回 0（POSIX 的 localtime_r 成功返回非空指针）。
inline int localtime_s(struct tm* out, const __time64_t* timer)
{
    if (out == nullptr || timer == nullptr) {
        return EINVAL;
    }
    static_assert(sizeof(__time64_t) >= sizeof(time_t), "time_t 宽于 64 位");
    const time_t value = static_cast<time_t>(*timer);
    return localtime_r(&value, out) != nullptr ? 0 : EINVAL;
}

#endif // !_WIN32
