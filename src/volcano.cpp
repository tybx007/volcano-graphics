/* ============================================================================
   VOLCANOX  -  Interactive 2D Volcano Eruption Simulator
   ----------------------------------------------------------------------------
   Computer Graphics Lab Project  |  C++ + Graphics.h (WinBGIm)
   ----------------------------------------------------------------------------
   CONTROLS
       SPACE   start / stop the eruption
       P       pause / resume the whole simulation
       R       reset the simulation
       +  -    increase / decrease eruption intensity (1 .. 10)
       S       save a screenshot (volcanox_shot.bmp)
       ESC     exit

   BUILD (from the project root, MinGW-w64 64-bit):
       g++ src/volcano.cpp -o volcanox.exe -I include -L lib -lbgi ^
           -lgdiplus -lcomdlg32 -lgdi32 -luuid -loleaut32 -lole32 -lwinmm ^
           -static -mwindows -O2

   Optional developer tools:
       volcanox.exe --bench 300            run 300 frames, print timing, exit
       volcanox.exe --bench 300 --shot x.bmp   ...and save the last frame
       --intensity N  force 1..10   --paused / --stopped  (start-up states)

   HOW THE PICTURE IS BUILT
       The sky, stars, moon, distant mountains, ground and the volcano never
       change, so they are drawn once into a memory image ("background cache").
       Every frame starts by blitting that image back, then only the moving
       things are drawn on top: glow, lava rivers, smoke, ash and lava bombs.
       This keeps the animation smooth even on a modest laptop.
   ========================================================================= */

#include <graphics.h>          // WinBGIm graphics library
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <windows.h>           // GetTickCount()

// ===========================================================================
//  1.  WINDOW AND SIMULATION CONSTANTS
// ===========================================================================
const int SCREEN_W = 1200;
const int SCREEN_H = 760;

const int HORIZON_Y     = 560;      // where the distant mountains stand
const int GROUND_Y      = 690;      // top edge of the foreground
const int CRATER_X      = 600;
const int CRATER_Y      = 271;
const int CRATER_RX     = 44;
const int CRATER_RY     = 12;
const int SUMMIT_Y      = 250;

const int MAX_LAVA      = 340;      // flying lava bombs
const int MAX_SMOKE     = 56;       // smoke puffs
const int MAX_ASH       = 80;       // ash flakes
const int MAX_EMBERS    = 18;       // glowing sparks in the smoke
const int MAX_STARS     = 250;
const int FLOW_POINTS   = 16;       // points in one lava river
const int NUM_FLOWS     = 2;

const float PI_F = 3.14159265f;

// ===========================================================================
//  2.  COLOUR PALETTE
//      WinBGIm's COLOR(r,g,b) builds 0x03BBGGRR, so when a colour is taken
//      apart again the order is red, green, blue (see helpers below).
// ===========================================================================
const int SKY_TOP      = COLOR(  4,   6,  15);
const int SKY_MID      = COLOR( 11,  13,  32);
const int SKY_LOW      = COLOR( 28,  19,  45);
const int SKY_HORIZON  = COLOR( 62,  32,  38);

const int MOON_MAIN    = COLOR(238, 236, 214);
const int MOON_SHADE   = COLOR(204, 204, 186);
const int MOON_DOT     = COLOR(190, 190, 172);

const int ROCK_DARK    = COLOR( 28,  24,  29);
const int ROCK_MID     = COLOR( 54,  45,  43);
const int ROCK_LIGHT   = COLOR( 92,  76,  64);
const int ROCK_MOON    = COLOR(120, 112, 112);
const int ROCK_WARM    = COLOR(104,  61,  44);

const int GROUND_DARK  = COLOR( 14,  12,  17);
const int GROUND_MID   = COLOR( 23,  20,  26);
const int GROUND_LIT   = COLOR( 38,  30,  30);

const int PANEL_BG     = COLOR(  9,   9,  15);
const int PANEL_LINE   = COLOR( 74,  48,  38);
const int PANEL_SOFT   = COLOR( 34,  30,  38);
const int TEXT_MAIN    = COLOR(232, 232, 240);
const int TEXT_DIM     = COLOR(138, 144, 160);
const int ACCENT       = COLOR(255, 138,  45);
const int ACCENT_DIM   = COLOR(150,  72,  30);
const int STATUS_OK    = COLOR(255, 156,  62);
const int STATUS_PAUSE = COLOR(255, 209, 104);

// hot colours, from white-yellow to dark red (used for lava bombs)
const int LAVA_COLORS[6] = {
    COLOR(255, 255, 222),
    COLOR(255, 238, 142),
    COLOR(255, 198,  70),
    COLOR(255, 148,  36),
    COLOR(236,  92,  20),
    COLOR(168,  44,  14)
};

// ===========================================================================
//  3.  DATA STRUCTURES
// ===========================================================================
struct LavaBomb {
    float x, y;             // position, pixels
    float vx, vy;           // velocity, pixels per frame
    float life;             // seconds left
    float maxLife;
    float heat;             // 0 = cold, 1 = white hot
    int   size;             // core radius in pixels
    bool  active;
};

struct SmokePuff {
    float x, y;
    float vx, vy;
    float radius;
    float grow;
    float life, maxLife;
    float wobble;           // small random value for a lumpy shape
    bool  active;
};

struct AshFlake {
    float x, y, vx, vy;
    float life, maxLife;
    float size;
    bool  active;
};

struct Ember {
    float x, y, vx, vy;
    float life, maxLife;
    float phase;
    bool  active;
};

struct Meteor {
    float x, y, vx, vy;
    float life, maxLife;
    bool  active;
};

struct LavaFlow {
    int   n;
    int   x[FLOW_POINTS];
    int   y[FLOW_POINTS];
    float phase;
};

struct Star {
    int x, y;
    int brightness;
};

// ===========================================================================
//  4.  GLOBAL STATE
// ===========================================================================
LavaBomb  bombs[MAX_LAVA];
SmokePuff smoke[MAX_SMOKE];
AshFlake  ash[MAX_ASH];
Ember     embers[MAX_EMBERS];
Meteor    meteor;
LavaFlow  flows[NUM_FLOWS];
Star      stars[MAX_STARS];

int   skyPalette[SCREEN_H];     // pre-computed sky colour for every scan line
void *backgroundImage = NULL;   // the cached static scene

bool  erupting      = true;
bool  paused        = false;
bool  running       = true;
int   intensity     = 5;         // 1 .. 10
float simTime       = 0.0f;
float glowLevel     = 0.65f;     // smooth 0..1 light level
float spawnAccum    = 0.0f;
float smokeAccum    = 0.0f;
float ashAccum      = 0.0f;
float nextBurst     = 2.5f;
float wind          = -0.25f;
float fps           = 60.0f;

char  screenshotMessage[80] = "";
unsigned long screenshotMessageUntil = 0;

// Draw-state caches.  WinBGIm re-creates a Windows pen/brush for every
// setcolor()/setfillstyle() call, so we only change them when really needed.
int   cachedDrawColor = -999;
int   cachedFillColor = -999;
int   cachedFontSize  = -1;
int   cachedTextBg    = -999;
int   cachedThickness = -1;

// ===========================================================================
//  5.  SMALL HELPERS
// ===========================================================================
int makeColor(int r, int g, int b) { return COLOR(r, g, b); }
int redOf(int c)   { return  c        & 0xFF; }
int greenOf(int c) { return (c >> 8)  & 0xFF; }
int blueOf(int c)  { return (c >> 16) & 0xFF; }

int mixColor(int c1, int c2, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int r = (int)(redOf(c1)   + (redOf(c2)   - redOf(c1))   * t);
    int g = (int)(greenOf(c1) + (greenOf(c2) - greenOf(c1)) * t);
    int b = (int)(blueOf(c1)  + (blueOf(c2)  - blueOf(c1))  * t);
    return makeColor(r, g, b);
}

float clampFloat(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float randomFloat(float lo, float hi)
{
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

int randomInt(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + rand() % (hi - lo + 1);
}

// --- state-cached wrappers -------------------------------------------------
void useDrawColor(int c)
{
    if (c != cachedDrawColor) { setcolor(c); cachedDrawColor = c; }
}

void useFillColor(int c)
{
    if (c != cachedFillColor) { setfillstyle(SOLID_FILL, c); cachedFillColor = c; }
    // WinBGIm also strokes the border of filled shapes with the DRAW colour.
    // Keep both in sync so smoke puffs, sectors and polygons have no stray
    // grey outline left over from whatever was drawn on the previous frame.
    if (c != cachedDrawColor) { setcolor(c); cachedDrawColor = c; }
}

void useThickness(int t)
{
    if (t != cachedThickness) {
        setlinestyle(SOLID_LINE, 0, t);
        cachedThickness = t;
    }
}

void useFont(int size)
{
    if (size != cachedFontSize) {
        settextstyle(SANS_SERIF_FONT, HORIZ_DIR, size);
        cachedFontSize = size;
    }
}

void useTextBackground(int c)
{
    if (c != cachedTextBg) { setbkcolor(c); cachedTextBg = c; }
}

// ===========================================================================
//  6.  SKY PALETTE
// ===========================================================================
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

// Colour of whatever is behind a point - used to fake transparency of smoke.
int backgroundColorAt(int x, int y)
{
    if (x < 0) x = 0;
    if (x >= SCREEN_W) x = SCREEN_W - 1;
    if (y < 0) y = 0;
    if (y >= SCREEN_H) y = SCREEN_H - 1;
    return skyColorAt(y);
}

// ===========================================================================
//  7.  VOLCANO GEOMETRY
//      The two flanks are mathematical curves, so the same function can be
//      used to draw the mountain and to let particles collide with it.
// ===========================================================================
float leftEdge(float y)
{
    float u = (y - 266.0f) / (float)(GROUND_Y - 266);
    u = clampFloat(u, 0.0f, 1.0f);
    // 0.82 < 1 gives the classic concave stratovolcano silhouette
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

// Approximate height of the solid surface at x (for particle collisions).
int surfaceYAt(float x)
{
    if (x < 100.0f || x > 1110.0f) return GROUND_Y - 2;

    if (x >= 552.0f && x <= 648.0f)
        return CRATER_Y + CRATER_RY;          // falls back into the crater

    if (x < 552.0f) {
        float u = powf(clampFloat((546.0f - x) / 426.0f, 0.0f, 1.0f), 1.0f / 0.82f);
        int y = 266 + (int)(u * (GROUND_Y - 266));
        return (y > GROUND_Y - 2) ? GROUND_Y - 2 : y;
    } else {
        float u = powf(clampFloat((x - 654.0f) / 436.0f, 0.0f, 1.0f), 1.0f / 0.80f);
        int y = 266 + (int)(u * (GROUND_Y - 266));
        return (y > GROUND_Y - 2) ? GROUND_Y - 2 : y;
    }
}

// ===========================================================================
//  8.  STATIC SCENE  (drawn once, then cached)
// ===========================================================================
void drawSky()
{
    for (int y = 0; y < SCREEN_H; y++) {
        useDrawColor(skyPalette[y]);
        line(0, y, SCREEN_W - 1, y);
    }

    // A thin, slightly lighter haze just above the horizon.
    for (int y = 480; y < 560; y += 4) {
        int c = mixColor(skyPalette[y], makeColor(96, 52, 58), 0.10f);
        useDrawColor(c);
        line(0, y, SCREEN_W - 1, y);
    }
}

void drawStars()
{
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = randomInt(0, SCREEN_W - 1);
        stars[i].y = randomInt(0, 470);
        stars[i].brightness = randomInt(70, 255);

        int b = stars[i].brightness;
        float haze = 1.0f - (float)stars[i].y / 620.0f;
        if (haze < 0.35f) haze = 0.35f;
        int c = makeColor((int)(b * haze),
                          (int)(b * haze),
                          (int)((b * 0.88f + 22) * haze));

        putpixel(stars[i].x, stars[i].y, c);
        if (b > 225) {                          // brightest stars get a cross
            int faint = mixColor(c, skyColorAt(stars[i].y), 0.55f);
            putpixel(stars[i].x - 1, stars[i].y, faint);
            putpixel(stars[i].x + 1, stars[i].y, faint);
            putpixel(stars[i].x, stars[i].y - 1, faint);
            putpixel(stars[i].x, stars[i].y + 1, faint);
        }
    }
}

void drawMoon(int cx, int cy, int r)
{
    // soft halo - wide, faint rings first
    for (int i = 10; i >= 1; i--) {
        int rr = r + i * 5;
        int c = mixColor(skyColorAt(cy), makeColor(186, 196, 224),
                         0.025f + 0.012f * (11 - i));
        useDrawColor(c);
        circle(cx, cy, rr);
    }

    useFillColor(MOON_MAIN);
    useDrawColor(makeColor(206, 205, 190));
    fillellipse(cx, cy, r, r);

    // craters
    useFillColor(MOON_DOT);
    fillellipse(cx - 13, cy - 10, 12, 9);
    fillellipse(cx + 14, cy + 7, 8, 6);
    fillellipse(cx + 1, cy + 20, 6, 4);
    fillellipse(cx - 20, cy + 13, 4, 3);

    useFillColor(MOON_SHADE);
    fillellipse(cx - 12, cy - 11, 6, 4);
    fillellipse(cx + 15, cy + 8, 4, 3);

    // gentle right-hand shadow to make it look spherical
    useDrawColor(makeColor(215, 214, 198));
    for (int i = 0; i < 12; i++) {
        int yy = cy - r + 6 + i * (2 * r - 12) / 11;
        int dx = (int)(r * 0.55);
        line(cx + dx, yy, cx + r - 2, yy);
    }
}

// One jagged mountain range drawn as a filled polygon.
void drawMountainRange(int baseY, int minHeight, int maxHeight, int color)
{
    int pts[400];
    int n = 0;
    pts[n++] = -20; pts[n++] = baseY;

    int x = -20;
    while (x < SCREEN_W + 20) {
        x += randomInt(24, 58);
        int valley = baseY - randomInt(0, 14);
        pts[n++] = x; pts[n++] = valley;

        x += randomInt(26, 64);
        int peak = baseY - minHeight - randomInt(0, maxHeight - minHeight);
        pts[n++] = x; pts[n++] = peak;
    }

    pts[n++] = SCREEN_W + 20; pts[n++] = baseY;
    pts[n++] = SCREEN_W + 20; pts[n++] = SCREEN_H;
    pts[n++] = -20;           pts[n++] = SCREEN_H;

    useFillColor(color);
    fillpoly(n / 2, pts);
}

void drawDistantRanges()
{
    drawMountainRange(556,  58, 120, COLOR(26, 25, 43));
    drawMountainRange(575,  32,  78, COLOR(16, 16, 29));
}

void drawGround()
{
    // mid-ground plain
    for (int y = HORIZON_Y; y < SCREEN_H; y++) {
        float t = (float)(y - HORIZON_Y) / 200.0f;
        if (t > 1.0f) t = 1.0f;
        useDrawColor(mixColor(GROUND_MID, GROUND_DARK, t * t));
        line(0, y, SCREEN_W - 1, y);
    }

    // faint warm strip at the foot of the volcano
    for (int y = 600; y < 660; y += 3) {
        float t = (float)(y - 600) / 60.0f;
        int c = mixColor(GROUND_MID, GROUND_LIT, 0.35f * (1.0f - t));
        useDrawColor(c);
        line(0, y, SCREEN_W - 1, y);
    }

    // cooling cracks on the plain
    for (int i = 0; i < 26; i++) {
        int x = randomInt(0, SCREEN_W);
        int y = randomInt(600, 688);
        int len = randomInt(12, 40);
        int c = mixColor(GROUND_DARK, makeColor(96, 32, 20), randomFloat(0.15f, 0.45f));
        useDrawColor(c);
        int px = x, py = y;
        for (int s = 0; s < 3; s++) {
            int nx = px + randomInt(-len / 3, len / 3);
            int ny = py + randomInt(1, 5);
            line(px, py, nx, ny);
            px = nx; py = ny;
        }
    }

    // scattered stones
    for (int i = 0; i < 55; i++) {
        int x = randomInt(10, SCREEN_W - 20);
        int y = randomInt(600, 740);
        int w = randomInt(2, 7);
        useFillColor(mixColor(GROUND_DARK, makeColor(52, 44, 40), randomFloat(0.1f, 0.4f)));
        fillellipse(x, y, w, w / 2 + 1);
    }
}

// Soft static glow behind the summit (the mountain is drawn over it).
void drawSummitGlow()
{
    int ref = skyColorAt(270);
    for (int i = 24; i >= 1; i--) {
        int rx = 50 + i * 8;
        int ry = 16 + i * 4;
        float a = 0.050f * (1.0f - (float)i / 26.0f);
        useFillColor(mixColor(ref, makeColor(255, 126, 46), a));
        fillellipse(CRATER_X, CRATER_Y, rx, ry);
    }
}

void drawVolcano()
{
    drawSummitGlow();

    // --- small cap above the crater -------------------------------------
    int cap[10] = { 546, 266, 566, 255, 600, 250, 634, 255, 654, 266 };
    useFillColor(ROCK_MID);
    fillpoly(5, cap);

    // --- cone body: scan lines shaded from left (shadow) to right (moon) --
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
            float light = 0.10f + 0.92f * powf(t, 1.55f);

            // large-scale colour variation + fine rock noise
            light += 0.045f * sinf(x1 * 0.017f + y * 0.051f)
                   + 0.022f * sinf(x1 * 0.083f - y * 0.019f);
            light *= 0.74f + 0.26f * (1.0f - depth);

            if (b == 0) light -= 0.10f;                 // dark left silhouette
            if (b == BANDS - 1) light += 0.12f;         // moonlit right edge

            int c = mixColor(ROCK_DARK, ROCK_LIGHT, clampFloat(light, 0.0f, 1.0f));
            if (b >= BANDS - 2) c = mixColor(c, ROCK_MOON, 0.18f);

            useDrawColor(c);
            line(x1, y, x2, y);
        }
    }

    // --- dark left rim and moonlit right rim ------------------------------
    for (int y = 266; y <= GROUND_Y; y += 2) {
        int xL = (int)leftEdge((float)y);
        useDrawColor(mixColor(ROCK_DARK, makeColor(8, 8, 12), 0.45f));
        line(xL, y, xL + 2, y + 2);
    }
    for (int y = 266; y <= GROUND_Y; y += 2) {
        int xR = (int)rightEdge((float)y);
        float t = (float)(y - 266) / (float)(GROUND_Y - 266);
        int c = mixColor(ROCK_MOON, ROCK_LIGHT, 0.35f + 0.4f * t);
        useDrawColor(c);
        line(xR - 2, y, xR, y + 2);
    }

    // --- erosion gullies that follow the slope ----------------------------
    for (int k = 0; k < 20; k++) {
        float g = -0.92f + 1.84f * (k / 19.0f);
        int prevX = 0, prevY = 0;
        for (int y = 282; y <= GROUND_Y - 6; y += 9) {
            float u = (float)(y - 266) / (float)(GROUND_Y - 266);
            float f = 0.5f + g * 0.5f * u;
            float x = leftEdge((float)y) + (rightEdge((float)y) - leftEdge((float)y)) * f;
            x += 3.0f * sinf(y * 0.09f + k * 2.1f);
            if (prevX != 0) {
                int c;
                if (g < 0) c = mixColor(ROCK_DARK, makeColor(10, 9, 13), 0.35f);
                else       c = mixColor(ROCK_LIGHT, ROCK_WARM, 0.25f);
                useDrawColor(c);
                line(prevX, prevY, (int)x, y);
            }
            prevX = (int)x; prevY = y;
        }
    }

    // --- horizontal strata ------------------------------------------------
    for (int k = 0; k < 16; k++) {
        int y0 = 310 + k * 24 + randomInt(-4, 4);
        int prevX = 0, prevY = y0;
        for (int s = 0; s <= 20; s++) {
            float y = (float)(y0 + s * 2 + sinf(s * 0.7f + k) * 2.0f);
            if (y > GROUND_Y - 4) break;
            float xL = leftEdge(y), xR = rightEdge(y);
            float x = xL + (xR - xL) * (s / 20.0f);
            if (prevX != 0) {
                int c = (k % 3 == 0)
                        ? mixColor(ROCK_DARK, makeColor(16, 13, 16), 0.4f)
                        : mixColor(ROCK_MID, ROCK_LIGHT, 0.28f);
                useDrawColor(c);
                line(prevX, prevY, (int)x, (int)y);
            }
            prevX = (int)x; prevY = (int)y;
        }
    }

    // --- reddish oxidised patches ----------------------------------------
    for (int k = 0; k < 7; k++) {
        int cy = 380 + k * 42 + randomInt(-10, 10);
        float u = (float)(cy - 266) / (float)(GROUND_Y - 266);
        int cx = (int)(leftEdge((float)cy) + (rightEdge((float)cy) - leftEdge((float)cy))
                       * randomFloat(0.25f, 0.85f));
        int w = randomInt(26, 70);
        int h = randomInt(10, 24);
        int pts[12];
        for (int p = 0; p < 6; p++) {
            pts[p * 2]     = cx + (int)(cosf(p * PI_F / 3.0f) * w * randomFloat(0.6f, 1.0f));
            pts[p * 2 + 1] = cy + (int)(sinf(p * PI_F / 3.0f) * h * randomFloat(0.6f, 1.0f));
        }
        useFillColor(mixColor(ROCK_MID, makeColor(92, 42, 30), 0.45f + 0.25f * u));
        fillpoly(6, pts);
    }

    // --- boulders near the foot ------------------------------------------
    for (int k = 0; k < 26; k++) {
        int y = randomInt(600, 686);
        float xL = leftEdge((float)y), xR = rightEdge((float)y);
        int x = (int)(xL + (xR - xL) * randomFloat(0.08f, 0.92f));
        int w = randomInt(3, 9);
        int h = randomInt(2, 6);
        int pts[8] = { x - w, y + h, x - w / 2, y - h, x + w / 2, y - h, x + w, y + h };
        useFillColor(mixColor(ROCK_DARK, makeColor(12, 11, 15), 0.3f));
        fillpoly(4, pts);
        useDrawColor(mixColor(ROCK_LIGHT, ROCK_MOON, 0.25f));
        line(x - w / 2, y - h, x + w / 2, y - h);
    }
}

void drawForeground()
{
    // A nearer strip of ground that hides the very bottom of the cone.
    // One point every 60 px plus the two bottom corners: keep room for all.
    int pts[64];
    int n = 0;
    pts[n++] = -10; pts[n++] = GROUND_Y + 4;
    for (int x = -10; x <= SCREEN_W + 10; x += 60) {
        pts[n++] = x;
        pts[n++] = GROUND_Y + randomInt(-3, 7);
    }
    pts[n++] = SCREEN_W + 10; pts[n++] = SCREEN_H;
    pts[n++] = -10;           pts[n++] = SCREEN_H;

    useFillColor(COLOR(12, 11, 16));
    fillpoly(n / 2, pts);

    // rocks, lit from the right by the moon
    for (int i = 0; i < 30; i++) {
        int x = randomInt(0, SCREEN_W);
        int y = randomInt(GROUND_Y + 8, SCREEN_H - 8);
        int w = randomInt(6, 26);
        int h = randomInt(4, 14);
        int rock[10] = { x - w, y + h, x - w / 2, y - h,
                         x + w / 2, y - h / 2, x + w, y + h,
                         x, y + h + 3 };
        useFillColor(COLOR(15, 14, 19));
        fillpoly(5, rock);
        useDrawColor(COLOR(48, 43, 48));
        line(x - w / 2, y - h, x + w / 2, y - h / 2);
    }
}

// ===========================================================================
//  9.  BUILD / RESTORE THE BACKGROUND CACHE
// ===========================================================================
void buildBackground()
{
    buildSkyPalette();

    // deterministic scenery every time the program starts
    srand(20251113);

    drawSky();
    drawStars();
    drawMoon(1030, 210, 42);
    drawDistantRanges();
    drawGround();
    drawVolcano();
    drawForeground();

    // Copy the finished scene into a memory bitmap.
    unsigned size = imagesize(0, 0, SCREEN_W - 1, SCREEN_H - 1);
    backgroundImage = malloc(size);
    if (backgroundImage == NULL) {
        closegraph();
        printf("Not enough memory for the background image.\n");
        exit(1);
    }
    getimage(0, 0, SCREEN_W - 1, SCREEN_H - 1, backgroundImage);
}

void restoreBackground()
{
    putimage(0, 0, backgroundImage, COPY_PUT);
}

// ===========================================================================
// 10.  LAVA RIVERS
// ===========================================================================
void buildFlows()
{
    for (int f = 0; f < NUM_FLOWS; f++) {
        flows[f].n = FLOW_POINTS;
        flows[f].phase = (f == 0) ? 0.0f : 0.47f;

        for (int i = 0; i < FLOW_POINTS; i++) {
            int y = 279 + i * 27;
            float u = (float)(y - 266) / (float)(GROUND_Y - 266);
            float pull = (u > 0.62f) ? (u - 0.62f) / 0.38f : 0.0f;
            pull *= pull;

            if (f == 0)
                flows[f].x[i] = (int)(leftEdge((float)y) + 30.0f + 62.0f * pull)
                                + (int)(4.0f * sinf(i * 1.7f));
            else
                flows[f].x[i] = (int)(rightEdge((float)y) - 30.0f - 112.0f * pull)
                                + (int)(4.0f * sinf(i * 1.4f + 2.0f));
            flows[f].y[i] = y;
        }
    }
}

void drawLavaFlows()
{
    for (int f = 0; f < NUM_FLOWS; f++) {
        LavaFlow &fl = flows[f];

        // Three thick passes give the river a crust, a body and a hot core.
        int colours[3];
        colours[0] = mixColor(COLOR(58, 16, 11), COLOR(120, 34, 12), 0.35f + 0.25f * glowLevel);
        colours[1] = mixColor(COLOR(146, 42, 13), COLOR(255, 126, 32), 0.30f + 0.45f * glowLevel);
        colours[2] = mixColor(COLOR(228, 108, 30), COLOR(255, 225, 130), 0.25f + 0.55f * glowLevel);
        int widths[3] = { 9, 6, 3 };

        for (int pass = 0; pass < 3; pass++) {
            useDrawColor(colours[pass]);
            useThickness(widths[pass]);
            for (int i = 0; i < fl.n - 1; i++)
                line(fl.x[i], fl.y[i], fl.x[i + 1], fl.y[i + 1]);
        }
        useThickness(1);

        // Bright blobs sliding down the river.
        useFillColor(mixColor(COLOR(255, 190, 80), COLOR(255, 246, 190), 0.4f + 0.4f * glowLevel));
        for (int k = 0; k < 11; k++) {
            float p = fmodf(simTime * 0.38f + k / 11.0f + fl.phase, 1.0f);
            float fp = p * (fl.n - 1);
            int i = (int)fp;
            if (i > fl.n - 2) i = fl.n - 2;
            float t = fp - i;
            int x = (int)(fl.x[i] + (fl.x[i + 1] - fl.x[i]) * t);
            int y = (int)(fl.y[i] + (fl.y[i + 1] - fl.y[i]) * t);
            int r = 2 + (int)(1.5f * (0.5f + 0.5f * sinf(simTime * 6.0f + k)));
            fillellipse(x, y, r, r);
        }

        // Source at the crater rim
        useFillColor(mixColor(COLOR(255, 170, 60), COLOR(255, 240, 170), 0.55f));
        fillellipse(fl.x[0], fl.y[0] - 2, 7, 4);
    }
}

// ===========================================================================
// 11.  CRATER AND GLOW
// ===========================================================================
void drawCrater()
{
    float pulse = 0.55f + 0.45f * sinf(simTime * 4.0f) * glowLevel;
    int rimY = CRATER_Y;

    // Rays of light shooting into the sky (behind the crater rim).
    for (int i = 0; i < 7; i++) {
        float len = (90.0f + 55.0f * sinf(simTime * 1.7f + i * 1.3f)) * (0.5f + 0.7f * glowLevel);
        int tipX = CRATER_X + (int)((i - 3) * 38 + 16 * sinf(simTime * 0.9f + i));
        int tipY = (int)(CRATER_Y - 22 - len);
        int baseX = CRATER_X + (i - 3) * 9;
        int ray[6] = { baseX - 14, CRATER_Y - 6, baseX + 14, CRATER_Y - 6, tipX, tipY };
        int c = mixColor(skyColorAt(200), makeColor(255, 138, 48),
                         0.03f + 0.055f * glowLevel);
        useFillColor(c);
        fillpoly(3, ray);
    }

    // Pulsing dome of light above the summit.
    int ref = skyColorAt(205);
    for (int i = 9; i >= 1; i--) {
        int rx = 26 + i * 11;
        int ry = 7 + i * 4;
        float a = (0.035f + 0.030f * i) * glowLevel;
        useFillColor(mixColor(ref, makeColor(255, 126, 44), a));
        fillellipse(CRATER_X, 202, rx, ry);
    }

    // Dark crater opening.
    useFillColor(COLOR(34, 14, 14));
    fillellipse(CRATER_X, rimY, CRATER_RX, CRATER_RY);

    // Far inner wall: concentric upper-half sectors from dark to warm.
    for (int i = 9; i >= 2; i--) {
        int rx = CRATER_RX * i / 10;
        int ry = CRATER_RY * i / 10;
        int c = mixColor(COLOR(46, 17, 15), COLOR(150, 52, 22), (float)(10 - i) / 9.0f);
        useFillColor(c);
        sector(CRATER_X, rimY, 0, 180, rx, ry);
    }

    // Glowing lava lake: a full ellipse so the pool fills the crater floor;
    // the dark ring around it is the inner wall in shadow.  The lake cools
    // down when the eruption is stopped (glowLevel fades).
    float lakeHeat = clampFloat((glowLevel - 0.15f) / 0.65f, 0.0f, 1.0f);
    int lakeOuter = mixColor(mixColor(COLOR(70, 18, 10), COLOR(220, 74, 18), 0.4f),
                             COLOR(255, 150, 44), pulse * lakeHeat);
    useFillColor(lakeOuter);
    fillellipse(CRATER_X, rimY - 1, 34, 8);
    useFillColor(mixColor(COLOR(140, 44, 14),
                          COLOR(255, 246, 190), pulse * lakeHeat));
    fillellipse(CRATER_X, rimY - 1, 24, 5);
    if (lakeHeat > 0.25f) {
        useFillColor(mixColor(COLOR(255, 190, 80), COLOR(255, 252, 215), lakeHeat));
        fillellipse(CRATER_X - 4, rimY - 2, 7, 2);
    }

    // Boiling highlights in the lake.
    useDrawColor(mixColor(COLOR(255, 160, 60), COLOR(255, 235, 170), pulse));
    ellipse(CRATER_X, rimY - 1, 0, 180, 15, 4);
    ellipse(CRATER_X - 10, rimY - 2, 0, 180, 7, 2);

    // Glowing rim arcs.
    useThickness(3);
    useDrawColor(mixColor(COLOR(150, 54, 20), COLOR(255, 170, 70), pulse));
    ellipse(CRATER_X, rimY, 0, 180, CRATER_RX + 1, CRATER_RY + 1);
    useThickness(1);
    useDrawColor(mixColor(COLOR(255, 140, 44), COLOR(255, 226, 140), pulse));
    ellipse(CRATER_X, rimY, 0, 180, CRATER_RX + 2, CRATER_RY + 2);
    ellipse(CRATER_X, rimY + 1, 0, 180, CRATER_RX + 5, CRATER_RY + 5);

    // Front lip: catches the light from the lake.
    useDrawColor(mixColor(COLOR(120, 44, 22), COLOR(255, 128, 46), glowLevel * 0.8f));
    ellipse(CRATER_X, rimY + 1, 180, 360, CRATER_RX, CRATER_RY);
}

// ===========================================================================
// 12.  LAVA BOMBS
// ===========================================================================
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

void updateLavaBombs(float dt)
{
    for (int i = 0; i < MAX_LAVA; i++) {
        if (!bombs[i].active) continue;

        bombs[i].x += bombs[i].vx;
        bombs[i].y += bombs[i].vy;
        bombs[i].vy += 0.145f;
        bombs[i].vx *= 0.995f;
        bombs[i].life -= dt;

        // fade the heat as the bomb cools
        float age = 1.0f - bombs[i].life / bombs[i].maxLife;
        bombs[i].heat = clampFloat(1.0f - age * 0.9f, 0.05f, 1.0f);

        if (bombs[i].life <= 0 ||
            bombs[i].y > surfaceYAt(bombs[i].x) + 4 ||
            bombs[i].x < -30 || bombs[i].x > SCREEN_W + 30)
            bombs[i].active = false;
    }
}

void drawLavaBombs()
{
    // Draw in groups of the same colour to avoid hundreds of brush changes.
    for (int c = 0; c < 6; c++) {
        useFillColor(LAVA_COLORS[c]);
        useDrawColor(LAVA_COLORS[c]);

        for (int i = 0; i < MAX_LAVA; i++) {
            if (!bombs[i].active) continue;

            float age = 1.0f - bombs[i].life / bombs[i].maxLife;
            int idx = (int)(age * 5.0f + (1.0f - bombs[i].heat) * 1.5f);
            if (idx < 0) idx = 0;
            if (idx > 5) idx = 5;
            if (idx != c) continue;

            int x = (int)bombs[i].x;
            int y = (int)bombs[i].y;

            // short motion trail
            line(x, y, (int)(x - bombs[i].vx * 2.2f),
                      (int)(y - bombs[i].vy * 2.2f));

            // glowing core
            if (bombs[i].size <= 1) {
                fillellipse(x, y, 1, 1);
                putpixel(x, y, mixColor(LAVA_COLORS[c], LAVA_COLORS[0], 0.5f));
            } else {
                fillellipse(x, y, 2, 2);
            }
        }
    }
}

// ===========================================================================
// 13.  SMOKE, ASH AND EMBERS
// ===========================================================================
void spawnSmoke()
{
    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smoke[i].active) {
            smoke[i].active  = true;
            smoke[i].x       = CRATER_X + randomFloat(-26.0f, 26.0f);
            smoke[i].y       = CRATER_Y - 8 + randomFloat(-4.0f, 6.0f);
            smoke[i].vx      = wind + randomFloat(-0.22f, 0.22f);
            smoke[i].vy      = -randomFloat(0.55f, 1.25f) * (0.8f + intensity * 0.05f);
            smoke[i].radius  = randomFloat(7.0f, 13.0f);
            smoke[i].grow    = randomFloat(0.26f, 0.52f);
            smoke[i].life    = randomFloat(4.5f, 8.0f);
            smoke[i].maxLife = smoke[i].life;
            smoke[i].wobble  = randomFloat(0.0f, 1.0f);
            return;
        }
    }
}

void updateSmoke(float dt)
{
    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smoke[i].active) continue;

        smoke[i].x += smoke[i].vx * dt * 60.0f;
        smoke[i].y += smoke[i].vy * dt * 60.0f;
        smoke[i].vy -= 0.004f * dt * 60.0f;        // buoyancy
        smoke[i].vx += 0.0015f * sinf(simTime * 0.7f + i) * dt * 60.0f;
        smoke[i].x += 0.18f * sinf(simTime * 1.1f + i * 2.3f);
        smoke[i].radius += smoke[i].grow * dt * 60.0f;
        smoke[i].life -= dt;

        if (smoke[i].life <= 0 || smoke[i].y < -80 || smoke[i].x < -80)
            smoke[i].active = false;
    }
}

void drawSmoke()
{
    int skyRef = mixColor(skyColorAt(250), ROCK_MID, 0.22f);
    int warm   = COLOR(120, 78, 62);
    int middle = COLOR(78, 74, 84);
    int cool   = COLOR(48, 48, 58);

    int bases[3] = { warm, middle, cool };

    // layer 0 = big soft outer puff, layer 2 = dense inner puff
    for (int layer = 0; layer < 3; layer++) {
        for (int band = 0; band < 3; band++) {
            useFillColor(mixColor(skyRef, bases[band],
                                  0.10f + layer * 0.13f));

            for (int i = 0; i < MAX_SMOKE; i++) {
                if (!smoke[i].active) continue;

                float age = 1.0f - smoke[i].life / smoke[i].maxLife;
                int b = (age < 0.30f) ? 0 : (age < 0.65f) ? 1 : 2;
                if (b != band) continue;

                float scale = (layer == 0) ? 1.0f : (layer == 1) ? 0.72f : 0.42f;
                int r = (int)(smoke[i].radius * scale);
                if (r < 2) r = 2;

                int ox = (int)((smoke[i].wobble - 0.5f) * smoke[i].radius * 0.55f * (1.0f - layer * 0.3f));
                int oy = (int)((0.5f - smoke[i].wobble) * smoke[i].radius * 0.30f);

                fillellipse((int)smoke[i].x + ox, (int)smoke[i].y + oy, r, (int)(r * 0.86f));
            }
        }
    }
}

void spawnAsh()
{
    for (int i = 0; i < MAX_ASH; i++) {
        if (!ash[i].active) {
            ash[i].active  = true;
            ash[i].x       = CRATER_X + randomFloat(-30.0f, 30.0f);
            ash[i].y       = CRATER_Y - randomFloat(0.0f, 24.0f);
            ash[i].vx      = wind * 1.5f + randomFloat(-0.55f, 0.65f);
            ash[i].vy      = -randomFloat(1.2f, 3.0f);
            ash[i].life    = randomFloat(2.5f, 5.5f);
            ash[i].maxLife = ash[i].life;
            ash[i].size    = randomFloat(0.5f, 1.4f);
            return;
        }
    }
}

void updateAsh(float dt)
{
    for (int i = 0; i < MAX_ASH; i++) {
        if (!ash[i].active) continue;
        ash[i].x += ash[i].vx * dt * 60.0f;
        ash[i].y += ash[i].vy * dt * 60.0f;
        ash[i].vx += 0.006f * sinf(simTime * 1.6f + i);
        ash[i].life -= dt;
        if (ash[i].life <= 0 || ash[i].y < -40) ash[i].active = false;
    }
}

void drawAsh()
{
    useDrawColor(COLOR(62, 60, 70));
    for (int i = 0; i < MAX_ASH; i++) {
        if (!ash[i].active) continue;
        if (i % 3 == 0) {
            putpixel((int)ash[i].x, (int)ash[i].y, COLOR(44, 43, 52));
        } else {
            putpixel((int)ash[i].x, (int)ash[i].y, COLOR(66, 64, 74));
            if (ash[i].size > 1.0f)
                putpixel((int)ash[i].x + 1, (int)ash[i].y, COLOR(40, 40, 48));
        }
    }
}

void spawnEmber()
{
    for (int i = 0; i < MAX_EMBERS; i++) {
        if (!embers[i].active) {
            embers[i].active  = true;
            embers[i].x       = CRATER_X + randomFloat(-30.0f, 30.0f);
            embers[i].y       = CRATER_Y - randomFloat(4.0f, 30.0f);
            embers[i].vx      = wind + randomFloat(-0.4f, 0.4f);
            embers[i].vy      = -randomFloat(0.8f, 2.2f);
            embers[i].life    = randomFloat(1.2f, 3.0f);
            embers[i].maxLife = embers[i].life;
            embers[i].phase   = randomFloat(0.0f, 2.0f * PI_F);
            return;
        }
    }
}

void updateEmbers(float dt)
{
    for (int i = 0; i < MAX_EMBERS; i++) {
        if (!embers[i].active) continue;
        embers[i].x += embers[i].vx * dt * 60.0f + 0.25f * sinf(simTime * 2.0f + embers[i].phase);
        embers[i].y += embers[i].vy * dt * 60.0f;
        embers[i].life -= dt;
        if (embers[i].life <= 0 || embers[i].y < -20) embers[i].active = false;
    }
}

void drawEmbers()
{
    useDrawColor(COLOR(255, 176, 60));
    for (int i = 0; i < MAX_EMBERS; i++) {
        if (!embers[i].active) continue;
        float flicker = 0.5f + 0.5f * sinf(simTime * 11.0f + embers[i].phase);
        if (flicker > 0.35f)
            putpixel((int)embers[i].x, (int)embers[i].y,
                     (flicker > 0.75f) ? COLOR(255, 226, 130) : COLOR(244, 130, 40));
    }
}

// ===========================================================================
// 14.  METEOR
// ===========================================================================
void updateMeteor(float dt)
{
    if (meteor.active) {
        meteor.x += meteor.vx;
        meteor.y += meteor.vy;
        meteor.life -= dt;
        if (meteor.life <= 0 || meteor.y > 470) meteor.active = false;
    } else if (rand() % 900 == 0) {
        meteor.active  = true;
        meteor.x       = randomFloat(200.0f, 1150.0f);
        meteor.y       = randomFloat(40.0f, 190.0f);
        meteor.vx      = -randomFloat(5.0f, 8.0f);
        meteor.vy      = randomFloat(2.5f, 4.0f);
        meteor.life    = randomFloat(0.7f, 1.2f);
        meteor.maxLife = meteor.life;
    }
}

void drawMeteor()
{
    if (!meteor.active) return;

    for (int i = 0; i < 9; i++) {
        float t = i / 9.0f;
        float x = meteor.x - meteor.vx * t * 3.2f;
        float y = meteor.y - meteor.vy * t * 3.2f;
        int c = mixColor(makeColor(255, 244, 210), skyColorAt((int)y), t * 1.1f);
        useDrawColor(c);
        line((int)x, (int)y, (int)(x - meteor.vx * 0.35f), (int)(y - meteor.vy * 0.35f));
    }
    useDrawColor(COLOR(255, 252, 232));
    line((int)meteor.x, (int)meteor.y, (int)(meteor.x - meteor.vx), (int)(meteor.y - meteor.vy));
}

// ===========================================================================
// 15.  LAVA POOLS ON THE PLAIN
// ===========================================================================
void drawGroundPools()
{
    int poolX[2] = { flows[0].x[flows[0].n - 1], flows[1].x[flows[1].n - 1] };
    int poolY[2] = { GROUND_Y + 15, GROUND_Y + 13 };

    for (int i = 0; i < 2; i++) {
        // warm light spilled on the ground
        for (int k = 6; k >= 1; k--) {
            int c = mixColor(COLOR(20, 16, 18), COLOR(255, 104, 30),
                             0.03f + 0.035f * k * glowLevel);
            useFillColor(c);
            fillellipse(poolX[i], poolY[i] + 2, 22 + k * 11, 8 + k * 4);
        }

        // the pool itself
        int c1 = mixColor(COLOR(120, 30, 10), COLOR(255, 120, 26), 0.5f + 0.3f * glowLevel);
        useFillColor(c1);
        fillellipse(poolX[i], poolY[i], 34, 11);
        useFillColor(mixColor(COLOR(240, 110, 30), COLOR(255, 216, 120), 0.5f));
        fillellipse(poolX[i] - 3, poolY[i] - 1, 22, 7);
        useFillColor(mixColor(COLOR(255, 200, 100), COLOR(255, 250, 205), 0.5f));
        fillellipse(poolX[i] - 5, poolY[i] - 2, 11, 3);

        // a few floating bright spots
        for (int s = 0; s < 4; s++) {
            int ox = (int)(sinf(simTime * 2.0f + s * 1.7f + i) * 16);
            putpixel(poolX[i] + ox, poolY[i] - 1, COLOR(255, 240, 170));
        }
    }
}

// ===========================================================================
// 16.  HUD  (all text is redrawn each frame so it always sits on top)
// ===========================================================================
void drawBadge(int x, int y, int w, const char *label)
{
    useFillColor(COLOR(26, 24, 32));
    bar(x, y, x + w, y + 22);
    useDrawColor(COLOR(88, 68, 56));
    rectangle(x, y, x + w, y + 22);

    useFont(1);
    useDrawColor(TEXT_MAIN);
    useTextBackground(COLOR(26, 24, 32));
    int tw = textwidth((char *)label);
    outtextxy(x + (w - tw) / 2, y + 2, (char *)label);
}

void drawHud()
{
    // ---------------------------------------------------------------- title
    useFillColor(PANEL_BG);
    bar(24, 20, 396, 182);
    useDrawColor(PANEL_LINE);
    rectangle(24, 20, 396, 182);
    useDrawColor(COLOR(46, 38, 44));
    rectangle(27, 23, 393, 179);

    // orange accent on the left edge
    for (int i = 0; i < 6; i++) {
        int c = mixColor(ACCENT, PANEL_BG, i / 6.0f);
        useDrawColor(c);
        line(24 + i, 26, 24 + i, 176);
    }

    useTextBackground(PANEL_BG);
    useFont(5);
    useDrawColor(ACCENT);
    outtextxy(44, 26, (char *)"VOLCANOX");
    useDrawColor(COLOR(255, 186, 92));          // fake bold: draw again
    outtextxy(45, 26, (char *)"VOLCANOX");

    useFont(1);
    useDrawColor(TEXT_DIM);
    outtextxy(46, 74, (char *)"ERUPTION SIMULATOR");

    useDrawColor(PANEL_SOFT);
    line(44, 99, 376, 99);

    // status row
    useDrawColor(TEXT_DIM);
    outtextxy(46, 108, (char *)"STATUS");

    int statusColor = paused ? STATUS_PAUSE
                             : (erupting ? STATUS_OK : COLOR(130, 138, 154));
    useFillColor(statusColor);
    fillellipse(136, 118, 5, 5);
    useFillColor(mixColor(statusColor, PANEL_BG, 0.55f));
    fillellipse(136, 118, 8, 8);
    useFillColor(statusColor);
    fillellipse(136, 118, 4, 4);

    useDrawColor(statusColor);
    if (paused)        outtextxy(150, 108, (char *)"PAUSED");
    else if (erupting) outtextxy(150, 108, (char *)"ACTIVE");
    else               outtextxy(150, 108, (char *)"STOPPED");

    // intensity row
    useDrawColor(TEXT_DIM);
    outtextxy(46, 138, (char *)"INTENSITY");

    int barX = 146, barY = 143, segW = 13, gap = 3;
    for (int i = 0; i < 10; i++) {
        int c;
        if (i < intensity) c = mixColor(COLOR(255, 120, 30), COLOR(255, 212, 110), i / 9.0f);
        else               c = COLOR(38, 33, 38);
        useFillColor(c);
        bar(barX + i * (segW + gap), barY,
            barX + i * (segW + gap) + segW, barY + 9);
    }

    char buf[40];
    sprintf(buf, "%d/10", intensity);
    useDrawColor(TEXT_MAIN);
    outtextxy(330, 136, buf);

    // -------------------------------------------------------------- statistics
    useFillColor(PANEL_BG);
    bar(884, 20, 1176, 124);
    useDrawColor(PANEL_LINE);
    rectangle(884, 20, 1176, 124);
    useDrawColor(COLOR(46, 38, 44));
    rectangle(887, 23, 1173, 121);

    useTextBackground(PANEL_BG);
    useFont(1);

    int lavaCount = 0, smokeCount = 0;
    for (int i = 0; i < MAX_LAVA; i++)  if (bombs[i].active) lavaCount++;
    for (int i = 0; i < MAX_SMOKE; i++) if (smoke[i].active) smokeCount++;

    int minutes = (int)simTime / 60;
    int seconds = (int)simTime % 60;

    char values[4][20];
    sprintf(values[0], "%4.1f", fps);
    sprintf(values[1], "%d", lavaCount);
    sprintf(values[2], "%d", smokeCount);
    sprintf(values[3], "%02d:%02d", minutes, seconds);

    const char *labels[4] = { "FPS", "LAVA BOMBS", "SMOKE PUFFS", "TIME" };
    for (int i = 0; i < 4; i++) {
        int y = 30 + i * 22;
        useDrawColor(TEXT_DIM);
        outtextxy(900, y, (char *)labels[i]);
        useDrawColor(i == 3 ? ACCENT : TEXT_MAIN);
        int tw = textwidth(values[i]);
        outtextxy(1160 - tw, y, values[i]);
    }

    // -------------------------------------------------------------- control bar
    useFillColor(COLOR(8, 8, 13));
    bar(0, 718, SCREEN_W - 1, SCREEN_H - 1);
    useDrawColor(PANEL_LINE);
    line(0, 718, SCREEN_W - 1, 718);

    useTextBackground(COLOR(8, 8, 13));
    useFont(1);
    useDrawColor(TEXT_DIM);

    drawBadge(30,  726, 70, "SPACE");
    outtextxy(108, 728, (char *)"Start / Stop");

    drawBadge(236, 726, 26, "P");
    outtextxy(270, 728, (char *)"Pause");

    drawBadge(340, 726, 26, "R");
    outtextxy(374, 728, (char *)"Reset");

    drawBadge(444, 726, 56, "+  -");
    outtextxy(508, 728, (char *)"Intensity");

    drawBadge(610, 726, 26, "S");
    outtextxy(644, 728, (char *)"Screenshot");

    drawBadge(764, 726, 48, "ESC");
    outtextxy(820, 728, (char *)"Exit");

    useDrawColor(COLOR(92, 80, 80));
    int tw = textwidth((char *)"VolcanoX  v1.0");
    outtextxy(SCREEN_W - 24 - tw, 728, (char *)"VolcanoX  v1.0");

    // -------------------------------------------------------------- paused banner
    if (paused) {
        useFillColor(COLOR(10, 10, 16));
        bar(430, 470, 770, 578);
        useDrawColor(ACCENT_DIM);
        rectangle(430, 470, 770, 578);
        useDrawColor(ACCENT);
        rectangle(433, 473, 767, 575);

        useTextBackground(COLOR(10, 10, 16));
        useFont(5);
        useDrawColor(ACCENT);
        int w = textwidth((char *)"PAUSED");
        outtextxy(600 - w / 2, 486, (char *)"PAUSED");

        useFont(1);
        useDrawColor(TEXT_DIM);
        w = textwidth((char *)"press P to continue");
        outtextxy(600 - w / 2, 542, (char *)"press P to continue");
    }

    // -------------------------------------------------------------- screenshot toast
    if (screenshotMessage[0] != '\0' && GetTickCount() < screenshotMessageUntil) {
        useFillColor(COLOR(10, 10, 16));
        bar(430, 668, 770, 702);
        useDrawColor(COLOR(80, 60, 48));
        rectangle(430, 668, 770, 702);
        useTextBackground(COLOR(10, 10, 16));
        useFont(1);
        useDrawColor(COLOR(200, 210, 190));
        int w = textwidth(screenshotMessage);
        outtextxy(600 - w / 2, 676, screenshotMessage);
    }
}

// ===========================================================================
// 17.  SIMULATION CONTROL
// ===========================================================================
void clearParticles()
{
    for (int i = 0; i < MAX_LAVA; i++)   bombs[i].active = false;
    for (int i = 0; i < MAX_SMOKE; i++)  smoke[i].active = false;
    for (int i = 0; i < MAX_ASH; i++)    ash[i].active = false;
    for (int i = 0; i < MAX_EMBERS; i++) embers[i].active = false;
    meteor.active = false;
}

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

void updateSimulation(float dt)
{
    simTime += dt;

    // slowly changing wind
    wind = -0.22f + 0.30f * sinf(simTime * 0.07f);

    // smooth glow: bright while erupting, dim embers when stopped
    float targetGlow = erupting ? (0.45f + 0.055f * intensity) : 0.18f;
    glowLevel += (targetGlow - glowLevel) * 0.045f;

    if (erupting) {
        // spawn lava bombs (fractional accumulator => frame-rate independent)
        spawnAccum += (0.6f + intensity * 0.55f) * dt * 60.0f;
        while (spawnAccum >= 1.0f) {
            spawnLavaBomb();
            spawnAccum -= 1.0f;
        }

        // every few seconds a stronger burst
        if (simTime > nextBurst) {
            int burst = 10 + intensity * 2;
            for (int i = 0; i < burst; i++) spawnLavaBomb();
            nextBurst = simTime + randomFloat(2.6f, 5.2f);
        }

        // smoke
        smokeAccum += (0.30f + intensity * 0.075f) * dt * 60.0f;
        while (smokeAccum >= 1.0f) {
            spawnSmoke();
            smokeAccum -= 1.0f;
        }
    }

    // ash and embers keep coming from the hot crater, even after "stop"
    if (erupting || glowLevel > 0.25f) {
        ashAccum += 0.8f * dt * 60.0f;
        while (ashAccum >= 1.0f) {
            spawnAsh();
            ashAccum -= 1.0f;
        }
        if (rand() % 14 == 0) spawnEmber();
    }

    updateLavaBombs(dt);
    updateSmoke(dt);
    updateAsh(dt);
    updateEmbers(dt);
    updateMeteor(dt);
}

void handleInput()
{
    while (kbhit()) {
        int key = getch();

        // WinBGIm sends 0 (or 224) before special keys such as arrows.
        if (key == 0 || key == 224) { getch(); continue; }

        switch (key) {
            case 27:                       // ESC
                running = false;
                break;
            case ' ':
                erupting = !erupting;
                break;
            case 'p': case 'P':
                paused = !paused;
                break;
            case 'r': case 'R':
                resetSimulation();
                break;
            case '+': case '=':
                if (intensity < 10) intensity++;
                break;
            case '-': case '_':
                if (intensity > 1) intensity--;
                break;
            case 's': case 'S':
                writeimagefile("volcanox_shot.bmp", 0, 0,
                               SCREEN_W - 1, SCREEN_H - 1, true);
                sprintf(screenshotMessage, "Saved volcanox_shot.bmp");
                screenshotMessageUntil = GetTickCount() + 2500;
                break;
        }
    }
}

// ===========================================================================
// 18.  MAIN
// ===========================================================================
int main(int argc, char *argv[])
{
    // ---- optional developer switches -------------------------------------
    bool benchMode = false;
    int  benchFrames = 0;
    const char *shotFile = NULL;
    int  forcedIntensity = 0;
    bool startPaused = false;
    bool startStopped = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
            benchMode = true;
            benchFrames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shotFile = argv[++i];
        } else if (strcmp(argv[i], "--intensity") == 0 && i + 1 < argc) {
            forcedIntensity = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--paused") == 0) {
            startPaused = true;
        } else if (strcmp(argv[i], "--stopped") == 0) {
            startStopped = true;
        }
    }

    // ---- open the graphics window ----------------------------------------
    initwindow(SCREEN_W, SCREEN_H,
               "VolcanoX - Interactive Volcano Eruption Simulator",
               90, 15, true, true);

    if (getmaxx() < SCREEN_W - 1) {
        closegraph();
        printf("Could not open the graphics window.\n");
        return 1;
    }

    setbkcolor(COLOR(0, 0, 0));
    cleardevice();
    setrefreshingbgi(false);       // we refresh once per frame with swapbuffers()

    srand((unsigned)time(NULL));
    buildSkyPalette();
    buildBackground();
    buildFlows();
    resetSimulation();
    srand((unsigned)time(NULL));

    // developer overrides (used for automated screenshots)
    if (forcedIntensity > 0) {
        if (forcedIntensity < 1) forcedIntensity = 1;
        if (forcedIntensity > 10) forcedIntensity = 10;
        intensity = forcedIntensity;
    }
    if (startPaused)  paused = true;
    if (startStopped) erupting = false;

    unsigned long lastFrame = GetTickCount();
    unsigned long benchStart = lastFrame;
    int frames = 0;

    // ---- main loop --------------------------------------------------------
    while (running) {
        unsigned long frameStart = GetTickCount();
        float dt = (frameStart - lastFrame) / 1000.0f;
        lastFrame = frameStart;

        if (benchMode) dt = 1.0f / 60.0f;
        dt = clampFloat(dt, 0.001f, 0.05f);

        handleInput();

        if (!paused) updateSimulation(dt);

        // --- draw one complete frame --------------------------------------
        restoreBackground();

        // The plume is drawn first so the crater rim and lava lake sit in
        // front of the smoke: the vent then looks like it is inside the bowl.
        drawSmoke();
        drawCrater();          // rays, glow dome, lava lake, rim
        drawLavaFlows();
        drawGroundPools();
        drawLavaBombs();
        drawAsh();
        drawEmbers();
        drawMeteor();

        // FPS smoothing
        float instant = 1.0f / dt;
        fps = fps * 0.90f + instant * 0.10f;

        drawHud();

        // --- present it ----------------------------------------------------
        if (benchMode) {
            frames++;
            if (frames >= benchFrames) {
                if (shotFile != NULL)
                    writeimagefile(shotFile, 0, 0, SCREEN_W - 1, SCREEN_H - 1, true);
                unsigned long elapsed = GetTickCount() - benchStart;
                printf("Rendered %d frames in %lu ms  (%.2f ms/frame)\n",
                       frames, elapsed, (double)elapsed / (double)frames);
                break;
            }
        }

        swapbuffers();

        if (!benchMode) {
            unsigned long spent = GetTickCount() - frameStart;
            if (spent < 16) delay((int)(16 - spent));
        }
    }

    free(backgroundImage);
    closegraph();
    return 0;
}
