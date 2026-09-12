/****************************************************************************
** benchmark/linux/checked_sprintf.h — 越界写入诊断垫片（仅调试用）
**
** 背景：Ubuntu 默认开启 _FORTIFY_SOURCE，它在编译期已知目标对象大小时会给
**       sprintf 加检查。本次移植中它在旧引擎进入中局后触发
**       "*** buffer overflow detected ***"，而 AddressSanitizer 抓不到——
**       因为越界目标是结构体的成员数组（char cmdline[32]），ASan 默认不检测
**       对象内越界，而 FORTIFY 检测。要定位到具体调用点，就需要把每个
**       sprintf 的目标大小和实际写入长度比一次。
**
** 用法（只用于诊断构建，正式构建不要加）：
**   g++ ... -include benchmark/linux/checked_sprintf.h ...
**
** 命中时会在 stderr 打印 文件:行号、目标大小、实际长度与格式化内容。
****************************************************************************/

#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// 目标大小未知时 __builtin_object_size 返回 (size_t)-1。
inline long nc_compat_obj_size(const void* p, long known)
{
    (void)p;
    return known;
}

inline int nc_checked_sprintf_impl(char* dst, long dstSize,
                                   const char* file, int line,
                                   const char* fmt, ...)
{
    static char scratch[8192];
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(scratch, sizeof(scratch), fmt, args);
    va_end(args);

    if (dstSize > 0 && n >= 0 && (long)n >= dstSize) {
        fprintf(stderr,
            "\n[checked_sprintf] 越界写入! %s:%d\n"
            "  目标大小=%ld 写入长度=%d 内容=\"%s\"\n\n",
            file, line, dstSize, n, scratch);
        abort();
    }

    if (dst != nullptr) {
        memcpy(dst, scratch, static_cast<size_t>(n) + 1u);
    }
    return n;
}

#define sprintf(dst, ...)                                                     \
    nc_checked_sprintf_impl((dst),                                            \
        nc_compat_obj_size((dst), (long)__builtin_object_size((dst), 0)),      \
        __FILE__, __LINE__, __VA_ARGS__)
