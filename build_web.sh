#!/usr/bin/env bash
# Build the WebAssembly Breakthrough GUI with Emscripten on Linux or macOS.
# The same build as build_web.bat (which is the Windows entry point), used by
# the GitHub Pages workflow in .github/workflows/web.yml.
#
# Prerequisites (see INSTALL.md): em++ on PATH (source emsdk_env.sh), a web
# build of raylib at third_party/raylib-web/lib/libraylib.a with headers in
# third_party/raylib-web/include, and PowerShell (pwsh) for
# tools/web_preloads.ps1, which lists and hash-checks the bundled model files.
#
# Output: build/web/index.html (+ .js, .wasm, .data).
# Pass "dev" as the first argument for a debug build (assertions + source map).
set -euo pipefail
cd "$(dirname "$0")"
RAYLIB=third_party/raylib-web
OUTDIR=${OUTDIR:-build/web}

if ! command -v em++ >/dev/null 2>&1; then
    echo "em++ not found. Install and activate the emsdk first (see INSTALL.md)."
    exit 1
fi
if [ ! -f "$RAYLIB/lib/libraylib.a" ]; then
    echo "Missing $RAYLIB/lib/libraylib.a. Build raylib for web first (see INSTALL.md)."
    exit 1
fi
PS=pwsh
command -v pwsh >/dev/null 2>&1 || PS=powershell
PRELOADS=$("$PS" -NoProfile -File tools/web_preloads.ps1 | tr -d '\r')

OPTFLAGS="-O3 -flto -sASSERTIONS=0"
if [ "${1:-}" = "dev" ]; then OPTFLAGS="-O0 -gsource-map -sASSERTIONS=2"; fi
mkdir -p "$OUTDIR"

# $OPTFLAGS and $PRELOADS are word-split on purpose (no paths with spaces).
em++ gui/main_gui.cpp gui/gui_engine.cpp gui/gui_library.cpp \
   src/globals.cpp src/board_io.cpp src/settings.cpp src/board_analysis.cpp \
   src/moves.cpp src/ai_eval.cpp src/ai_random.cpp src/ai_minimax.cpp \
   src/ml_features.cpp src/ml_model.cpp src/ml_eval.cpp src/ml_cluster.cpp \
   src/datastore.cpp src/transposition.cpp \
   src/agents.cpp src/explorers.cpp src/choosers.cpp src/ranking.cpp src/ai_gumbel.cpp \
   -I src -I gui -I "$RAYLIB/include" "$RAYLIB/lib/libraylib.a" \
   -fwasm-exceptions -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB -sSTACK_SIZE=8MB $OPTFLAGS \
   --preload-file boards --preload-file gui/presets.txt@gui/presets.txt $PRELOADS \
   --shell-file gui/shell.html -o "$OUTDIR/index.html" -DPLATFORM_WEB

echo "Web build OK -> $OUTDIR/index.html"
echo "Serve locally with:  python3 -m http.server -d $OUTDIR"
