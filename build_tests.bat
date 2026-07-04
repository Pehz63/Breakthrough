@echo off
REM Build the Breakthrough test suite (Catch2).
REM Requires Visual Studio with the C++ toolset.
setlocal

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VSPATH=%%i
if "%VSPATH%"=="" (
    echo Could not locate Visual Studio via vswhere. Run this from a Developer Command Prompt instead.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"

if not exist build mkdir build

cl tests\test_main.cpp ^
   tests\test_move_validation.cpp tests\test_win_detection.cpp ^
   tests\test_eval.cpp tests\test_ai_integration.cpp tests\test_game_outcomes.cpp ^
   tests\test_ml.cpp tests\test_ranking.cpp ^
   src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp ^
   src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp ^
   src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp ^
   src\explorers.cpp src\choosers.cpp src\agents.cpp src\datastore.cpp ^
   src\transposition.cpp src\ml_train.cpp src\ranking.cpp ^
   /I src /I tests /EHsc /O2 /Fo"build\\" /Fe:tests.exe

if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)
echo Build OK -^> tests.exe
