@echo off
rem Build tools: DepthProbe (depth timing), ReplayDebug (dual-model replay), TestMill (mill detection A/B), EvalProbe (evaluate scoring standard verifier)
rem Result exes: bin\DepthProbe.exe bin\ReplayDebug.exe bin\TestMill.exe bin\EvalProbe.exe
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VCVARS% >nul 2>&1
if errorlevel 1 (echo Cannot load MSVC environment & exit /b 1)
set CFLAGS=/nologo /c /O2 /EHsc /permissive- /utf-8 /W0 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG
if not exist obj\tools mkdir obj\tools
if not exist obj\old mkdir obj\old
if not exist obj\new mkdir obj\new
if not exist obj\may mkdir obj\may
if not exist bin mkdir bin
if not exist obj\old\ninechess.obj (cl %CFLAGS% /Fo:obj\old\ old_engine\ninechess.cpp old_engine\ninechessai_ab.cpp)
if not exist obj\new\ninechess.obj (cl %CFLAGS% /Fo:obj\new\ ..\NineChess\src\ninechess.cpp ..\NineChess\src\ninechess_ai_ab.cpp ..\NineChess\src\ninechess_symmetry.cpp)
if not exist obj\may\ninechess.obj (cl %CFLAGS% /Fo:obj\may\ may_engine\ninechess.cpp may_engine\ninechess_ai_ab.cpp)
cl %CFLAGS% /Fo:obj\tools\ depthprobe.cpp replay_debug.cpp test_oldmill.cpp evalprobe.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:bin\DepthProbe.exe /SUBSYSTEM:CONSOLE obj\tools\depthprobe.obj obj\old\ninechess.obj obj\old\ninechessai_ab.obj obj\new\ninechess.obj obj\new\ninechess_ai_ab.obj obj\new\ninechess_symmetry.obj
link /nologo /OUT:bin\ReplayDebug.exe /SUBSYSTEM:CONSOLE obj\tools\replay_debug.obj obj\old\ninechess.obj obj\old\ninechessai_ab.obj obj\new\ninechess.obj obj\new\ninechess_ai_ab.obj obj\new\ninechess_symmetry.obj
link /nologo /OUT:bin\TestMill.exe /SUBSYSTEM:CONSOLE obj\tools\test_oldmill.obj obj\old\ninechess.obj obj\new\ninechess.obj
link /nologo /OUT:bin\EvalProbe.exe /SUBSYSTEM:CONSOLE obj\tools\evalprobe.obj obj\new\ninechess.obj obj\new\ninechess_ai_ab.obj obj\new\ninechess_symmetry.obj
echo Build OK: bin\DepthProbe.exe bin\ReplayDebug.exe bin\TestMill.exe bin\EvalProbe.exe
endlocal
