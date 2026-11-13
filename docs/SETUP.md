# Setup Guide — from a clean Windows PC to a running volcano

This guide installs a 64-bit C++ toolchain, builds the 64-bit WinBGIm library,
compiles the project, and configures VS Code. Every command was tested on
Windows 11 with PowerShell.

> **Already working?** The repository contains `lib/libbgi.a`, the three
> WinBGIm headers, and a compiled `volcanox.exe`. You can skip to
> [Step 7](#step-7--build-the-project). Read Steps 1–6 if you want to rebuild
> the environment from scratch.

---

## Verified environment

| Item | Value |
|---|---|
| OS | Windows 11 (64-bit), 1536×864 desktop |
| Compiler | MinGW-w64 GCC **16.2.0** (UCRT, POSIX threads) |
| Package | `BrechtSanders.WinLibs.POSIX.UCRT` via winget |
| Graphics library | WinBGIm 64-bit, built locally into `lib/libbgi.a` |
| Editor | VS Code + Microsoft C/C++ extension |
| Project resolution | 1200 × 760, window at (90, 15) |

The 32-bit `libbgi.a` that ships with old Turbo/Borland tutorials **will not
link** with a 64-bit compiler. That is why this project builds the 64-bit
WinBGIm source itself.

---

## Step 1 — Install VS Code and the C/C++ extension

1. Install VS Code from <https://code.visualstudio.com/>.
2. Open VS Code → **Extensions** (`Ctrl+Shift+X`) → search **C/C++** →
   install the Microsoft extension (`ms-vscode.cpptools`).
3. Optional but recommended: install **PowerShell** extension.

No CMake is required for the project; the included build scripts call `g++`
directly.

---

## Step 2 — Install MinGW-w64 (64-bit)

Open **PowerShell** and run:

```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e
```

This installs a self-contained WinLibs MinGW-w64 toolchain. Close and reopen
PowerShell afterwards so the `PATH` changes take effect, then verify:

```powershell
g++ --version
```

Expected output begins with something like:

```text
g++.exe (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 16.2.0
```

If `g++` is still not recognised, find it and add it to `PATH` manually:

```powershell
Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" `
    -Filter g++.exe -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
```

Example result on the test machine:

```text
C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\
BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\
mingw64\bin\g++.exe
```

Add that `bin` folder to the user `PATH`:

```powershell
$mingwBin = "C:\Users\<you>\AppData\Local\Microsoft\WinGet\Packages\
BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
[Environment]::SetEnvironmentVariable(
    "Path", $env:Path + ";" + $mingwBin, "User")
```

> The included `build.ps1` searches this WinGet folder automatically, so the
> project builds even when `g++` is not on `PATH`.

**Alternative toolchains:** MSYS2 (`pacman -S mingw-w64-x86_64-gcc`) or a WinLibs
ZIP also work. Use the **64-bit** `mingw64` environment, not `mingw32`.

---

## Step 3 — Download the WinBGIm 64-bit source

Download or clone the 64-bit WinBGIm project:

```powershell
cd $env:USERPROFILE\Downloads
git clone https://github.com/ahmedshakill/WinBGIm-64
```

If Git is unavailable, download the ZIP from the repository page and extract it.
The folder used below is:

```text
WinBGIm-64\libbgi\
├─ include\bgi\graphics.h
├─ include\bgi\winbgim.h
├─ include\bgi\winbgitypes.h
└─ src\*.cxx
```

The upstream project is a 64-bit port of WinBGIm/BGI. The original library is
by Konstantin Knizhnik and contributors.

---

## Step 4 — Build `libbgi.a` with g++

The library must be compiled with the **same 64-bit compiler** that will link
your program. Create a temporary build folder and compile the nine source files
used by the official CMake target:

```powershell
$env:PATH = "C:\path\to\mingw64\bin;$env:PATH"

$src = "C:\Users\<you>\Downloads\WinBGIm-64\libbgi"
$out = "$env:USERPROFILE\Downloads\libbgi-build"
New-Item -ItemType Directory -Force -Path $out | Out-Null
Set-Location $out

$files = @("drawing", "misc", "mouse", "palette", "text",
           "winbgi", "winthread", "dibutil", "file")

foreach ($f in $files) {
    g++ -O2 -m64 -std=c++11 -fno-rtti -fno-exceptions -fpermissive `
        -Wno-write-strings -Wno-narrowing `
        -I "$src\include" -I "$src\src" `
        -c "$src\src\$f.cxx" -o "$f.o"
    if ($LASTEXITCODE -ne 0) { throw "Failed to compile $f.cxx" }
}

# PowerShell does not expand *.o for native commands, so pass the names.
$objs = Get-ChildItem *.o | Select-Object -ExpandProperty Name
ar rcs libbgi.a $objs
Get-Item libbgi.a | Select-Object Name, Length
```

A successful build produces a static library of roughly **110 KB**. Copy it
into the project:

```powershell
Copy-Item "$out\libbgi.a" "C:\path\to\volcano\lib\libbgi.a" -Force
```

Copy the three headers as well:

```powershell
Copy-Item "$src\include\bgi\graphics.h"    "C:\path\to\volcano\include\" -Force
Copy-Item "$src\include\bgi\winbgim.h"     "C:\path\to\volcano\include\" -Force
Copy-Item "$src\include\bgi\winbgitypes.h" "C:\path\to\volcano\include\" -Force
```

### Optional: build with CMake instead

If CMake is installed:

```powershell
cd C:\Users\<you>\Downloads\WinBGIm-64
cmake -G "MinGW Makefiles" -S . -B build-winbgim
cmake --build build-winbgim
```

Then locate the produced `libbgi.a` (usually under
`build-winbgim\libbgi\`) and copy it into `lib\`. The direct `g++` recipe above
is the one used for this repository and is easier to debug.

---

## Step 5 — Check the project layout

Your project folder should contain:

```text
volcano\
├─ include\graphics.h
├─ include\winbgim.h
├─ include\winbgitypes.h
├─ lib\libbgi.a
├─ src\volcano.cpp
├─ stages\...
├─ build.ps1
└─ .vscode\...
```

If `include\graphics.h` is missing, IntelliSense and compilation fail with
`graphics.h: No such file or directory`.

---

## Step 6 — Run a graphics test first

Before the full volcano, prove that the window and WinBGIm work. Build Stage 1:

```powershell
cd C:\path\to\volcano
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage1
Start-Process .\stages\build\stage1.exe
```

Stage 1 should show a dark blue sky, stars, a moon, three test colour cards,
and an FPS counter. Press `ESC` to close, or `S` to save a screenshot.

If Stage 1 fails, fix the toolchain now — do not continue to the final build.

Manual equivalent:

```powershell
g++ -O2 -std=c++17 stages\stage1_window.cpp -o stages\build\stage1.exe `
    -I include -L lib -lbgi -lgdiplus -lcomdlg32 -lgdi32 `
    -luuid -loleaut32 -lole32 -lwinmm -static -mwindows
```

---

## Step 7 — Build the project

### The easy way

```powershell
.\build.bat
```

or:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Target final
```

Build everything (final + all stages):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Target all
```

Build with a console window (helpful for `printf` diagnostics and benchmarks):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Target final -Console
```

### Manual command

```powershell
g++ -O2 -std=c++17 src\volcano.cpp -o volcanox.exe `
    -I include -L lib `
    -lbgi -lgdiplus -lcomdlg32 -lgdi32 `
    -luuid -loleaut32 -lole32 -lwinmm `
    -static -mwindows
```

Flag reference:

| Flag | Why it is needed |
|---|---|
| `-I include` | finds `graphics.h`, `winbgim.h`, `winbgitypes.h` |
| `-L lib` | finds the static library |
| `-lbgi` | WinBGIm graphics functions |
| `-lgdiplus` | GDI+ used by WinBGIm's drawing code |
| `-lgdi32 -lcomdlg32` | Windows GDI and common dialogs |
| `-luuid -loleaut32 -lole32` | COM/GDI+ dependencies |
| `-lwinmm` | Windows multimedia timer support |
| `-static` | no MinGW runtime DLLs are needed at run time |
| `-mwindows` | GUI subsystem: no console window behind the graphics window |
| `-O2` | optimisation; important for particle-heavy frames |

If you omit `-mwindows`, a console stays open and `printf`/benchmark output is
visible. That is useful while debugging.

---

## Step 8 — Configure VS Code

The repository already contains a working `.vscode` folder:

- `tasks.json` — `Ctrl+Shift+B` builds the final program. Other tasks build
  all stages, a single stage, or build-and-run.
- `launch.json` — `F5` starts the debugger for `volcanox.exe` (and Stage 4).
- `c_cpp_properties.json` — include paths and IntelliSense.
- `settings.json` — C++17, UTF-8, 4-space indentation.

**The compiler path in `launch.json` / `c_cpp_properties.json` is specific to
this machine.** On another PC, replace:

```text
C:/Users/Administrator/AppData/Local/Microsoft/WinGet/Packages/
BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin/g++.exe
```

with your own `g++.exe` path (and `gdb.exe` for debugging). To find it:

```powershell
Get-Command g++ -ErrorAction SilentlyContinue | Select-Object Source
```

Useful VS Code shortcuts:

| Shortcut | Action |
|---|---|
| `Ctrl+Shift+B` | build final program |
| `F5` | build + debug |
| `Ctrl+F5` | run without debugging |
| `Ctrl+Shift+P` → **Tasks: Run Task** | choose a stage / build all / build and run |
| `Ctrl+`` ` | integrated terminal |
| `Ctrl+Shift+V` | preview this Markdown file |

### VS Code tasks in this project

| Task label | What it does |
|---|---|
| `build final` | `build.ps1 -Target final` (default build task) |
| `build all stages` | final + stage 1–5 |
| `build stage 2/3/4` | build one tutorial stage |
| `build and run` | build final, then launch `volcanox.exe` |
| `run final (no build)` | launch the existing exe |

---

## Step 9 — Run and test

```powershell
Start-Process .\volcanox.exe
```

or double-click `volcanox.exe`.

Test checklist:

1. Window opens at 1200×760 with a moon and stars.
2. Lava bombs fly out of the crater, smoke rises, rivers glow.
3. `P` shows the **PAUSED** banner and freezes the particles.
4. `SPACE` stops new eruptions; the crater slowly cools.
5. `+` fills more intensity segments and increases activity.
6. `R` clears all particles and restarts the clock.
7. `S` saves `volcanox_shot.bmp` next to the exe.
8. `ESC` closes the program.

Headless / automated check (no user input):

```powershell
Start-Process .\volcanox.exe `
    -ArgumentList "--bench","300","--shot","screenshots\check.bmp" -Wait
powershell -ExecutionPolicy Bypass -File tools\inspect.ps1 screenshots\check.bmp
```

The inspector prints image size, region colour averages, lava/sky pixel counts
and an ASCII luminance map — useful when you cannot look at the screen (for
example over a remote connection).

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `g++ is not recognized` | MinGW `bin` folder is not on `PATH`. Use `build.ps1` (it searches for g++) or add the folder as shown in Step 2. |
| `cannot find -lbgi` | `lib\libbgi.a` is missing or `-L lib` is wrong. Rebuild it in Step 4. |
| `undefined reference to ...` | Link flags missing. Copy the full command in Step 7; do not omit `-lgdiplus -lgdi32 -luuid -loleaut32 -lole32`. |
| `0xc000007b` at startup | 32/64-bit mismatch. Use the 64-bit compiler and the 64-bit `libbgi.a`; never mix a 32-bit library with g++ 64-bit. |
| `graphics.h: No such file` | Headers are not in `include\`, or `-I include` is missing. |
| Window opens and closes instantly | You built with `-mwindows`, which hides error messages. Rebuild with `-Console` and run from a terminal. |
| Window does not open | `initwindow` failed. Check the desktop resolution is larger than 1200×760 and try running outside a restricted Remote Desktop session. |
| Black client area in screenshots | Some remote/virtual desktops cannot capture BGI's window with `CopyFromScreen`. Use the program's own `S` key or `--shot` (WinBGIm `writeimagefile`). |
| Flicker | Ensure `initwindow(..., true, true)`, call `setrefreshingbgi(false)`, and call `swapbuffers()` once per frame. |
| Text has black boxes | WinBGIm text backgrounds are opaque. Call `setbkcolor(panelColour)` (`useTextBackground`) before `outtextxy`. |
| Colours look swapped | `COLOR(r,g,b)` is packed as `0x03BBGGRR`. Read channels with `c & 0xFF`, `(c >> 8) & 0xFF`, `(c >> 16) & 0xFF`. |
| Grey outlines around puffs/sectors | In WinBGIm, filled shapes are outlined with the **draw** colour. The helper `useFillColor` sets both fill and draw colour; keep this behaviour if you refactor. |
| Animation runs too fast/slow on another PC | Movement must use `dt` (seconds), not a fixed number of pixels per frame. The final code clamps `dt` to 0.05 s to survive stalls. |
| `build.ps1 cannot be loaded` | PowerShell execution policy. Run `powershell -ExecutionPolicy Bypass -File build.ps1`, or use `build.bat`. |
| Path contains spaces / OneDrive | Quote paths. Building inside OneDrive can be slower; a short local folder such as `C:\dev\volcano` is more reliable. |
| Antivirus warns about the exe | Static MinGW executables are sometimes flagged heuristically. Allow the file or rebuild locally. |
| High-DPI blur | Right-click `volcanox.exe` → Properties → Compatibility → Change high DPI settings → **Override high DPI scaling behaviour → Application**. |

---

## Uninstall / clean

Build outputs and screenshots are not source code. To clean them:

```powershell
Remove-Item volcanox.exe, volcanox_shot.bmp -ErrorAction SilentlyContinue
Remove-Item stages\build -Recurse -Force -ErrorAction SilentlyContinue
```

Keep `include\`, `lib\`, `src\`, `stages\*.cpp`, `docs\`, `screenshots\*.png`,
`build.ps1` and `.vscode\`.
