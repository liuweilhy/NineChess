@echo off
rem Build AIBenchmarkMay: May engine (d78811a, ncmay) vs current engine, with eval A/B switches
rem Usage after build: bin\AIBenchmarkMay.exe [rule1] [rule2] [games] [mayDepth] [newDepth] [timeMs] [threads] [pureBest] [wp] [pv]
rem Result exe: bin\AIBenchmarkMay.exe
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VCVARS% >nul 2>&1
if errorlevel 1 (echo Cannot load MSVC environment & exit /b 1)
set CFLAGS=/nologo /c /O2 /EHsc /permissive- /utf-8 /W0 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG
if not exist obj\may mkdir obj\may
if not exist obj\maydrv mkdir obj\maydrv
if not exist obj\new mkdir obj\new
if not exist bin mkdir bin
cl %CFLAGS% /Fo:obj\may\ may_engine\ninechess.cpp may_engine\ninechess_ai_ab.cpp
if errorlevel 1 exit /b 1
cl %CFLAGS% /Fo:obj\maydrv\ benchmark_may.cpp
if errorlevel 1 exit /b 1
cl %CFLAGS% /Fo:obj\new\ ..\NineChess\src\ninechess.cpp ..\NineChess\src\ninechess_ai_ab.cpp ..\NineChess\src\ninechess_symmetry.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:bin\AIBenchmarkMay.exe /SUBSYSTEM:CONSOLE obj\may\*.obj obj\maydrv\benchmark_may.obj obj\new\ninechess.obj obj\new\ninechess_ai_ab.obj obj\new\ninechess_symmetry.obj
if errorlevel 1 (echo Link failed & exit /b 1)
echo Build OK: bin\AIBenchmarkMay.exe
endlocal
