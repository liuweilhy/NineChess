@echo off
rem Build AIBenchmark: 2018 engine (a588f4a, ncold) vs current engine (randomPlies supported)
rem Result exe: bin\AIBenchmark.exe
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VCVARS% >nul 2>&1
if errorlevel 1 (echo Cannot load MSVC environment & exit /b 1)
set CFLAGS=/nologo /c /O2 /EHsc /permissive- /utf-8 /W0 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG
if not exist obj\old mkdir obj\old
if not exist obj\new mkdir obj\new
if not exist bin mkdir bin
cl %CFLAGS% /Fo:obj\old\ old_engine\ninechess.cpp old_engine\ninechessai_ab.cpp
if errorlevel 1 exit /b 1
cl %CFLAGS% /Fo:obj\new\ benchmark.cpp ..\NineChess\src\ninechess.cpp ..\NineChess\src\ninechess_ai_ab.cpp ..\NineChess\src\ninechess_symmetry.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:bin\AIBenchmark.exe /SUBSYSTEM:CONSOLE obj\old\*.obj obj\new\*.obj
if errorlevel 1 (echo Link failed & exit /b 1)
echo Build OK: bin\AIBenchmark.exe
endlocal
