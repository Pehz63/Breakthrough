@echo off
REM Build the WebAssembly Breakthrough GUI with Emscripten.
REM
REM Prerequisites:
REM   1. The emsdk is installed and activated (emcc must be on PATH). Run this
REM      from a shell where you have already run emsdk_env.
REM   2. A web build of raylib (libraylib.a compiled with emcc) exists at
REM      third_party\raylib-web\lib\libraylib.a with headers in
REM      third_party\raylib-web\include. See README for how to build it.
REM
REM Output goes to docs\index.html (+ .wasm, .js, .data) so GitHub Pages can
REM serve it from the main branch /docs folder.
REM
REM Pass "dev" as the first argument for a debug build (assertions + source map).
setlocal
set RAYLIB=third_party\raylib-web
set OUT=docs\index.html

where emcc >nul 2>nul
if errorlevel 1 (
    echo emcc not found on PATH. Activate the emsdk first ^(emsdk_env^).
    exit /b 1
)
if not exist "%RAYLIB%\lib\libraylib.a" (
    echo Missing %RAYLIB%\lib\libraylib.a - build raylib for web first ^(see README^).
    exit /b 1
)
if not exist docs mkdir docs

set OPTFLAGS=-O3 -flto -sASSERTIONS=0
if /I "%~1"=="dev" set OPTFLAGS=-O0 -gsource-map -sASSERTIONS=2

emcc gui\main_gui.cpp ^
   src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp ^
   src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp ^
   -I src -I gui -I %RAYLIB%\include %RAYLIB%\lib\libraylib.a ^
   -sUSE_GLFW=3 %OPTFLAGS% ^
   --preload-file boards ^
   --shell-file gui\shell.html -o %OUT% -DPLATFORM_WEB

if errorlevel 1 (
    echo Web build FAILED.
    exit /b 1
)
echo Web build OK -^> %OUT%
echo Serve locally with:  python -m http.server -d docs
