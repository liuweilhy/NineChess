@echo off
rem Quick build of NineChessConsole (no Qt) for AI spot checks
rem Usage after build: bin\NineChessConsole.exe
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VCVARS% >nul 2>&1
if errorlevel 1 (echo Cannot load MSVC environment & exit /b 1)
set CFLAGS=/nologo /c /O2 /EHsc /permissive- /utf-8 /W0 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I..\NineChess\src
if not exist obj\console mkdir obj\console
cl %CFLAGS% /Fo:obj\console\ ..\NineChess\src\ninechess.cpp ..\NineChess\src\ninechess_ai_ab.cpp ..\NineChess\src\ninechess_book.cpp ..\NineChess\src\ninechess_symmetry.cpp ..\NineChessConsole\ninechessconsole.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:bin\NineChessConsole.exe /SUBSYSTEM:CONSOLE obj\console\*.obj
if errorlevel 1 (echo Link failed & exit /b 1)
echo Build OK: bin\NineChessConsole.exe
endlocal
