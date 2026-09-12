/****************************************************************************
** benchmark/linux/direct.h — MSVC <direct.h> 的最小替代
**
** NineChessConsole 用到了 <direct.h> 的唯一功能：启动时 _mkdir("books")
** 建开局库目录。POSIX 下用 mkdir(path, mode) 即可。
****************************************************************************/

#pragma once

#ifndef _WIN32

#include <sys/stat.h>
#include <sys/types.h>

inline int _mkdir(const char* path)
{
    return mkdir(path, 0777);
}

#endif // !_WIN32
