@echo off
REM Build the Breakthrough ML trainer (train.exe).
REM Requires Visual Studio with the C++ toolset.
setlocal

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VSPATH=%%i
if "%VSPATH%"=="" (
    echo Could not locate Visual Studio via vswhere. Run this from a Developer Command Prompt instead.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"

if not exist build mkdir build
if not exist data mkdir data
if not exist models mkdir models
if not exist agents mkdir agents

cl tools\train_main.cpp ^
   src\globals.cpp src\board_io.cpp src\board_analysis.cpp src\moves.cpp ^
   src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp ^
   src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp ^
   src\explorers.cpp src\choosers.cpp src\agents.cpp src\datastore.cpp src\ml_train.cpp ^
   /I src /EHsc /O2 /Fo"build\\" /Fe:train.exe

if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)
echo Build OK -^> train.exe
