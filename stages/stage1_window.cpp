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
