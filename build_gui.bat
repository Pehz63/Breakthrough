@echo off
REM Build the native (Windows/MSVC) Breakthrough GUI.
REM Requires Visual Studio with the C++ toolset and the prebuilt raylib in
REM third_party\raylib-5.5_win64_msvc16 (see README for download instructions).
setlocal
set RAYLIB=third_party\raylib-5.5_win64_msvc16

REM Locate and enter the Visual Studio build environment (provides cl).
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VSPATH=%%i
if "%VSPATH%"=="" (
    echo Could not locate Visual Studio via vswhere. Run this from a Developer Command Prompt instead.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"

if not exist build mkdir build

cl gui\main_gui.cpp ^
   src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp ^
   src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp ^
   src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp ^
   /I src /I gui /I %RAYLIB%\include /EHsc /O2 /MD /Fo"build\\" /Fe:breakthrough_gui.exe ^
   /link /LIBPATH:%RAYLIB%\lib raylib.lib opengl32.lib gdi32.lib winmm.lib kernel32.lib user32.lib shell32.lib

if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)
echo Build OK -^> breakthrough_gui.exe
