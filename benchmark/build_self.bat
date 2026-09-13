@echo off
rem Build AIBenchmarkSelf: current engine (9D) self-play benchmark, engine-default random params
rem Usage after build: bin\AIBenchmarkSelf.exe [rule1] [rule2] [games] [depth] [timeMs] [threads] [stepsLimit]
rem Result exe: bin\AIBenchmarkSelf.exe
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VCVARS% >nul 2>&1
if errorlevel 1 (echo Cannot load MSVC environment & exit /b 1)
set CFLAGS=/nologo /c /O2 /EHsc /permissive- /utf-8 /W0 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG
if not exist obj\selfdrv mkdir obj\selfdrv
if not exist obj\selfnew mkdir obj\selfnew
if not exist bin mkdir bin
cl %CFLAGS% /Fo:obj\selfdrv\ benchmark_self.cpp
if errorlevel 1 exit /b 1
cl %CFLAGS% /Fo:obj\selfnew\ ..\NineChess\src\ninechess.cpp ..\NineChess\src\ninechess_ai_ab.cpp ..\NineChess\src\ninechess_symmetry.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:bin\AIBenchmarkSelf.exe /SUBSYSTEM:CONSOLE obj\selfdrv\benchmark_self.obj obj\selfnew\ninechess.obj obj\selfnew\ninechess_ai_ab.obj obj\selfnew\ninechess_symmetry.obj
if errorlevel 1 (echo Link failed & exit /b 1)
echo Build OK: bin\AIBenchmarkSelf.exe
endlocal
