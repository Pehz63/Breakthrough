# Install / Setup

Everything you need to build Breakthrough: the console app, the native desktop
GUI, and (optionally) the WebAssembly build for the web.

The game logic is plain **C++11** with no third-party dependencies. The GUI adds
two libraries: **raylib** (windowing, drawing, input) and **raygui** (widgets).
raygui is a single header already vendored in `gui/raygui.h`, so you only need to
fetch raylib.

---

## 1. C++ toolchain (required for everything)

- **Visual Studio Community** (2022 or newer; this project was built with the
  VS 18 / 2026 toolset, MSVC 19.51) with the
  **"Desktop development with C++"** workload.
  - That workload is what provides `cl.exe` (the compiler), the linker, and the
    Windows SDK. If you installed "C++ through Visual Studio Community", you most
    likely already have it. To confirm/add it: open **Visual Studio Installer ->
    Modify -> Workloads -> check "Desktop development with C++"**.
- **Language standard:** C++11. MSVC compiles the project fine with its default
  standard, so no `/std` flag is required.

That alone is enough to build and run the **console** app (`build_gui.bat` and the
README's `cl` commands).

---

## 2. raylib (required for the GUI, native build)

The native GUI links a prebuilt raylib for MSVC. Download it once into
`third_party/` (this folder is git-ignored, so each machine fetches its own copy).

From the project root in PowerShell:

```powershell
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
New-Item -ItemType Directory -Force third_party | Out-Null
Invoke-WebRequest `
  -Uri "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_msvc16.zip" `
  -OutFile third_party\raylib.zip
tar -xf third_party\raylib.zip -C third_party
```

This creates `third_party\raylib-5.5_win64_msvc16\` with `include\` and `lib\`,
which is exactly the path `build_gui.bat` expects.

> Note: the prebuilt raylib is compiled against the **dynamic** CRT, so the GUI is
> built with `/MD` (already set in `build_gui.bat`). The resulting
> `breakthrough_gui.exe` links raylib statically (no `raylib.dll` needed at
> runtime) but does rely on the Visual C++ / UCRT runtime, which is present on any
> machine with VS or the VC++ redistributable installed.

### Build & run the native GUI

```powershell
.\build_gui.bat
.\breakthrough_gui.exe
```

`build_gui.bat` locates Visual Studio automatically (via `vswhere`) and sets up the
compiler environment, so you can run it from a plain terminal.

---

## 3. Emscripten + raylib-for-web (optional, only for the web build)

This is only needed to produce the browser/WebAssembly version for GitHub Pages.

### 3a. Install the Emscripten SDK (emsdk)

```powershell
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
.\emsdk install latest
.\emsdk activate latest
.\emsdk_env.ps1        # run this in each new shell to put emcc on PATH
```

### 3b. Build raylib for the web (one time)

There is no official prebuilt web library, so compile raylib once with emcc. With
the emsdk environment active:

```powershell
git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git third_party\raylib-src
cd third_party\raylib-src\src
emcc -c rcore.c rshapes.c rtextures.c rtext.c rmodels.c raudio.c rglfw.c `
  -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I.
emar rcs libraylib.a rcore.o rshapes.o rtextures.o rtext.o rmodels.o raudio.o rglfw.o
cd ..\..\..
# Lay it out where build_web.bat expects it:
New-Item -ItemType Directory -Force third_party\raylib-web\lib, third_party\raylib-web\include | Out-Null
Copy-Item third_party\raylib-src\src\libraylib.a third_party\raylib-web\lib\
Copy-Item third_party\raylib-src\src\raylib.h, third_party\raylib-src\src\raymath.h, third_party\raylib-src\src\rlgl.h third_party\raylib-web\include\
```

### 3c. Build and preview the web version

```powershell
.\build_web.bat            # release build -> docs\index.html (+ .wasm/.js/.data)
.\build_web.bat dev        # debug build (assertions + source map)
python -m http.server -d docs   # then open http://localhost:8000
```

### 3d. Host on GitHub Pages

Commit the generated `docs\` folder, push, then in the GitHub repo go to
**Settings -> Pages** and set the source to **Deploy from a branch**, branch
`main`, folder `/docs`. The game will be served at
`https://<user>.github.io/<repo>/`.

---

## Summary

| You want to...            | Install                                                            |
|---------------------------|--------------------------------------------------------------------|
| Build the console game    | VS Community + "Desktop development with C++"                      |
| Build the desktop GUI     | The above + prebuilt raylib in `third_party/` (step 2)            |
| Build the web version     | The above + emsdk + raylib-for-web (step 3)                       |
