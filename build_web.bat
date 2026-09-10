@echo off
REM Build the WebAssembly Breakthrough GUI with Emscripten.
REM
REM Prerequisites (see INSTALL.md):
REM   1. The emsdk, installed and activated. If emcc is not on PATH, this script
REM      activates third_party\emsdk itself when that folder exists.
REM   2. A web build of raylib at third_party\raylib-web\lib\libraylib.a with
REM      headers in third_party\raylib-web\include.
REM
REM Output: build\web\index.html (+ .js, .wasm, .data). Any static file host can
REM serve that folder. Local test:  python -m http.server -d build\web
REM
REM The page is the GUI's simple mode: play White or Black against the Easy /
REM Medium / Hard presets of gui\presets.txt, or watch two presets play. The
REM preset agents' model files are bundled (tools\web_preloads.ps1 lists them).
REM
REM Pass "dev" as the first argument for a debug build (assertions + source map).
setlocal
cd /d "%~dp0"
set RAYLIB=third_party\raylib-web
set OUTDIR=build\web

where emcc >nul 2>nul
if errorlevel 1 (
    if exist "%~dp0third_party\emsdk\emsdk_env.bat" (
        call "%~dp0third_party\emsdk\emsdk_env.bat" >nul 2>nul
    )
)
where emcc >nul 2>nul
if errorlevel 1 (
    echo emcc not found. Install and activate the emsdk first ^(see INSTALL.md^).
    exit /b 1
)
if not exist "%RAYLIB%\lib\libraylib.a" (
    echo Missing %RAYLIB%\lib\libraylib.a. Build raylib for web first ^(see INSTALL.md^).
    exit /b 1
)
if not exist build mkdir build
if not exist %OUTDIR% mkdir %OUTDIR%

set PRELOADS=
for /f "usebackq delims=" %%L in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\web_preloads.ps1"`) do set PRELOADS=%%L
if "%PRELOADS%"=="" (
    echo Could not list the preset model files ^(tools\web_preloads.ps1 failed^).
    exit /b 1
)

set OPTFLAGS=-O3 -flto -sASSERTIONS=0
if /I "%~1"=="dev" set OPTFLAGS=-O0 -gsource-map -sASSERTIONS=2

em++ gui\main_gui.cpp gui\gui_engine.cpp gui\gui_library.cpp ^
   src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp ^
   src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp ^
   src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\ml_cluster.cpp ^
   src\datastore.cpp src\transposition.cpp ^
   src\agents.cpp src\explorers.cpp src\choosers.cpp src\ranking.cpp src\ai_gumbel.cpp ^
   -I src -I gui -I %RAYLIB%\include %RAYLIB%\lib\libraylib.a ^
   -fwasm-exceptions -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB -sSTACK_SIZE=8MB %OPTFLAGS% ^
   --preload-file boards --preload-file gui/presets.txt@gui/presets.txt %PRELOADS% ^
   --shell-file gui\shell.html -o %OUTDIR%\index.html -DPLATFORM_WEB

if errorlevel 1 (
    echo Web build FAILED.
    exit /b 1
)
echo Web build OK -^> %OUTDIR%\index.html
echo Serve locally with:  python -m http.server -d %OUTDIR%
