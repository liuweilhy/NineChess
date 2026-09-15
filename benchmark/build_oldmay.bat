@echo off
rem Build AIBenchmarkOldMay: 2018 engine (ncold) vs May engine (ncmay), both single-thread
rem Result exe: bin\AIBenchmarkOldMay.exe
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VCVARS% >nul 2>&1
if errorlevel 1 (echo Cannot load MSVC environment & exit /b 1)
set CFLAGS=/nologo /c /O2 /EHsc /permissive- /utf-8 /W0 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG
if not exist obj\oldmay mkdir obj\oldmay
if not exist obj\old mkdir obj\old
if not exist obj\may mkdir obj\may
if not exist bin mkdir bin
cl %CFLAGS% /Fo:obj\oldmay\ benchmark_old_may.cpp
if errorlevel 1 exit /b 1
cl %CFLAGS% /Fo:obj\old\ old_engine\ninechess.cpp old_engine\ninechessai_ab.cpp
if errorlevel 1 exit /b 1
cl %CFLAGS% /Fo:obj\may\ may_engine\ninechess.cpp may_engine\ninechess_ai_ab.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:bin\AIBenchmarkOldMay.exe /SUBSYSTEM:CONSOLE obj\oldmay\benchmark_old_may.obj obj\old\ninechess.obj obj\old\ninechessai_ab.obj obj\may\ninechess.obj obj\may\ninechess_ai_ab.obj
if errorlevel 1 (echo Link failed & exit /b 1)
echo Build OK: bin\AIBenchmarkOldMay.exe
endlocal
