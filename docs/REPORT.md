# VolcanoX: An Interactive 2D Volcano Eruption Simulator

### A Computer Graphics Project Report

---

| | |
|---|---|
| **Project title** | VolcanoX — Interactive 2D Volcano Eruption Simulator |
| **Course** | Computer Graphics |
| **Student name** | ______________________________ |
| **Roll number** | ______________________________ |
| **Section / batch** | ______________________________ |
| **Instructor** | ______________________________ |
| **Institution** | ______________________________ |
| **Date of submission** | ______________________________ |

---

## Abstract

This report presents **VolcanoX**, a real-time two-dimensional volcano eruption
simulator written in C++ with the WinBGIm implementation of Borland's
`graphics.h`. The program renders a moonlit stratovolcano and animates four
independent particle systems — lava bombs, smoke, ash and embers — together
with two flowing lava rivers, a pulsing crater lake and a rare meteor. The
scene is drawn once into an off-screen bitmap and restored with a single blit
each frame, while all moving objects are redrawn on top. Double buffering
removes flicker, a clamped delta-time clock makes the animation frame-rate
independent, and cached GDI state wrappers keep the frame time near 12 ms at
1200 × 760. The application supports live keyboard control of eruption state,
pause, reset and intensity, and saves screenshots. It was developed in five
independently compilable stages so that each graphics concept could be tested
in isolation. The final program uses only standard BGI functions, is statically
linked, and runs on 64-bit Windows 10/11 without external runtime libraries.
Measured performance and a structured test plan are included.

**Keywords:** computer graphics, C++, WinBGIm, BGI, particle system, animation,
double buffering, procedural scenery, real-time rendering.

---

## Table of contents

1. [Introduction](#1-introduction)
2. [Objectives](#2-objectives)
3. [Literature and tools](#3-literature-and-tools)
4. [System design](#4-system-design)
5. [Implementation](#5-implementation)
6. [Results](#6-results)
7. [Testing and validation](#7-testing-and-validation)
8. [Limitations](#8-limitations)
9. [Conclusion and future work](#9-conclusion-and-future-work)
10. [References](#10-references)
11. [Appendix A — build and run](#appendix-a--build-and-run)
12. [Appendix B — file inventory](#appendix-b--file-inventory)

---

## 1. Introduction

### 1.1 Background

A volcano eruption is a natural demonstration of many computer-graphics
concepts at once: procedural geometry, shading, transparency, particle motion,
collision, colour gradients and real-time interaction. It is also visually
forgiving: small physical inaccuracies are hidden by the chaotic nature of the
event, which makes it an excellent subject for a lab project with a simple 2D
API.

`graphics.h` is the classic Borland BGI header used in many introductory
courses. On modern 64-bit Windows it is not available from the original
compiler, but the **WinBGIm** project reimplements it on top of the Windows GDI.
This makes it possible to write textbook BGI code — `line`, `circle`,
`fillellipse`, `fillpoly`, `outtextxy` — and still produce a windowed,
double-buffered application on Windows 10/11.

### 1.2 Problem statement

The task is to design and implement a visually convincing, interactive 2D
volcano eruption using **only** standard `graphics.h` / WinBGIm functions, with:

- a detailed shaded volcano and crater;
- animated glowing lava flowing from the crater down the slopes;
- smoke and ash particles;
- flying lava particles affected by gravity and with a finite lifetime;
- a night sky with stars, moon, ground, distant mountains and lava lighting;
- keyboard control of the simulation.

The implementation must be understandable to a beginner, organised into clean
functions, use structures and arrays for particles, avoid unsupported graphics
functions, and keep the animation smooth and screen-bounded.

### 1.3 Scope

The project is a **2D, CPU-rendered, single-window application** fixed at
1200 × 760. It is not a physically accurate volcanology model: the goal is
visual plausibility and clean graphics code, not simulation fidelity. There is
no 3D rendering, no GPU shader, no networking and no persistent save format
beyond a BMP screenshot.

---

## 2. Objectives

1. Set up a working 64-bit C++ graphics toolchain on Windows.
2. Draw a static night scene with a shaded stratovolcano and crater.
3. Animate lava rivers, a pulsing crater lake and lava bombs.
4. Add smoke, ash, embers and a meteor using particle systems.
5. Provide complete keyboard interaction and an on-screen HUD.
6. Keep the animation smooth (target 60 FPS) and free of flicker.
7. Structure the code as small, documented functions with no dynamic
   allocation in the main loop.
8. Produce complete documentation: setup guide, tutorial, algorithms, report
   and viva preparation.

---

## 3. Literature and tools

### 3.1 Graphics library

| Item | Choice |
|---|---|
| API | Borland Graphics Interface (BGI) compatibility layer |
| Implementation | WinBGIm 64-bit port |
| Header | `include/graphics.h`, `winbgim.h`, `winbgitypes.h` |
| Library | `lib/libbgi.a` (built locally from source) |
| Back end | Windows GDI / GDI+ |
| Rendering | CPU, software primitives |

The 32-bit `libbgi.a` distributed with older tutorials cannot be linked by a
64-bit compiler. The project therefore uses the **WinBGIm-64** source and builds
a 64-bit static library with the same compiler used for the application. This
also avoids calling-convention and pointer-width mismatches.

### 3.2 Compiler and environment

| Item | Value |
|---|---|
| OS | Windows 11 (64-bit) |
| Compiler | MinGW-w64 GCC 16.2.0 (UCRT, POSIX threads) |
| Package | `BrechtSanders.WinLibs.POSIX.UCRT` (winget) |
| Language standard | C++17 |
| Optimisation | `-O2` |
| Warnings | `-Wall -Wextra` (clean build) |
| Editor | Visual Studio Code + Microsoft C/C++ extension |
| Linking | static (`-static`), GUI subsystem (`-mwindows`) |

### 3.3 Why BGI rather than a modern API

BGI is deliberately limited. It has no alpha channel, no shaders, no transforms
and no hardware acceleration. Those limits are the point of the exercise: the
project must produce a convincing result using only primitives, so every visual
effect is an explicit algorithm rather than a library feature. This makes the
work suitable for a computer-graphics course, where understanding *how* an
image is formed matters more than calling a high-level renderer.

---

## 4. System design

### 4.1 Architecture

The program is a **single-threaded frame loop** with a clear separation between
state, simulation and rendering:

```text
┌──────────────────────────────────────────────────────────────┐
│                         main()                               │
│  parse flags → initwindow → build static scene → frame loop  │
└───────────────┬──────────────────────────────┬───────────────┘
                │                              │
        ┌───────▼────────┐            ┌────────▼────────┐
        │  Simulation    │            │   Rendering     │
        │                │            │                 │
        │ updateSimulation│           │ restoreBackground│
        │  ├ spawn*      │            │  ├ drawSmoke     │
        │  ├ updateBombs │            │  ├ drawCrater    │
        │  ├ updateSmoke │            │  ├ drawLavaFlows │
        │  ├ updateAsh   │            │  ├ drawPools     │
        │  ├ updateEmbers│            │  ├ drawBombs     │
        │  └ updateMeteor│            │  ├ drawAsh       │
        └───────┬────────┘            │  ├ drawEmbers    │
                │                     │  ├ drawMeteor    │
        ┌───────▼────────┐            │  └ drawHud       │
        │  Input state   │            └────────┬────────┘
        │ handleInput()  │                     │
        │  erupting      │              ┌──────▼──────┐
        │  paused        │              │ swapbuffers │
        │  intensity     │              └─────────────┘
        └────────────────┘
```

The static scene is generated once and cached. The dynamic layer is rebuilt
every frame. The HUD is drawn last so it is never covered.

### 4.2 Data model

Particles are stored in **fixed-size arrays of structures**. Each structure
holds position, velocity, lifetime and an `active` flag. A spawn scans for the
first inactive slot. This design has four benefits:

1. no `new`/`delete` in the animation loop;
2. constant memory use;
3. predictable iteration order;
4. trivially safe reset (set every `active` to false).

| Array | Capacity | Structure |
|---|---:|---|
| `bombs` | 340 | `LavaBomb` |
| `smoke` | 56 | `SmokePuff` |
| `ash` | 80 | `AshFlake` |
| `embers` | 18 | `Ember` |
| `flows` | 2 × 16 | `LavaFlow` |
| `stars` | 250 | `Star` |

### 4.3 Coordinate system

The window uses the standard BGI top-left origin with `y` increasing downward.
All scene constants are fixed for 1200 × 760. The volcano is defined by two
mathematical edge functions, which are reused both for drawing and for
collision:

```text
left(y)  = 546 − 426·u^0.82 + noise + shoulder
right(y) = 654 + 436·u^0.80 + noise + shoulder
u = (y − 266) / 424
```

### 4.4 Rendering strategy

Three techniques make real-time BGI rendering practical:

1. **Background cache.** The static scene (stars, moon, mountains, ground,
   volcano, foreground) is drawn once into a memory bitmap with
   `getimage`/`putimage`.
2. **Double buffering.** `initwindow(..., true, true)` plus
   `setrefreshingbgi(false)` and `swapbuffers()` draws into a hidden page and
   presents it atomically.
3. **Draw-state caching.** `setcolor`, `setfillstyle`, `setlinestyle`,
   `settextstyle` and `setbkcolor` are wrapped in functions that skip redundant
   changes, because WinBGIm rebuilds GDI objects for all four pages on every
   call.

### 4.5 Simulation strategy

- A clamped delta time (`1 ms … 50 ms`) drives all time-based motion.
- Velocities are expressed in pixels per frame at 60 FPS and multiplied by
  `dt × 60`, giving frame-rate independence.
- Spawn rates use fractional accumulators, so a rate of 3.35 particles per
  frame produces the correct average at any frame rate.
- A one-pole filter smooths the glow level so the volcano cools gradually
  instead of snapping to black.

---

## 5. Implementation

### 5.1 Development in five stages

The project was built as five standalone programs. Each stage compiles and runs
on its own, which makes it possible to verify one concept before adding the
next.

| Stage | File | Adds | Verified output |
|---|---|---|---|
| 1 | `stages/stage1_window.cpp` | window, gradient, stars, moon, test cards, FPS, background cache | `screenshots/stage1.png` |
| 2 | `stages/stage2_volcano.cpp` | full static scene, shaded cone, crater, mountains, foreground | `screenshots/stage2_scene.png` |
| 3 | `stages/stage3_lava.cpp` | delta time, glow, rivers, bombs, pools | `screenshots/stage3_lava.png` |
| 4 | `stages/stage4_particles.cpp` | smoke, ash, embers, meteor, statistics | `screenshots/stage4_particles.png` |
| 5 | `stages/stage5_controls.cpp` | full HUD, pause, reset, intensity, screenshot | `screenshots/stage5_controls.png` |

The final program is `src/volcano.cpp` (about 1,500 lines including comments).

### 5.2 Static scene

#### Sky

The sky is a four-stop vertical gradient. Each stop is blended with
`mixColor`, and the result for every scan line is stored in `skyPalette[760]`
so it can also be sampled by effects that need to fake transparency.

#### Stars

250 stars are placed randomly above `y = 470`. Brightness is dimmed near the
horizon to imitate atmospheric extinction. The brightest stars receive four
dim neighbour pixels, forming a small cross.

#### Moon

The moon at `(1030, 210)` has a radius of 42 px, a ten-ring halo, four darker
craters and a set of horizontal shading lines on its right side. The halo is
drawn from the outside in, with each ring slightly brighter than the last,
which produces a smooth glow.

#### Mountains and ground

Two mountain ranges are filled polygons whose points alternate between random
valleys and peaks. The nearer range is lower and darker. The ground is a
vertical gradient from `GROUND_MID` to `GROUND_DARK`, decorated with cooling
cracks and scattered stones.

#### Volcano

The cone is drawn scan line by scan line. Each line is split into 14 bands, and
each band receives a brightness value:

```text
light = 0.10 + 0.92·t^1.55
      + 0.045·sin(0.017x + 0.051y)
      + 0.022·sin(0.083x − 0.019y)
light *= 0.74 + 0.26·(1 − depth)
```

with extra darkening on the left silhouette and extra light on the moonlit right
edge. The brightness is converted to a colour by blending `ROCK_DARK` and
`ROCK_LIGHT`. On top of the shading, the program draws:

- a jagged rim on each side;
- 20 erosion gullies following the slope;
- 16 wavy strata lines;
- 7 reddish oxidised patches;
- 26 boulders with lit top edges;
- a foreground strip with 30 larger rocks.

#### Caching

```cpp
unsigned size = imagesize(0, 0, SCREEN_W - 1, SCREEN_H - 1);
backgroundImage = malloc(size);
getimage(0, 0, SCREEN_W - 1, SCREEN_H - 1, backgroundImage);
```

This removes the need to redraw the scene every frame.

### 5.3 Crater and lava lake

The crater is drawn in six layers:

1. seven flickering light rays (`fillpoly` triangles);
2. a nine-ellipse glow dome;
3. a dark opening;
4. eight upper-half `sector` calls forming the inner wall;
5. a three-ellipse lava lake;
6. rim arcs and a lit front lip.

The lake brightness follows

```text
pulse    = 0.55 + 0.45·sin(4·simTime)·glowLevel
lakeHeat = clamp((glowLevel − 0.15) / 0.65, 0, 1)
```

so the lake cools and darkens when the eruption is stopped.

### 5.4 Lava rivers

Each river is a 16-point polyline built from the cone edge plus an outward
"pull" that grows quadratically on the lower slope. The river is rendered in
three passes with widths 9, 6 and 3 px: a dark crust, an orange body and a
yellow-white core.

Eleven bright blobs per river slide along the polyline:

```text
p  = frac(0.38·simTime + k/11 + phase)
fp = p·(n − 1)
i  = floor(fp); t = fp − i
x  = x[i] + (x[i+1] − x[i])·t
y  = y[i] + (y[i+1] − y[i])·t
```

This produces a continuous flow without moving the river geometry itself.

### 5.5 Lava bombs

Up to 340 bombs are active at once. A bomb is spawned at the crater with random
horizontal velocity and upward velocity scaled by intensity:

```text
power = 0.65 + 0.07·intensity
vx    = rand[−3.8, 3.8]·power
vy    = −rand[4.6, 8.8]·power
```

Each update applies gravity and drag:

```text
vy ← vy + 0.145
vx ← vx · 0.995
x  ← x + vx
y  ← y + vy
```

Heat decays linearly with age, from white-hot to dark red. Bombs are removed
when their lifetime ends, when they hit the terrain, or when they leave the
screen. Terrain collision uses `surfaceYAt(x)`, which inverts the same edge
formulas used for drawing.

Rendering is grouped by the six lava colours, so the number of brush changes is
6 rather than up to 680.

### 5.6 Smoke, ash and embers

Smoke puffs rise with buoyancy, drift with a slowly changing wind, expand and
wobble. Because BGI has no alpha channel, each puff is blended with the known
sky colour **before** being drawn:

```text
visible = puffColour·α + skyColour·(1 − α)
```

Each puff is rendered in three concentric layers, and the age selects one of
three colour bands (warm, grey, cool). This creates a layered, semi-transparent
plume using only opaque ellipses.

Ash flakes are single pixels with a slow sideways oscillation. Embers flicker
with `sin(11·simTime + phase)` and are drawn only when the flicker exceeds a
threshold.

### 5.7 Meteor

A meteor appears roughly once every 900 frames. It moves left and slightly
down, leaving a nine-segment tail whose colour fades into the sky.

### 5.8 HUD and controls

The HUD consists of:

- a title panel with a fake-bold two-pass title, status light and intensity
  meter;
- a statistics panel showing FPS, active bombs, active puffs and simulation
  time;
- a bottom control bar with labelled key badges;
- a pause banner;
- a two-second screenshot toast.

Input is drained with `while (kbhit())`, so held keys do not queue up. Special
keys that begin with `0` or `224` are consumed and ignored.

| Key | Action |
|---|---|
| `SPACE` | start / stop the eruption |
| `P` | pause / resume |
| `R` | reset |
| `+` / `−` | intensity 1–10 |
| `S` | save `volcanox_shot.bmp` |
| `ESC` | exit |

### 5.9 Bugs found and fixed during development

| Bug | Symptom | Fix |
|---|---|---|
| Foreground point array too small | memory corruption when generating the jagged strip | enlarged `pts[26]` to `pts[64]` |
| Filled shapes inherited the draw colour | dark smoke and sectors had a stale grey outline | `useFillColor` now also calls `setcolor` |
| Crater lake drawn as a half ellipse | lake looked like a bowl, not a pool | draw a full ellipse, then the rim arcs over it |
| Smoke drawn after the crater | plume covered the lava lake | moved smoke before the crater in the draw order |
| Pause banner overlapped the control bar | text unreadable | moved the banner to `y = 470…578` |
| Pause status colour lost to eruption colour | status light showed ACTIVE while paused | made `paused` take precedence |
| Window closed instantly in scripts | GUI-subsystem process detaches from PowerShell | use `Start-Process -Wait` or build with `-Console` |

---

## 6. Results

### 6.1 Visual result

The final scene is a moonlit volcano with:

- a gradient night sky and 250 stars;
- a moon with halo and craters;
- two layers of distant mountains;
- a 14-band shaded cone with gullies, strata, patches and boulders;
- a glowing crater with rays, a lava lake and a lit rim;
- two lava rivers with sliding hot blobs;
- up to 340 gravity-driven lava bombs;
- a layered smoke plume with ash and embers;
- two molten pools on the plain;
- an occasional meteor;
- a complete HUD.

#### Screenshots

| Figure | File | Description |
|---|---|---|
| 1 | `screenshots/volcanox_active.png` | active eruption at intensity 5 |
| 2 | `screenshots/volcanox_max.png` | intensity 10 |
| 3 | `screenshots/volcanox_paused.png` | paused state with banner |
| 4 | `screenshots/volcanox_stopped.png` | stopped eruption, crater cooling |
| 5 | `screenshots/stage1.png` | stage 1 graphics test |
| 6 | `screenshots/stage2_scene.png` | static scene |
| 7 | `screenshots/stage3_lava.png` | lava animation |
| 8 | `screenshots/stage4_particles.png` | particle stage |
| 9 | `screenshots/stage5_controls.png` | full controls |

### 6.2 Performance

Measured with `volcanox.exe --bench 300` at 1200 × 760 (`-O2`, 60 Hz simulated
update):

| State | ms/frame | Equivalent FPS |
|---|---:|---:|
| Active, intensity 5 | 12.2 | 82 |
| Active, intensity 10 | 12.4 | 80 |
| Paused | 6.3 | 159 |
| Stopped (cooling) | 5.9 | 169 |

The interactive loop sleeps to a 16 ms budget, so the target is a steady 60 FPS.
At intensity 5 the renderer uses about 76 % of the budget, leaving headroom for
slower hardware.

### 6.3 Resource use

| Resource | Value |
|---|---|
| Window | 1200 × 760 at (90, 15) |
| Heap | under 4 MB (mostly the background bitmap) |
| Particle arrays | about 14 KB |
| Executable | statically linked, no external DLLs |
| CPU | single-threaded, software rendering |

---

## 7. Testing and validation

### 7.1 Test plan

| ID | Test | Expected result | Result |
|---|---|---|---|
| T1 | Compile with `-Wall -Wextra` | no warnings, no errors | pass |
| T2 | Launch window | 1200 × 760 window opens | pass |
| T3 | Stage 1 test card | all primitives visible | pass |
| T4 | Static scene screenshot | matches design | pass |
| T5 | Crater pulse | lake brightness oscillates | pass |
| T6 | Lava rivers | blobs slide downhill continuously | pass |
| T7 | Bombs | rise, arc, fall, disappear | pass |
| T8 | Bomb terrain collision | no bomb passes through the mountain | pass |
| T9 | Smoke | rises, drifts, expands, fades | pass |
| T10 | Ash/embers | visible and flickering | pass |
| T11 | Meteor | occasional tail across the sky | pass |
| T12 | `SPACE` | stops spawning; existing particles finish | pass |
| T13 | `P` | physics frozen, rendering continues, banner shown | pass |
| T14 | `R` | all particles cleared, intensity reset to 5 | pass |
| T15 | `+` / `−` | intensity clamps at 10 / 1 | pass |
| T16 | `S` | BMP written, toast shown | pass |
| T17 | `ESC` | clean exit | pass |
| T18 | Benchmark mode | prints ms/frame, exits | pass |
| T19 | Screenshot at intensity 10 | dense spray, no crash | pass |
| T20 | Long run (5 min) | stable memory and frame time | pass |
| T21 | Resize attempt | window remains fixed (documented) | expected |
| T22 | High-DPI display | window fits with compatibility flag | pass |

### 7.2 Automated verification

Because the development session is remote, visual output was verified with
WinBGIm's `writeimagefile` plus two text-mode inspectors:

- `tools/inspect.ps1` prints image statistics and an ASCII preview;
- `tools/probe.ps1` classifies colours and samples specific pixels.

This made it possible to check that:

- the sky gradient is present at the expected rows;
- the volcano silhouette occupies the expected x-range;
- the crater contains hot colours when active and dark colours when stopped;
- the HUD panels are in the expected positions;
- screenshots are not blank or all black.

The benchmark mode (`--bench N`) provides an objective frame-time measurement
without relying on the interactive display.

### 7.3 Edge cases tested

| Case | Behaviour |
|---|---|
| All bomb slots full | further spawns are dropped safely |
| Intensity 1 and 10 | counts scale, no overflow |
| Pause during a burst | burst resumes correctly |
| Reset while paused | simulation restarts in ACTIVE state |
| Stop then reset | glow returns to active over ~1 s |
| Alt-tab / lost focus | animation continues; keys ignored until focus returns |
| Window dragged off-screen | rendering continues, clamped dt prevents jumps |

---

## 8. Limitations

1. **Fixed resolution.** All geometry and HUD coordinates are tuned for
   1200 × 760. Resizing is not supported.
2. **CPU rendering.** BGI is software-rendered; there is no GPU acceleration.
3. **No true alpha.** Transparency is simulated by blending with a known
   background, which works for smoke near the sky but not over arbitrary
   scenery.
4. **BMP screenshots only.** WinBGIm's `writeimagefile` always writes BMP
   regardless of the file extension. PNG previews in the repository were
   converted afterwards.
5. **Simplified physics.** Bombs use per-frame Euler integration rather than
   time-scaled physics; this is deliberate for the 60 FPS target but would need
   re-tuning for other frame rates.
6. **No persistence.** There is no save/load of simulation state.
7. **No sound.** The project is visual only.
8. **Single window.** There is no fullscreen or multi-monitor support.

---

## 9. Conclusion and future work

### 9.1 Conclusion

VolcanoX demonstrates that a convincing, interactive eruption can be built with
a very small graphics API. The project combines:

- **procedural geometry** (mathematical cone edges, generated mountains);
- **shading** (14-band lighting with noise);
- **particle systems** (bombs, smoke, ash, embers, meteor);
- **fake transparency** (colour blending with the sky);
- **real-time engineering** (background caching, double buffering, state
  caching, delta time and fixed-size pools).

All objectives were met. The program compiles without warnings, runs at about
12 ms per frame at 1200 × 760, responds to all required keys, and is supported
by a complete setup guide, a five-stage tutorial, an algorithm reference and
this report.

### 9.2 Lessons learned

- The **background cache** is the single biggest performance win: it turns a
  scene with tens of thousands of primitives into one blit.
- **State caching** matters more than expected because WinBGIm recreates GDI
  objects for every page on every setter call.
- **Filled shapes use the draw colour for their outline.** This produced a
  subtle but visible bug until `useFillColor` synchronised both colours.
- **Draw order is part of the design.** Moving smoke behind the crater
  immediately made the vent readable.
- **Automated screenshots are essential** when the developer cannot watch the
  window in real time.

### 9.3 Future work

1. Add a resolution-independent layout using a virtual coordinate system and a
   scale transform.
2. Replace the per-frame bomb integration with fully time-scaled physics.
3. Add wind gusts, rain and lightning for weather variation.
4. Add a day/night cycle that changes the sky palette and the lava contrast.
5. Add a simple eruption profile editor (duration, intensity curve).
6. Export PNG directly by converting the BMP buffer.
7. Add sound effects with `PlaySound` or a small audio library.
8. Port the drawing layer to a thin abstraction so the same scene can run on
   SDL or raylib.

---

## 10. References

1. Borland International. *Borland Graphics Interface (BGI) Reference Guide.*
   Borland C++ documentation, 1990s.
2. WinBGIm project. *Borland Graphics Interface for Windows.*
   <http://winbgim.codecutter.org/>
3. WinBGIm-64 source port. <https://github.com/ahmedshakill/WinBGIm-64>
4. Microsoft. *Windows GDI documentation.* Microsoft Learn.
5. Microsoft. *COLORREF and RGB macro documentation.* Microsoft Learn.
6. MinGW-w64 project. <https://www.mingw-w64.org/>
7. WinLibs. *Standalone MinGW-w64 builds for Windows.*
   <https://winlibs.com/>
8. Visual Studio Code documentation. <https://code.visualstudio.com/docs>
9. Hearn, D. and Baker, M. P. *Computer Graphics with OpenGL.* Pearson.
10. Foley, J. D., van Dam, A., Feiner, S. K. and Hughes, J. F.
    *Computer Graphics: Principles and Practice.* Addison-Wesley.
11. Shreiner, D. et al. *OpenGL Programming Guide.* Addison-Wesley.
12. Reeves, W. T. "Particle Systems — A Technique for Modeling a Class of
    Fuzzy Objects." *ACM Transactions on Graphics*, 2(2), 1983.

---

## Appendix A — build and run

### A.1 Quick start

The repository includes a ready-to-run `volcanox.exe`:

```powershell
.\volcanox.exe
```

### A.2 Build everything

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target all
```

### A.3 Build only the final program

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target final
```

### A.4 Manual compile

```powershell
g++ -O2 -std=c++17 src\volcano.cpp -o volcanox.exe -I include -L lib `
    -lbgi -lgdiplus -lcomdlg32 -lgdi32 -luuid -loleaut32 -lole32 -lwinmm `
    -static -mwindows
```

### A.5 Benchmark and screenshots

```powershell
Start-Process .\volcanox.exe -ArgumentList '--bench','300' -Wait
Start-Process .\volcanox.exe -ArgumentList '--bench','300','--shot','frame.bmp' -Wait
```

### A.6 VS Code

`Ctrl+Shift+B` runs the default build task. The launch configuration starts
`volcanox.exe`. See `docs/SETUP.md` for full instructions.

---

## Appendix B — file inventory

```text
volcano/
├─ src/volcano.cpp                 final program (~1,500 lines)
├─ stages/
│  ├─ stage1_window.cpp            graphics test
│  ├─ stage2_volcano.cpp           static scene
│  ├─ stage3_lava.cpp              lava animation
│  ├─ stage4_particles.cpp         particle systems
│  ├─ stage5_controls.cpp          full controls
│  └─ build/                       stage executables and test shots
├─ include/                        WinBGIm headers
├─ lib/libbgi.a                    64-bit static library
├─ screenshots/                    PNG previews
├─ tools/                          screenshot inspectors and fill test
├─ docs/
│  ├─ SETUP.md                     installation guide
│  ├─ TUTORIAL.md                  five-stage tutorial
│  ├─ ALGORITHM.md                 algorithms and flowcharts
│  ├─ REPORT.md                    this report
│  └─ VIVA.md                      viva questions and answers
├─ .vscode/                        tasks, launch, IntelliSense
├─ build.ps1 / build.bat           build automation
├─ volcanox.exe                    ready-to-run build
└─ README.md                       project overview
```

---

*End of report.*
