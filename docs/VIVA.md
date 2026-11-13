# Viva Questions and Answers — VolcanoX

Forty-five questions covering the graphics library, the design, the mathematics,
the implementation and the engineering decisions. Answers are written the way
you would say them aloud in a lab viva: short, direct, then one sentence of
justification.

---

## A. Graphics library fundamentals

### 1. What is `graphics.h`?

`graphics.h` is the header of the **Borland Graphics Interface (BGI)**, a
classic 2D graphics library used with Turbo C/C++. It provides primitives such as
`line`, `circle`, `rectangle`, `fillellipse`, `fillpoly`, `outtextxy` and
functions for colours, fonts and viewports. Our project uses **WinBGIm**, a
Windows reimplementation of the same API, so the original BGI calls work on
modern 64-bit Windows.

### 2. What is WinBGIm and why did you use it instead of OpenGL?

WinBGIm is a BGI-compatible layer built on top of the Windows GDI/GDI+. It gives
us the textbook `graphics.h` functions in a normal window, with double buffering
and mouse/keyboard support. We used it because the course requires
`graphics.h`; OpenGL or DirectX would be a different assignment and would hide
the low-level drawing algorithms we are supposed to demonstrate.

### 3. Why does the project include its own `libbgi.a` and headers?

The `libbgi.a` distributed with old tutorials is 32-bit. A 64-bit MinGW compiler
cannot link against it — the pointer sizes and calling conventions differ. We
therefore built the 64-bit WinBGIm-64 source into `lib/libbgi.a` with the same
GCC 16 compiler used for the application, and copied the three headers into
`include/`. That makes the project self-contained and avoids link errors.

### 4. What does `initwindow()` do?

```cpp
initwindow(1200, 760, "VolcanoX", 90, 15, true, true);
```

It creates the graphics window. The parameters are width, height, title,
left/top screen position, **double-buffer flag** and **close-button flag**. With
the double-buffer flag set, drawing goes to a hidden page until `swapbuffers()`
is called.

### 5. What is double buffering and why is it needed?

Double buffering means drawing the whole frame into an **off-screen page** and
then presenting it in one operation. Without it, the user sees each primitive
appear one at a time, which looks like flicker. In WinBGIm:

```cpp
initwindow(..., true, true);   // enable double buffer
setrefreshingbgi(false);       // do not repaint after every call
// ... draw the frame ...
swapbuffers();                 // present it
```

### 6. What does `setrefreshingbgi(false)` do?

It disables WinBGIm's automatic refresh after each drawing call. We refresh
exactly once per frame with `swapbuffers()`. Without it, every `line` or
`fillellipse` would trigger a repaint and the frame rate would collapse.

### 7. What does `swapbuffers()` do?

It swaps the hidden drawing page with the visible page, so the frame just drawn
becomes visible. It is the last graphics call in the loop, apart from timing.

### 8. How do `imagesize`, `getimage` and `putimage` work?

- `imagesize(x1,y1,x2,y2)` returns how many bytes the rectangle needs.
- `getimage(...)` copies the pixels into a memory buffer.
- `putimage(x,y,buffer,COPY_PUT)` blits the buffer back.

We use them to cache the entire static scene in one bitmap. Each frame starts
with one `putimage`, which is far faster than redrawing thousands of
primitives.

### 9. What is the colour format of `COLOR(r,g,b)` in WinBGIm?

It returns a Windows `COLORREF` in the form `0x03BBGGRR`:

```text
bits 23..16 = blue
bits 15..8  = green
bits 7..0   = red
```

That is why the code extracts channels with `c & 0xFF`, `(c >> 8) & 0xFF` and
`(c >> 16) & 0xFF`.

### 10. What is the difference between `setcolor` and `setfillstyle`?

`setcolor` sets the **outline / draw** colour used by lines, circles and text.
`setfillstyle(style, colour)` sets the **interior** colour and pattern for
filled shapes. In WinBGIm, filled shapes also use the current draw colour for
their outline, so the project's `useFillColor()` sets both.

### 11. What does `sector()` do?

`sector(x, y, startAngle, endAngle, rx, ry)` fills a pie/elliptical sector
between two angles. We use `sector(x, y, 0, 180, rx, ry)` for the crater's inner
wall; in this environment that draws the upper half of an ellipse.

### 12. What does `fillpoly()` need?

A count and an `int` array of `2 × count` coordinates: `{x1,y1, x2,y2, ...}`.
The last point is connected back to the first automatically, so the array does
not need to repeat the starting point (though some of our arrays do, which is
also valid).

### 13. How is text drawn, and why does `setbkcolor` matter?

`outtextxy(x, y, text)` draws text at a position using the current font, draw
colour and background colour. WinBGIm text is **opaque**, so the background
colour must match the panel behind the text, otherwise the text appears in a
coloured box. Every HUD panel sets `setbkcolor` to its own background before
drawing text.

---

## B. Design and data structures

### 14. Why did you use structures and arrays instead of classes and `std::vector`?

The requirement is beginner-friendly C++ with structures and arrays. Fixed
arrays make the maximum memory use known at compile time, avoid allocation in
the animation loop, and keep iteration cache-friendly. A `struct` groups the
related fields (position, velocity, life, heat, active), which is exactly what a
particle needs.

### 15. What is the `active` flag for?

It marks whether an array slot currently holds a live particle. Spawning scans
for the first slot with `active == false`; updating and drawing skip inactive
slots; resetting sets every flag to false. It is a simple free-list without
pointers.

### 16. How many particles can the program handle?

| Particle | Capacity |
|---|---:|
| Lava bombs | 340 |
| Smoke puffs | 56 |
| Ash flakes | 80 |
| Embers | 18 |
| Stars | 250 |

If every bomb slot is busy, an extra spawn is simply dropped. There is no
overflow and no crash.

### 17. Why are the static stars stored but never updated?

They are generated once while building the background and then baked into the
cached bitmap. There is no need to update them, so the `Star` array is only used
during scene construction. It is kept as a structure to make the star-generation
code readable.

### 18. What is the role of `simTime`?

It is the simulation clock in seconds, advanced by `dt` each frame. All periodic
effects — crater pulse, river blobs, smoke wobble, ember flicker, meteor — are
functions of `simTime`. Using a single clock keeps the effects synchronised and
makes them easy to test.

### 19. What is `glowLevel`?

A smoothed 0..1 value representing how hot the volcano is. It chases a target:

```cpp
targetGlow = erupting ? 0.45 + 0.055*intensity : 0.18;
glowLevel += (targetGlow - glowLevel) * 0.045;
```

It controls the crater lake, rim, rivers, pools and rays, so the whole volcano
cools together instead of switching off instantly.

### 20. How does pause differ from stop?

- **Stop** (`SPACE`): `erupting = false`. No new bombs or smoke are spawned, but
  existing particles continue to move and die, and the crater slowly cools.
- **Pause** (`P`): `paused = true`. The simulation update is skipped entirely,
  so the scene freezes, while rendering and the HUD continue. A banner is shown.

They are independent, so the user can pause a stopped volcano or stop a paused
one after resuming.

---

## C. Mathematics and physics

### 21. How is the volcano shape generated?

The two flanks are functions of the vertical coordinate:

```text
u = (y - 266) / 424
left  = 546 - 426·u^0.82 + bumps + shoulder
right = 654 + 436·u^0.80 + bumps + shoulder
```

The exponent below 1 makes the profile concave, like a stratovolcano. Sine terms
add irregularity and a Gaussian term adds a shoulder. Because the shape is a
formula rather than a bitmap, it can also be used for collision detection.

### 22. How is the volcano shaded?

Each scan line is split into 14 horizontal bands. A brightness value is
computed from the band position, two sine noise terms and the depth down the
cone, then converted to a colour with `mixColor(ROCK_DARK, ROCK_LIGHT, light)`.
The left edge is darkened and the right edge is brightened to match the moon on
the right.

### 23. What is `mixColor` and where is it used?

It linearly interpolates two colours per channel:

```text
result = c1 + (c2 - c1) * t,   t clamped to 0..1
```

It is used for the sky gradient, the cone shading, the crater glow, the lava
lake, the rivers, the smoke, the meteor tail and the HUD intensity meter.
Essentially every smooth colour transition goes through it.

### 24. How do you fake transparency without an alpha channel?

By blending the object colour with the **known background colour** before
drawing it opaquely:

```text
visible = object·α + background·(1 − α)
```

For smoke, the background is the sky, whose colour is available from
`skyColorAt(y)`. This works because the plume is always over the sky. It would
not work over arbitrary scenery, which is why we do not use it for bombs.

### 25. How does the lava river animate?

The river geometry is static: a 16-point polyline built from the cone edges.
Eleven bright blobs per river move along it. Their normalised position is

```text
p = frac(0.38·simTime + k/11 + phase)
```

and they are interpolated between the two nearest polyline points. The river
itself does not move; only the blobs do.

### 26. What physics do the lava bombs use?

Simple Euler integration:

```text
vy = vy + 0.145      // gravity
vx = vx * 0.995      // drag
x  = x + vx
y  = y + vy
```

Heat decays with age, and the bomb is removed when its lifetime ends, when it
hits the terrain, or when it leaves the screen.

### 27. How is bomb-terrain collision detected?

`surfaceYAt(x)` returns the surface height at a given x. It inverts the same
edge formulas used for drawing, returns the crater rim height inside the crater
interval, and clamps to the ground near the edges. A bomb is removed when
`y > surfaceYAt(x) + 4`.

### 28. Why do bombs fall back into the crater?

For `x` between 552 and 648, `surfaceYAt` returns `CRATER_Y + CRATER_RY` — the
rim/lake level. A bomb that falls inside that x-range therefore disappears at
the lake instead of passing through the mountain.

### 29. What is delta time and why is it important?

`dt` is the time in seconds since the previous frame:

```cpp
dt = (now - lastFrame) / 1000.0f;
dt = clamp(dt, 0.001f, 0.05f);
```

Motion written as `x += vx * dt * 60` is **frame-rate independent**: the same
distance is covered per second whether the program runs at 30 or 120 FPS. The
clamp prevents division by zero and huge jumps after a debugger pause.

### 30. Why are spawn rates multiplied by `dt * 60`?

Particle velocities and rates were tuned at 60 FPS. `dt * 60` converts a
per-frame value into a per-second value. At 60 FPS the factor is 1; at 30 FPS it
is 2, so the same number of particles is produced per second.

### 31. How do fractional accumulators work?

The spawn rate is not an integer. The program adds the fractional amount to an
accumulator every frame and spawns while it is at least 1:

```text
accum += rate * dt * 60
while accum >= 1:
    spawn()
    accum -= 1
```

The leftover fraction is kept, so the long-run average is exact and no particle
is lost to rounding.

### 32. Why is the bomb motion not multiplied by `dt` while its life is?

The bomb flight is a per-frame Euler integration tuned to look right at 60 FPS,
and the interactive loop caps itself at 60 FPS. The **life timer** is in seconds
so that it remains correct if a frame is slow. This is a deliberate trade-off
between visual tuning and timing correctness, and it is documented in the code
and report. A fully time-scaled version is listed as future work.

---

## D. Rendering and performance

### 33. Why is the background cached?

The static scene contains tens of thousands of primitives: 760 sky lines, 250
stars, mountain polygons, 14 shaded bands for each of 424 scan lines, gullies,
strata, boulders and rocks. Redrawing all of that every frame would be far too
slow. It is drawn once and restored with a single `putimage`.

### 34. What is the draw order and why does it matter?

```text
background → smoke → crater → rivers → pools → bombs → ash → embers →
meteor → HUD → swapbuffers
```

Smoke is drawn **before** the crater so the opaque puffs do not cover the lava
lake and the vent stays visible. The HUD is last so no particle can draw over
the interface. Bombs and ash are in front of the mountain because they are
closer to the viewer.

### 35. Why group particles by colour when drawing?

Every `setcolor` / `setfillstyle` call in WinBGIm recreates GDI objects for all
four pages. Drawing 340 bombs with individual colour changes would mean up to
680 state changes per frame. Grouping by the six lava colours reduces that to
six. Smoke is grouped by layer and age band for the same reason — 9 changes
instead of 168.

### 36. What do the `useDrawColor`, `useFillColor`, etc. wrappers do?

They cache the last value and only call the real WinBGIm function when the value
changes:

```cpp
void useDrawColor(int c) {
    if (c != cachedDrawColor) { setcolor(c); cachedDrawColor = c; }
}
```

This avoids thousands of redundant GDI object creations per second. There are
similar wrappers for fill colour, line thickness, font size and text
background.

### 37. Why does `useFillColor` also call `setcolor`?

Because WinBGIm draws the **outline** of filled shapes with the current draw
colour. If the previous shape left a grey draw colour, a dark smoke puff would
get a grey border. Setting both colours to the same value removes that artefact.
`tools/filltest.cpp` is a small proof program for this behaviour.

### 38. How is the frame rate capped?

After `swapbuffers()`, the loop measures how long the frame took and sleeps for
the remainder of 16 ms:

```cpp
unsigned long spent = GetTickCount() - frameStart;
if (spent < 16) delay((int)(16 - spent));
```

This targets about 60 FPS and prevents the CPU from spinning at full speed.

### 39. How is FPS measured?

Instantaneous FPS is `1 / dt`; the display uses exponential smoothing:

```cpp
fps = fps * 0.90f + (1.0f / dt) * 0.10f;
```

The smoothing makes the number readable instead of jumping on every frame.

### 40. What are the measured timings?

At 1200 × 760 with `-O2`:

| State | ms/frame |
|---|---:|
| Active, intensity 5 | 12.2 |
| Active, intensity 10 | 12.4 |
| Paused | 6.3 |
| Stopped | 5.9 |

The 16 ms budget gives about 60 FPS with headroom.

### 41. Why is the program statically linked?

`-static` includes the compiler runtime in the executable, so it does not need
MinGW DLLs on the target machine. This makes `volcanox.exe` portable: it runs by
double-click on a clean Windows 10/11 system. `-mwindows` additionally marks it
as a GUI application so no console window appears.

---

## E. Interaction, testing and limitations

### 42. How is keyboard input read without blocking?

```cpp
while (kbhit()) {
    int key = getch();
    ...
}
```

`kbhit()` returns non-zero when a key is waiting, so `getch()` never blocks. The
`while` loop drains the entire queue, which prevents a backlog when a key is
held down. Special keys arrive as two bytes (first `0` or `224`); the handler
consumes the second byte and ignores the key.

### 43. How does the screenshot feature work?

`S` calls:

```cpp
writeimagefile("volcanox_shot.bmp", 0, 0, SCREEN_W - 1, SCREEN_H - 1, true);
```

It saves the active page — called before `swapbuffers()`, so it captures the
frame about to be shown. WinBGIm always writes **BMP**, regardless of the file
extension. A toast confirms the save for 2.5 seconds.

### 44. Why can a GUI-subsystem program seem to "finish instantly" in PowerShell?

`-mwindows` builds a GUI application, so Windows detaches it from the console.
PowerShell returns immediately while the window is still open. Scripts must use
`Start-Process -Wait`, or build with `-Console` to keep the process attached.
This mattered for the automated screenshot and benchmark runs.

### 45. What are the main limitations and how would you fix them?

| Limitation | Fix |
|---|---|
| fixed 1200 × 760 layout | virtual coordinates plus a scale transform |
| CPU-only rendering | port the draw layer to SDL/raylib or a GPU API |
| no true alpha | render to an off-screen buffer and blend pixels manually |
| BMP-only screenshots | convert the buffer to PNG in code |
| per-frame bomb physics | scale velocity by `dt` and re-tune the constants |
| no save/load | serialise the global state to a small binary file |
| no sound | add `PlaySound` or an audio library |

---

## F. Quick "one-liner" answers

- **Window size?** 1200 × 760 at screen position (90, 15).
- **Crater centre?** (600, 271).
- **Number of shading bands?** 14.
- **Number of stars?** 250.
- **Number of lava rivers?** 2, each with 16 points.
- **River passes?** 3 (crust, body, core), widths 9/6/3.
- **Maximum bombs?** 340.
- **Gravity constant?** 0.145 per frame.
- **Drag?** 0.995 per frame.
- **Smoke layers per puff?** 3.
- **Smoke colour bands?** 3 (warm, grey, cool).
- **Pause key?** `P`.
- **Reset key?** `R`.
- **Intensity range?** 1–10, default 5.
- **Screenshot key?** `S`, writes `volcanox_shot.bmp`.
- **Libraries linked?** `bgi, gdiplus, comdlg32, gdi32, uuid, oleaut32, ole32, winmm`.
- **Language standard?** C++17.
- **Measured active frame time?** about 12.2 ms.
- **Heap use?** under 4 MB, mostly the background bitmap.

---

## G. Advice for the viva

1. **Know the draw order** — it is the question most likely to expose whether
   you actually wrote the program.
2. **Be able to explain one formula** — `mixColor`, the cone edges or the bomb
   integration. Pick one and practise it.
3. **Admit the trade-offs** — fixed resolution, fake alpha, per-frame physics.
   A clear limitation answer is stronger than pretending there are none.
4. **Show the stages** — opening `stages/stage2_volcano.cpp` and explaining the
   14-band loop is more convincing than showing only the final file.
5. **Mention the bug fixes** — the `useFillColor` outline bug and the smoke draw
   order show real debugging, which examiners reward.
