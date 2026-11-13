# Tutorial â€” build VolcanoX in five stages

This tutorial builds the complete simulator **one working program at a time**.
Every stage is a standalone `.cpp` file that compiles and runs on its own, so
you always have something to demonstrate, even if you stop halfway.

| Stage | File | Lines | New idea |
|---|---|---:|---|
| 1 | `stages/stage1_window.cpp` | ~240 | Open a window, draw shapes, cache the frame, read the keyboard |
| 2 | `stages/stage2_volcano.cpp` | ~440 | Build the static night scene and the shaded volcano |
| 3 | `stages/stage3_lava.cpp` | ~690 | Add a simulation clock, crater glow, lava rivers and lava bombs |
| 4 | `stages/stage4_particles.cpp` | ~980 | Smoke, ash, embers, meteor and live statistics |
| 5 | `stages/stage5_controls.cpp` | ~1,390 | Full HUD, pause, reset, intensity, screenshots |

The final program is `src/volcano.cpp`. Stage 5 is a mirror of it, kept separate
so the tutorial has a clean "last step".

> **Before Stage 1:** complete **[SETUP.md](SETUP.md)**. You need a 64-bit
> MinGW-w64 compiler, the three WinBGIm headers in `include/`, and
> `lib/libbgi.a`. Verify with `g++ --version` and make sure the project folder
> contains `include\graphics.h` and `lib\libbgi.a`.

---

## How each stage is built

From the project root:

```powershell
# one stage
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage1

# everything (final + all five stages)
powershell -ExecutionPolicy Bypass -File build.ps1 -Target all
```

Or call the compiler directly. The link line is always the same:

```powershell
g++ -O2 -std=c++17 -Wall -Wextra stages\stage1_window.cpp -o stages\build\stage1.exe `
    -I include -L lib -lbgi -lgdiplus -lcomdlg32 -lgdi32 `
    -luuid -loleaut32 -lole32 -lwinmm -static -mwindows
```

**Why those libraries?**

| Flag | Purpose |
|---|---|
| `-I include` | find `graphics.h` / `winbgim.h` |
| `-L lib` | find `libbgi.a` |
| `-lbgi` | WinBGIm itself |
| `-lgdiplus` | GDI+ (used by WinBGIm for anti-aliased text/images) |
| `-lgdi32` | core Windows drawing API |
| `-lcomdlg32`, `-luuid`, `-loleaut32`, `-lole32` | WinBGIm's file dialogs / COM |
| `-lwinmm` | timing helpers |
| `-static` | no MinGW DLLs needed at run time |
| `-mwindows` | GUI subsystem: no console window |

The build script adds `-mwindows` automatically. Add `-Console` if you want to
see `printf()` output (useful for the `--bench` timing mode):

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target final -Console
```

> **Important on Windows:** a GUI-subsystem program returns immediately in
> PowerShell. To wait for it in a script, use
> `Start-Process .\stages\build\stage1.exe -Wait`.

---

# Stage 1 â€” a window, some shapes, and a test card

### Goal

Prove that the compiler, the library and the window all work. Then learn the two
tricks that make the rest of the project possible:

1. **A cached background** â€” draw the expensive scenery once, save it with
   `getimage`, restore it with `putimage` every frame.
2. **Double buffering** â€” draw into a hidden page, then `swapbuffers()`.

### Complete code

This is the full contents of `stages/stage1_window.cpp`. Type it in, or open
the file and read along.

```cpp
/* ============================================================================
   VolcanoX - Interactive 2D Volcano Eruption Simulator
   ----------------------------------------------------------------------------
   STAGE 1 : Opening a graphics window and drawing a test scene
   ----------------------------------------------------------------------------
   What you learn here:
     * how to open a WinBGIm window with initwindow()
     * how to build a colour gradient with horizontal lines
     * how to draw stars, a moon and simple filled shapes
     * how to use double buffering + swapbuffers() for flicker-free animation
     * how to read the keyboard without blocking (kbhit / getch)

   Compile (from the project root):
     g++ stages/stage1_window.cpp -o stage1.exe -I include -L lib -lbgi ^
         -lgdiplus -lcomdlg32 -lgdi32 -luuid -loleaut32 -lole32 -lwinmm -static
   ========================================================================= */

#include <graphics.h>      // WinBGIm : the graphics.h replacement for MinGW
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>       // GetTickCount() for the FPS counter

// ---------------------------------------------------------------------------
// 1. Global constants  (one place to change the window size)
// ---------------------------------------------------------------------------
const int SCREEN_W = 1200;
const int SCREEN_H = 760;

// ---------------------------------------------------------------------------
// 2. Small colour helpers
//    WinBGIm's COLOR(r,g,b) returns 0x03BBGGRR, so when we take a colour
//    apart again the bytes come out as red, green, blue in that order.
// ---------------------------------------------------------------------------
int makeColor(int r, int g, int b) { return COLOR(r, g, b); }

int redOf(int c)   { return  c        & 0xFF; }
int greenOf(int c) { return (c >> 8)  & 0xFF; }
int blueOf(int c)  { return (c >> 16) & 0xFF; }

// Mix two colours. t = 0 gives c1, t = 1 gives c2.
int mixColor(int c1, int c2, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int r = (int)(redOf(c1)   + (redOf(c2)   - redOf(c1))   * t);
    int g = (int)(greenOf(c1) + (greenOf(c2) - greenOf(c1)) * t);
    int b = (int)(blueOf(c1)  + (blueOf(c2)  - blueOf(c1))  * t);
    return makeColor(r, g, b);
}

// Colour of the night sky at a given height.  Four stops, linearly blended.
int skyColorAt(int y)
{
    const int Y0 = 0,   Y1 = 300, Y2 = 480, Y3 = 570;
    const int C0 = makeColor( 5,  7, 16);   // top: almost black blue
    const int C1 = makeColor(14, 15, 34);   // middle: deep indigo
    const int C2 = makeColor(34, 21, 43);   // low: purple
    const int C3 = makeColor(58, 29, 41);   // horizon: warm maroon

    if (y <= Y0) return C0;
    if (y < Y1)  return mixColor(C0, C1, (float)(y - Y0) / (Y1 - Y0));
    if (y < Y2)  return mixColor(C1, C2, (float)(y - Y1) / (Y2 - Y1));
    if (y < Y3)  return mixColor(C2, C3, (float)(y - Y2) / (Y3 - Y2));
    return C3;
}

// ---------------------------------------------------------------------------
// 3. Drawing helpers
// ---------------------------------------------------------------------------

// A gradient is just a lot of one-pixel-high horizontal lines.
void drawSkyGradient()
{
    for (int y = 0; y < SCREEN_H; y++) {
        setcolor(skyColorAt(y));
        line(0, y, SCREEN_W - 1, y);
    }
}

// Stars: mostly tiny dots, a few bright ones with a little cross of light.
void drawStars(int count)
{
    for (int i = 0; i < count; i++) {
        int x = rand() % SCREEN_W;
        int y = rand() % 470;                  // keep stars above the horizon
        int bright = 90 + rand() % 166;         // 90 .. 255

        // Stars near the horizon are dimmed by the warm haze.
        float haze = 1.0f - (float)y / 640.0f;
        if (haze < 0.35f) haze = 0.35f;
        int c = makeColor((int)(bright * haze), (int)(bright * haze),
                          (int)((bright * 0.9f + 25) * haze));

        putpixel(x, y, c);
        if (bright > 225) {                     // bright star -> small cross
            putpixel(x - 1, y, mixColor(c, skyColorAt(y), 0.45f));
            putpixel(x + 1, y, mixColor(c, skyColorAt(y), 0.45f));
            putpixel(x, y - 1, mixColor(c, skyColorAt(y), 0.45f));
            putpixel(x, y + 1, mixColor(c, skyColorAt(y), 0.45f));
        }
    }
}

// A moon made of a bright disc, a soft halo and a few craters.
void drawMoon(int cx, int cy, int r)
{
    // --- halo (widest, faintest ring first) ---
    for (int i = 26; i >= 1; i--) {
        int rr = r + i * 3;
        float alpha = 0.14f * (1.0f - (float)i / 27.0f);
        int c = mixColor(skyColorAt(cy), makeColor(190, 200, 225), alpha);
        setcolor(c);
        circle(cx, cy, rr);
    }

    // --- moon disc ---
    setfillstyle(SOLID_FILL, makeColor(235, 234, 214));
    setcolor(makeColor(200, 200, 185));
    fillellipse(cx, cy, r, r);

    // --- craters: darker grey ellipses, clipped visually inside the disc ---
    setfillstyle(SOLID_FILL, makeColor(205, 205, 190));
    fillellipse(cx - 14, cy - 12, 13, 10);
    fillellipse(cx + 15, cy + 8, 9, 7);
    fillellipse(cx + 2, cy + 22, 7, 5);
    setfillstyle(SOLID_FILL, makeColor(215, 214, 198));
    fillellipse(cx - 10, cy - 11, 7, 5);
    fillellipse(cx + 16, cy + 9, 5, 4);
}

// A simple white "card" used only by this test stage.
void drawTestCard(int x, int y, int w, int h, const char *title, int accent)
{
    setfillstyle(SOLID_FILL, makeColor(12, 14, 24));
    setcolor(accent);
    bar(x, y, x + w, y + h);
    rectangle(x, y, x + w, y + h);
    rectangle(x + 2, y + 2, x + w - 2, y + h - 2);
    setbkcolor(makeColor(12, 14, 24));
    setcolor(accent);
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 2);
    outtextxy(x + 14, y + 12, (char *)title);
    setcolor(makeColor(190, 195, 210));
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 1);
    outtextxy(x + 16, y + 42, (char *)"graphics.h test");

    // some demonstration primitives
    setcolor(makeColor(90, 200, 255));
    circle(x + 60, y + 100, 28);
    setfillstyle(SOLID_FILL, makeColor(255, 110, 40));
    setcolor(makeColor(255, 190, 80));
    fillellipse(x + 150, y + 100, 34, 20);
    int tri[8] = { x + 200, y + 120, x + 240, y + 60, x + 280, y + 120, x + 200, y + 120 };
    setfillstyle(SOLID_FILL, makeColor(70, 210, 130));
    fillpoly(4, tri);
}

// ---------------------------------------------------------------------------
// 4. main
// ---------------------------------------------------------------------------
int main()
{
    // dbflag = true  -> double buffered window (no flicker, no page tearing)
    // closeflag=true -> the X button may be used to close the window
    initwindow(SCREEN_W, SCREEN_H, "VolcanoX - Stage 1: Graphics Test",
               100, 20, true, true);
    setbkcolor(makeColor(0, 0, 0));
    cleardevice();

    // We do the refresh ourselves at the end of each frame.
    setrefreshingbgi(false);
    srand(12345);

    // --- draw the static part of this test frame once -----------------------
    drawSkyGradient();
    drawStars(240);
    drawMoon(1000, 120, 46);

    // a fake "mountain" so we can already see a silhouette against the sky
    int hill[20] = { -40, 640, 150, 500, 300, 560, 430, 470,
                     620, 545, 760, 440, 900, 540, 1200, 470,
                     1200, 760, -40, 760 };
    setfillstyle(SOLID_FILL, makeColor(14, 15, 26));
    fillpoly(10, hill);

    // title
    setbkcolor(makeColor(5, 7, 16));
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 4);
    setcolor(makeColor(255, 120, 40));
    outtextxy(60, 60, (char *)"VolcanoX");
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 2);
    setcolor(makeColor(235, 235, 245));
    outtextxy(62, 112, (char *)"Interactive 2D Volcano Eruption Simulator");
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 1);
    setcolor(makeColor(150, 160, 185));
    outtextxy(64, 146, (char *)"Stage 1 : graphics window, shapes, colours and keyboard input");

    // three cards to prove the drawing primitives all work
    drawTestCard( 60, 230, 330, 170, "WINDOW",  makeColor( 80, 190, 255));
    drawTestCard(435, 230, 330, 170, "COLOURS", makeColor(255, 170, 60));
    drawTestCard(810, 230, 330, 170, "SHAPES",  makeColor(120, 235, 150));

    // footer bar
    setfillstyle(SOLID_FILL, makeColor(8, 9, 16));
    setcolor(makeColor(60, 45, 40));
    bar(0, SCREEN_H - 42, SCREEN_W - 1, SCREEN_H - 1);
    line(0, SCREEN_H - 42, SCREEN_W - 1, SCREEN_H - 42);
    setbkcolor(makeColor(8, 9, 16));
    setcolor(makeColor(220, 220, 230));
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 1);
    outtextxy(24, SCREEN_H - 27, (char *)"ESC: Exit     (in later stages: SPACE start/stop, P pause, R reset, + / - intensity)");

    // --- keep the finished static frame in a memory image -------------------
    // The scene above needs more than a thousand drawing calls.  Instead of
    // repeating them every frame we copy the finished picture once into a
    // bitmap (getimage) and blit it back in one go (putimage).  That is the
    // classic "background cache" trick and it keeps the animation smooth.
    void *background = malloc(imagesize(0, 0, SCREEN_W - 1, SCREEN_H - 1));
    if (background == NULL) {
        closegraph();
        return 1;
    }
    getimage(0, 0, SCREEN_W - 1, SCREEN_H - 1, background);

    // show the finished frame
    swapbuffers();

    // -----------------------------------------------------------------------
    // 5. Animated loop: an FPS counter + keyboard handling
    // -----------------------------------------------------------------------
    unsigned long lastTime = GetTickCount();
    float fps = 0;
    bool  running = true;

    while (running) {
        // ---- input ---------------------------------------------------------
        while (kbhit()) {
            int key = getch();
            if (key == 0 || key == 224) getch();   // special key: drop second byte
            if (key == 27) running = false;         // ESC
            if (key == 's' || key == 'S')           // save a screenshot
                writeimagefile("screenshots\\stage1.bmp", 0, 0,
                               SCREEN_W - 1, SCREEN_H - 1, true);
        }

        // ---- update the FPS counter (exponential smoothing) ----------------
        unsigned long now = GetTickCount();
        unsigned long dt  = now - lastTime;
        lastTime = now;
        if (dt > 0) fps = fps * 0.9f + (1000.0f / dt) * 0.1f;

        // ---- draw the small dynamic part on top of the frame ---------------
        // Put the cached static scene back (one fast blit)...
        putimage(0, 0, background, COPY_PUT);

        // ...then draw the panel that hides the previous FPS value.
        setfillstyle(SOLID_FILL, makeColor(8, 9, 16));
        setcolor(makeColor(60, 45, 40));
        bar(SCREEN_W - 200, 20, SCREEN_W - 20, 52);
        setbkcolor(makeColor(8, 9, 16));
        setcolor(makeColor(120, 235, 150));
        char text[64];
        sprintf(text, "FPS  %5.1f", fps);
        outtextxy(SCREEN_W - 185, 31, text);

        // ---- show it --------------------------------------------------------
        swapbuffers();
        delay(16);       // about 60 frames per second
    }

    closegraph();
    return 0;
}
```

### What each part teaches

| Code | Why it matters later |
|---|---|
| `initwindow(w, h, title, x, y, db, close)` | the last two flags are **double buffer** and **allow close** |
| `setrefreshingbgi(false)` | stops WinBGIm repainting after every drawing call |
| `swapbuffers()` | shows the hidden page; one call per frame |
| `imagesize` / `getimage` / `putimage` | the background cache used by the whole project |
| `kbhit` / `getch` | non-blocking keyboard input |
| `GetTickCount` | millisecond clock for frame timing |
| `mixColor` | all gradients and lava heat colours are built with this |

### Compile and run

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage1
Start-Process .\stages\build\stage1.exe
```

### Checkpoint

You should see a 1200Ã—760 window with a blue-black gradient sky, 240 stars, a
moon with a halo, three coloured test cards, and a live FPS counter at the top
right. Press `S` to save `screenshots\stage1.bmp`, press `ESC` to exit.

If the window does not appear, run the console build to see the error message:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage1 -Console
.\stages\build\stage1.exe
```

### Experiments

- Change `drawStars(240)` to `drawStars(600)` and see the sky get busy.
- Replace `mixColor(...)` in `drawMoon` with a constant colour; the halo
  disappears, which proves the blending was doing the soft edge.
- Change `delay(16)` to `delay(50)`; the FPS counter drops to about 20.

---

# Stage 2 â€” the static volcano scene

### Goal

Draw the final night scene **once**: sky, stars, moon, two mountain ranges,
ground, a 14-band shaded stratovolcano, erosion gullies, strata, boulders and
the foreground. Cache the result. No animation yet, except the FPS counter.

Open `stages/stage2_volcano.cpp` next to this page; the listing below shows the
new parts in the order they appear.

### 2.1 Pre-computed sky palette

Stage 1 computed the sky colour for every line. Stage 2 does that once into an
array, so the colour can also be sampled later (smoke transparency, glow, meteors):

```cpp
int skyPalette[SCREEN_H];

void buildSkyPalette()
{
    for (int y = 0; y < SCREEN_H; y++) {
        int c;
        if (y < 170)       c = mixColor(SKY_TOP, SKY_MID,     (float)y / 170.0f);
        else if (y < 380)  c = mixColor(SKY_MID, SKY_LOW,     (float)(y - 170) / 210.0f);
        else if (y < 520)  c = mixColor(SKY_LOW, SKY_HORIZON, (float)(y - 380) / 140.0f);
        else               c = SKY_HORIZON;
        skyPalette[y] = c;
    }
}

int skyColorAt(int y)
{
    if (y < 0) y = 0;
    if (y >= SCREEN_H) y = SCREEN_H - 1;
    return skyPalette[y];
}
```

### 2.2 The volcano is a pair of mathematical curves

Instead of storing a bitmap silhouette, both flanks are functions of `y`.
That same function is reused for collision detection in Stage 3.

```cpp
float leftEdge(float y)
{
    float u = (y - 266.0f) / (float)(GROUND_Y - 266);
    u = clampFloat(u, 0.0f, 1.0f);

    // 0.82 < 1 gives the concave stratovolcano silhouette
    float x = 546.0f - 426.0f * pow(u, 0.82f);
    x += u * (7.0f * sinf(y * 0.055f) + 3.0f * sinf(y * 0.17f));
    x += 26.0f * expf(-powf((y - 470.0f) / 58.0f, 2.0f)) * u;   // shoulder
    return x;
}

float rightEdge(float y)
{
    float u = (y - 266.0f) / (float)(GROUND_Y - 266);
    u = clampFloat(u, 0.0f, 1.0f);

    float x = 654.0f + 436.0f * pow(u, 0.80f);
    x += u * (6.0f * sinf(y * 0.061f + 1.3f) + 3.0f * sinf(y * 0.15f + 0.7f));
    x += 18.0f * expf(-powf((y - 520.0f) / 70.0f, 2.0f)) * u;
    return x;
}
```

`u` runs from 0 at the summit to 1 at the ground. `pow(u, 0.82)` grows faster
near the summit than near the base, which produces the concave profile of a real
stratovolcano. The `sin` terms add small bumps and the Gaussian term adds a
shoulder.

### 2.3 Shading the cone with 14 bands

For every scan line, split the width into 14 vertical bands and light each band
differently. This is a cheap form of **Lambert-style shading** without any 3D
maths:

```cpp
const int BANDS = 14;

for (int y = 266; y <= GROUND_Y; y++) {
    int xL = (int)leftEdge((float)y);
    int xR = (int)rightEdge((float)y);
    if (xR <= xL) continue;

    float depth = (float)(y - 266) / (float)(GROUND_Y - 266);

    for (int b = 0; b < BANDS; b++) {
        int x1 = xL + (xR - xL) * b / BANDS;
        int x2 = xL + (xR - xL) * (b + 1) / BANDS;
        if (x2 <= x1) x2 = x1 + 1;

        float t = (b + 0.5f) / (float)BANDS;
        float light = 0.10f + 0.92f * powf(t, 1.55f);   // dark left -> lit right

        // large-scale variation + fine rock noise
        light += 0.045f * sinf(x1 * 0.017f + y * 0.051f)
               + 0.022f * sinf(x1 * 0.083f - y * 0.019f);
        light *= 0.74f + 0.26f * (1.0f - depth);         // darker near the base

        if (b == 0)         light -= 0.10f;              // dark silhouette
        if (b == BANDS-1)   light += 0.12f;              // moonlit edge

        int c = mixColor(ROCK_DARK, ROCK_LIGHT, clampFloat(light, 0.0f, 1.0f));
        if (b >= BANDS - 2) c = mixColor(c, ROCK_MOON, 0.18f);

        useDrawColor(c);
        line(x1, y, x2, y);
    }
}
```

`light` is just a number between 0 and 1. `mixColor` turns it into a real
colour, and the whole cone is shaded without a single texture.

### 2.4 Details that sell the scene

- **Erosion gullies:** 20 lines that run down the slope, following
  `leftEdge`/`rightEdge` with a per-gully offset.
- **Strata:** 16 nearly horizontal lines, slightly wavy, drawn only inside the
  cone.
- **Oxidised patches:** 7 reddish hexagonal `fillpoly` blobs.
- **Boulders:** 26 small polygons near the base, with a lit top edge.
- **Moon:** 10 faint halo rings (`circle`), then a disc and craters.
- **Mountains:** one `fillpoly` per range, generated with random peaks/valleys.
- **Foreground:** a jagged dark polygon across the bottom with 30 rocks.

### 2.5 Cache the whole scene

```cpp
void buildBackground()
{
    buildSkyPalette();
    srand(20251113);          // deterministic scenery every launch

    drawSky();
    drawStars();
    drawMoon(1030, 210, 42);
    drawDistantRanges();
    drawGround();
    drawVolcano();
    drawForeground();

    unsigned size = imagesize(0, 0, SCREEN_W - 1, SCREEN_H - 1);
    backgroundImage = malloc(size);
    if (backgroundImage == NULL) { closegraph(); exit(1); }

    getimage(0, 0, SCREEN_W - 1, SCREEN_H - 1, backgroundImage);
}

void restoreBackground()
{
    putimage(0, 0, backgroundImage, COPY_PUT);
}
```

The scene needs **tens of thousands** of drawing calls. Repeating them every
frame would be far too slow. `getimage` stores the finished pixels; `putimage`
copies them back in one operation.

### Compile and run

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage2
Start-Process .\stages\build\stage2.exe
```

The stage supports a screenshot flag for automated checks:

```powershell
Start-Process .\stages\build\stage2.exe -ArgumentList '--shot','stage2_scene.bmp' -Wait
```

### Checkpoint

A moonlit volcano with a visible **left-dark / right-lit** gradient, a dark
crater, layered mountains, scattered boulders and a foreground silhouette.
Nothing moves except the FPS counter. The image should look exactly like
`screenshots/stage2_scene.png`.

---

# Stage 3 â€” lava: clock, glow, rivers and bombs

### Goal

Turn the static scene into a living eruption:

- a **delta-time** clock so animation speed does not depend on frame rate;
- a pulsing crater with a hot lava lake and light rays;
- two **lava rivers** that flow down the flanks;
- **lava bombs** with gravity, drag, cooling and lifetime;
- glowing **pools** where the rivers reach the plain.

### 3.1 Delta time

Every frame measures how long the previous frame took:

```cpp
unsigned long frameStart = GetTickCount();
float dt = (frameStart - lastFrame) / 1000.0f;
lastFrame = frameStart;
dt = clampFloat(dt, 0.001f, 0.05f);   // never 0, never a huge jump
```

`dt` is in **seconds**. A particle that moves `8 pixels per frame at 60 FPS`
moves `8 * dt * 60` pixels in the new code, so it behaves the same at 30 FPS.

The simulation keeps a global clock:

```cpp
simTime += dt;
```

All pulsing effects are functions of `simTime`, for example:

```cpp
float pulse = 0.55f + 0.45f * sinf(simTime * 4.0f) * glowLevel;
```

### 3.2 Smooth glow level

Instead of switching brightness instantly, `glowLevel` chases a target:

```cpp
float targetGlow = erupting ? (0.45f + 0.055f * intensity) : 0.18f;
glowLevel += (targetGlow - glowLevel) * 0.045f;
```

This is a one-pole low-pass filter. When the eruption stops, the crater, rivers
and pools cool down over about a second instead of snapping to black.

### 3.3 Crater and lava lake

The crater is drawn in layers:

1. **Rays** â€” 7 thin triangles (`fillpoly`) that lean and change length with
   `simTime`.
2. **Glow dome** â€” 9 concentric ellipses blended towards orange; earlier
   ellipses are wider and fainter.
3. **Dark opening** â€” one dark ellipse.
4. **Inner wall** â€” 8 upper-half `sector` calls, dark to warm.
5. **Lava lake** â€” three ellipses (outer crust, body, white-hot centre), whose
   brightness depends on `glowLevel`.
6. **Rim arcs** â€” upper-half ellipses, then the front lip.

The lake cooling formula is important:

```cpp
float lakeHeat = clampFloat((glowLevel - 0.15f) / 0.65f, 0.0f, 1.0f);
```

When `glowLevel` falls to 0.15 the lake is cold; at 0.80 it is fully hot.

### 3.4 Lava rivers

A river is a polyline of 16 points that follows the cone. The x coordinate is
the cone edge plus an offset that grows near the bottom, so the river bends
outwards like real lava:

```cpp
int y = 279 + i * 27;
float u = (float)(y - 266) / (float)(GROUND_Y - 266);
float pull = (u > 0.62f) ? (u - 0.62f) / 0.38f : 0.0f;
pull *= pull;

flows[f].x[i] = (int)(leftEdge((float)y) + 30.0f + 62.0f * pull)
                + (int)(4.0f * sinf(i * 1.7f));
flows[f].y[i] = y;
```

Each river is drawn **three times** with decreasing width to fake a crust, a
body and a hot core:

```cpp
int widths[3] = { 9, 6, 3 };
```

Eleven bright blobs slide along the polyline. Their position is

```cpp
float p = fmodf(simTime * 0.38f + k / 11.0f + fl.phase, 1.0f);
```

`p` is a fraction of the river length; `fmodf` wraps it, so the blobs keep
circulating. The blobs are interpolated between two polyline points:

```cpp
int x = (int)(fl.x[i] + (fl.x[i + 1] - fl.x[i]) * t);
int y = (int)(fl.y[i] + (fl.y[i + 1] - fl.y[i]) * t);
```

### 3.5 Lava bombs

Bombs live in a fixed array of 340 slots. Spawning scans for the first inactive
slot (no dynamic allocation, no fragmentation):

```cpp
struct LavaBomb {
    float x, y;          // position
    float vx, vy;        // velocity
    float life, maxLife; // seconds
    float heat;          // 0 cold .. 1 white hot
    int   size;          // radius
    bool  active;
};

void spawnLavaBomb()
{
    float power = 0.65f + intensity * 0.07f;

    for (int i = 0; i < MAX_LAVA; i++) {
        if (!bombs[i].active) {
            bombs[i].active  = true;
            bombs[i].x       = CRATER_X + randomFloat(-20.0f, 20.0f);
            bombs[i].y       = CRATER_Y - 2 + randomFloat(-3.0f, 4.0f);
            bombs[i].vx      = randomFloat(-3.8f, 3.8f) * power;
            bombs[i].vy      = -randomFloat(4.6f, 8.8f) * power;
            bombs[i].life    = randomFloat(1.5f, 3.4f);
            bombs[i].maxLife = bombs[i].life;
            bombs[i].heat    = randomFloat(0.75f, 1.0f);
            bombs[i].size    = randomInt(1, 2);
            return;
        }
    }
}
```

Update (simple Euler integration):

```cpp
bombs[i].x  += bombs[i].vx;
bombs[i].y  += bombs[i].vy;
bombs[i].vy += 0.145f;          // gravity
bombs[i].vx *= 0.995f;          // air drag
bombs[i].life -= dt;

float age = 1.0f - bombs[i].life / bombs[i].maxLife;
bombs[i].heat = clampFloat(1.0f - age * 0.9f, 0.05f, 1.0f);
```

A bomb dies when its life runs out, when it hits the surface, or when it leaves
the screen. Collision uses the same curve functions as the drawing:

```cpp
if (bombs[i].life <= 0 ||
    bombs[i].y > surfaceYAt(bombs[i].x) + 4 ||
    bombs[i].x < -30 || bombs[i].x > SCREEN_W + 30)
    bombs[i].active = false;
```

`surfaceYAt(x)` returns `CRATER_Y + CRATER_RY` inside the crater (so bombs fall
back in), the flank height outside it, and the ground near the edges.

### 3.6 Drawing bombs efficiently

Changing the GDI brush hundreds of times per frame is expensive. Bombs are
grouped by the six lava colours and each group is drawn in one pass:

```cpp
for (int c = 0; c < 6; c++) {
    useFillColor(LAVA_COLORS[c]);
    useDrawColor(LAVA_COLORS[c]);
    for (int i = 0; i < MAX_LAVA; i++) {
        if (!bombs[i].active) continue;
        int idx = (int)((1.0f - bombs[i].heat) * 5.0f);
        if (idx != c) continue;
        ...
    }
}
```

The outer glow and the bright core are drawn as two `fillellipse` calls.

### Compile and run

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage3
Start-Process .\stages\build\stage3.exe
```

### Checkpoint

The crater pulses, rays flicker above the summit, two orange rivers run down the
flanks with sliding blobs, bombs fly out and fall back, and two lava pools glow
at the foot. The FPS counter is still visible. Compare with
`screenshots/stage3_lava.png`.

### Experiments

- Change gravity `0.145f` to `0.25f`: bombs fall faster and land earlier.
- Change `MAX_LAVA` from 340 to 80: fewer bombs, slightly faster frame.
- Set the river widths to `{ 5, 3, 1 }`: thin streams instead of thick lava.

---

# Stage 4 â€” smoke, ash, embers, meteor and statistics

### Goal

Add atmosphere and information:

- **smoke** puffs that rise, drift with the wind and expand;
- **ash** flakes that fall and fade;
- **embers** that flicker inside the plume;
- a rare **meteor** in the sky;
- a live **statistics panel** (FPS, bomb count, puff count, simulation time).

### 4.1 Smoke puffs

Smoke uses the same pool pattern as bombs:

```cpp
struct SmokePuff {
    float x, y;
    float vx, vy;
    float radius;
    float grow;
    float life, maxLife;
    float wobble;        // tiny per-puff randomness for a lumpy shape
    bool  active;
};
```

Spawn:

```cpp
smoke[i].x      = CRATER_X + randomFloat(-26.0f, 26.0f);
smoke[i].y      = CRATER_Y - 8 + randomFloat(-4.0f, 6.0f);
smoke[i].vx     = wind + randomFloat(-0.22f, 0.22f);
smoke[i].vy     = -randomFloat(0.55f, 1.25f) * (0.8f + intensity * 0.05f);
smoke[i].radius = randomFloat(7.0f, 13.0f);
smoke[i].grow   = randomFloat(0.26f, 0.52f);
smoke[i].life   = randomFloat(4.5f, 8.0f);
```

Update adds buoyancy and a slow sideways wobble:

```cpp
smoke[i].x += smoke[i].vx * dt * 60.0f;
smoke[i].y += smoke[i].vy * dt * 60.0f;
smoke[i].vy -= 0.004f * dt * 60.0f;                       // rises faster
smoke[i].vx += 0.0015f * sinf(simTime * 0.7f + i) * dt * 60.0f;
smoke[i].x += 0.18f * sinf(simTime * 1.1f + i * 2.3f);    // wobble
smoke[i].radius += smoke[i].grow * dt * 60.0f;
smoke[i].life -= dt;
```

`wind` is global and changes very slowly:

```cpp
wind = -0.22f + 0.30f * sinf(simTime * 0.07f);
```

### 4.2 Fake transparency

WinBGIm has no alpha channel. The trick is to **blend the smoke colour with the
sky colour first**, then draw an opaque ellipse:

```cpp
int skyRef = mixColor(skyColorAt(250), ROCK_MID, 0.22f);
int bases[3] = { warm, middle, cool };

for (int layer = 0; layer < 3; layer++) {
    for (int band = 0; band < 3; band++) {
        useFillColor(mixColor(skyRef, bases[band], 0.10f + layer * 0.13f));
        ...
        fillellipse(x + ox, y + oy, r, (int)(r * 0.86f));
    }
}
```

Each puff is drawn three times: a big faint outer ellipse, a medium one and a
small dense one. The age of the puff selects one of three colour bands (hot,
grey, cool), which makes the plume look layered.

### 4.3 Ash and embers

- **Ash** uses `putpixel`. Add flakiness with a slow sine on `vx` and a small
  size check that sometimes draws a second pixel.
- **Embers** flicker by comparing `sinf(simTime * 11.0f + phase)` with a
  threshold; they are drawn only when the flicker is bright enough.

```cpp
float flicker = 0.5f + 0.5f * sinf(simTime * 11.0f + embers[i].phase);
if (flicker > 0.35f)
    putpixel((int)embers[i].x, (int)embers[i].y,
             (flicker > 0.75f) ? COLOR(255, 226, 130) : COLOR(244, 130, 40));
```

### 4.4 Meteor

A meteor is a rare event, triggered by a random check:

```cpp
} else if (rand() % 900 == 0) {
    meteor.active = true;
    meteor.x = randomFloat(200.0f, 1150.0f);
    meteor.y = randomFloat(40.0f, 190.0f);
    meteor.vx = -randomFloat(5.0f, 8.0f);
    meteor.vy = randomFloat(2.5f, 4.0f);
    meteor.life = randomFloat(0.7f, 1.2f);
}
```

Its tail is nine short lines; each one is blended further towards the sky
colour, which produces the fading trail.

### 4.5 Statistics panel

```cpp
int lavaCount = 0, smokeCount = 0;
for (int i = 0; i < MAX_LAVA; i++)  if (bombs[i].active) lavaCount++;
for (int i = 0; i < MAX_SMOKE; i++) if (smoke[i].active) smokeCount++;

int minutes = (int)simTime / 60;
int seconds = (int)simTime % 60;
```

The panel shows FPS, active lava bombs, active smoke puffs and `MM:SS`. Only
**active** slots are counted, which proves the pool reuse works.

### Compile and run

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage4
Start-Process .\stages\build\stage4.exe
```

### Checkpoint

Grey-brown smoke drifts left from the crater, ash falls, embers flicker, and the
statistics panel counts live particles. A meteor occasionally crosses the sky.
Compare with `screenshots/stage4_particles.png`.

---

# Stage 5 â€” controls, HUD and the finished program

### Goal

The last stage turns the demo into an interactive application:

- `SPACE` start/stop, `P` pause, `R` reset, `+`/`-` intensity, `S` screenshot,
  `ESC` exit;
- a title panel, an intensity meter, a statistics panel and a control bar;
- a pause banner and a screenshot confirmation;
- the performance wrappers that keep the frame rate smooth.

Stage 5 is a mirror of `src/volcano.cpp`, so the final program and the last
tutorial step behave identically.

### 5.1 Non-blocking input

```cpp
void handleInput()
{
    while (kbhit()) {
        int key = getch();

        // WinBGIm sends 0 (or 224) before special keys such as arrows.
        if (key == 0 || key == 224) { getch(); continue; }

        switch (key) {
            case 27:  running = false; break;                 // ESC
            case ' ': erupting = !erupting; break;            // SPACE
            case 'p': case 'P': paused = !paused; break;
            case 'r': case 'R': resetSimulation(); break;
            case '+': case '=': if (intensity < 10) intensity++; break;
            case '-': case '_': if (intensity > 1)  intensity--; break;
            case 's': case 'S':
                writeimagefile("volcanox_shot.bmp", 0, 0,
                               SCREEN_W - 1, SCREEN_H - 1, true);
                sprintf(screenshotMessage, "Saved volcanox_shot.bmp");
                screenshotMessageUntil = GetTickCount() + 2500;
                break;
        }
    }
}
```

`while (kbhit())` drains the queue, so holding a key does not build up a backlog.
`getch()` never blocks here because `kbhit()` said a key is waiting.

### 5.2 Reset

`resetSimulation()` deactivates every particle and restores every global:

```cpp
void resetSimulation()
{
    clearParticles();
    erupting   = true;
    paused     = false;
    intensity  = 5;
    simTime    = 0.0f;
    glowLevel  = 0.60f;
    spawnAccum = 0.0f;
    smokeAccum = 0.0f;
    ashAccum   = 0.0f;
    nextBurst  = 2.0f;
    wind       = -0.25f;
    screenshotMessage[0] = '\0';
}
```

### 5.3 Spawn rates that do not depend on frame rate

Spawning uses **fractional accumulators**. At 60 FPS a rate of `3.35 per frame`
adds 3.35 to the accumulator; each whole unit spawns one particle:

```cpp
spawnAccum += (0.6f + intensity * 0.55f) * dt * 60.0f;
while (spawnAccum >= 1.0f) {
    spawnLavaBomb();
    spawnAccum -= 1.0f;
}
```

At 30 FPS the same code runs half as often, but adds twice as much each time, so
the average spawn rate is identical. The same pattern is used for smoke and ash.

### 5.4 The HUD

All text is redrawn every frame so it always sits on top of the particles. The
HUD is built from:

- a title panel (`bar`, `rectangle`, accent stripes, two `outtextxy` calls for
  fake bold);
- a status row whose colour depends on `paused` / `erupting`;
- a 10-segment intensity meter;
- a statistics panel;
- a bottom control bar with key badges;
- a pause banner and a screenshot toast.

**Text background matters.** WinBGIm text is opaque, so every panel calls
`setbkcolor` to match its own background before drawing. The final code caches
that too:

```cpp
void useTextBackground(int c)
{
    if (c != cachedTextBg) { setbkcolor(c); cachedTextBg = c; }
}
```

### 5.5 Performance wrappers

On this library every `setcolor`, `setfillstyle`, `setlinestyle`,
`settextstyle` and `setbkcolor` rebuilds GDI objects for **all four pages**.
The program calls them thousands of times per frame, so it remembers the last
value and only changes it when needed:

```cpp
void useDrawColor(int c)
{
    if (c != cachedDrawColor) { setcolor(c); cachedDrawColor = c; }
}

void useFillColor(int c)
{
    if (c != cachedFillColor) { setfillstyle(SOLID_FILL, c); cachedFillColor = c; }
    // WinBGIm also strokes filled shapes with the DRAW colour.
    if (c != cachedDrawColor) { setcolor(c); cachedDrawColor = c; }
}
```

The second line of `useFillColor` is a real bug fix: `fillellipse`, `sector` and
`fillpoly` draw their outline with the **draw** colour, not the fill colour. If
the previous shape was grey, a dark smoke puff would get a grey border.
`tools/filltest.cpp` is a tiny program that demonstrates this behaviour.

### 5.6 Frame loop

```cpp
while (running) {
    unsigned long frameStart = GetTickCount();
    float dt = (frameStart - lastFrame) / 1000.0f;
    lastFrame = frameStart;
    dt = clampFloat(dt, 0.001f, 0.05f);

    handleInput();
    if (!paused) updateSimulation(dt);

    restoreBackground();
    drawSmoke();          // behind the crater rim
    drawCrater();
    drawLavaFlows();
    drawGroundPools();
    drawLavaBombs();
    drawAsh();
    drawEmbers();
    drawMeteor();

    float instant = 1.0f / dt;
    fps = fps * 0.90f + instant * 0.10f;

    drawHud();
    swapbuffers();

    unsigned long spent = GetTickCount() - frameStart;
    if (spent < 16) delay((int)(16 - spent));   // cap at ~60 FPS
}
```

The **draw order** is deliberate: smoke is behind the crater so the lava lake
stays visible; bombs and ash are in front of the mountain; the HUD is last.

### 5.7 Screenshot and developer flags

`writeimagefile("volcanox_shot.bmp", ...)` saves the **active** page. Because it
is called before `swapbuffers()`, it captures the frame that is about to be
shown.

The main function also accepts flags used for automated testing:

```text
--bench N        render N frames, print ms/frame, exit
--shot file      save the last frame (BMP, despite the extension)
--intensity N    force intensity 1..10 at startup
--paused         start paused
--stopped        start with the eruption off
```

Example:

```powershell
Start-Process .\volcanox.exe -ArgumentList '--bench','300','--shot','frame.bmp' -Wait
```

### Compile and run

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage5
Start-Process .\stages\build\stage5.exe
```

### Checkpoint

Everything from Stages 1â€“4 plus a complete HUD. Test every key:

| Key | Expected result |
|---|---|
| `SPACE` | crater cools and bombs stop spawning; existing particles finish |
| `P` | "PAUSED" banner, simulation frozen, rendering continues |
| `R` | particles cleared, intensity back to 5, status ACTIVE |
| `+` / `-` | intensity meter changes, more/fewer bombs and smoke |
| `S` | toast "Saved volcanox_shot.bmp" for 2.5 s |
| `ESC` | window closes cleanly |

Compare with `screenshots/stage5_controls.png` and the final
`screenshots/volcanox_active.png`.

---

# Final build and benchmark

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Target all

# timing (console build prints the result)
powershell -ExecutionPolicy Bypass -File build.ps1 -Target final -Console
.\volcanox.exe --bench 300
```

Expected on the development laptop at 1200Ã—760:

| State | Time per frame | Approx. FPS |
|---|---:|---:|
| Active eruption (intensity 5) | 12.2 ms | 82 |
| Maximum intensity | 12.4 ms | 80 |
| Paused | 6.3 ms | 159 |
| Stopped (cooling) | 5.9 ms | 169 |

The interactive loop caps itself at 60 FPS, so the extra headroom keeps the
animation smooth even on slower machines.

---

# Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `graphics.h: No such file` | run from the project root; `-I include` is missing |
| `undefined reference to ...` | a library is missing from the link line; copy the full line from this page |
| `cannot find -lbgi` | `lib/libbgi.a` is missing; see SETUP.md |
| Window opens then closes instantly | a `printf`/`exit` path was hit; build with `-Console` to read the message |
| Black window | `getmaxx()` check failed or the scene was drawn before `initwindow` |
| Flicker | `setrefreshingbgi(false)` or `swapbuffers()` missing |
| Filled shapes have grey borders | use `useFillColor()`, which also sets the draw colour |
| Text has black boxes | `setbkcolor` does not match the panel colour |
| Keys do nothing | the window must have focus; click once inside it |
| Program seems to freeze | `getch()` was called without `kbhit()`, or a loop has no exit |
| Screenshot is black | call `writeimagefile` before `swapbuffers`, or run without `-mwindows` issues (see note below) |

> **Screenshots with `-mwindows`:** the GUI build has no console, so a script
> that launches it must use `Start-Process -Wait`, otherwise the next command
> runs while the window is still opening and the file may not exist yet.

---

# Stage checklist

Use this before a lab evaluation:

- [ ] `build.ps1 -Target all` finishes with `Build OK`.
- [ ] Stage 1 window opens and `ESC` exits.
- [ ] Stage 2 scene matches the screenshot.
- [ ] Stage 3 lava flows, bombs and pools animate.
- [ ] Stage 4 smoke, ash, embers and statistics are visible.
- [ ] Stage 5 every key works.
- [ ] Final `volcanox.exe` runs by double-click (no DLL errors).
- [ ] `--bench 300` prints a timing line.
- [ ] `S` saves `volcanox_shot.bmp`.
- [ ] README, SETUP, TUTORIAL, ALGORITHM, REPORT and VIVA are present.
