/* ============================================================================
   STAGE 4 - SMOKE, ASH, EMBERS AND A METEOR
   ----------------------------------------------------------------------------
   Build:  see docs/SETUP.md        Run: stage4.exe
   Developer: stage4.exe --shot     renders 6 seconds, saves stage4_shot.bmp

   New in this stage
       1. smoke puffs: spawned at the vent, they rise, drift with the wind,
          grow and are drawn as three soft concentric ellipses
       2. ash flakes: small slow specks that fall around the plume
       3. embers: short-lived sparks that flicker inside the smoke
       4. a rare meteor that streaks across the night sky
       5. a statistics panel with live FPS and particle counts

   The draw order matters: the plume goes behind the crater rim so the lava
   lake stays visible inside the bowl.  Stage 5 adds the keyboard controls,
   pause overlay, reset and adjustable intensity.
   ========================================================================= */

#include <graphics.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <windows.h>

// ---------------------------------------------------------------------------
//  constants
// ---------------------------------------------------------------------------
const int SCREEN_W = 1200;
const int SCREEN_H = 760;
const int HORIZON_Y = 560;
const int GROUND_Y  = 690;
const int CRATER_X  = 600;
const int CRATER_Y  = 271;
const int CRATER_RX = 44;
const int CRATER_RY = 12;
const int MAX_STARS = 220;
const int MAX_LAVA  = 220;
const int MAX_SMOKE = 56;
const int MAX_ASH   = 80;
const int MAX_EMBERS = 18;
const int FLOW_POINTS = 16;
const int NUM_FLOWS = 2;
const float PI_F = 3.14159265f;

// ---------------------------------------------------------------------------
//  palette
// ---------------------------------------------------------------------------
const int SKY_TOP     = COLOR(  4,   6,  15);
const int SKY_MID     = COLOR( 11,  13,  32);
const int SKY_LOW     = COLOR( 28,  19,  45);
const int SKY_HORIZON = COLOR( 62,  32,  38);

const int MOON_MAIN   = COLOR(238, 236, 214);
const int MOON_SHADE  = COLOR(204, 204, 186);
const int MOON_DOT    = COLOR(190, 190, 172);

const int ROCK_DARK   = COLOR( 28,  24,  29);
const int ROCK_MID    = COLOR( 54,  45,  43);
const int ROCK_LIGHT  = COLOR( 92,  76,  64);
const int ROCK_MOON   = COLOR(120, 112, 112);
const int ROCK_WARM   = COLOR(104,  61,  44);

const int GROUND_DARK = COLOR( 14,  12,  17);
const int GROUND_MID  = COLOR( 23,  20,  26);
const int GROUND_LIT  = COLOR( 38,  30,  30);

const int PANEL_BG    = COLOR(  9,   9,  15);
const int TEXT_MAIN   = COLOR(232, 232, 240);
const int TEXT_DIM    = COLOR(138, 144, 160);
const int ACCENT      = COLOR(255, 138,  45);

const int LAVA_COLORS[6] = {
    COLOR(255, 255, 222), COLOR(255, 238, 142), COLOR(255, 198, 70),
    COLOR(255, 148,  36), COLOR(236,  92,  20), COLOR(168, 44, 14)
};

// ---------------------------------------------------------------------------
//  data
// ---------------------------------------------------------------------------
struct Star { int x, y, brightness; };

struct LavaBomb {
    float x, y, vx, vy;
    float life, maxLife;
    float heat;
    int   size;
    bool  active;
};

struct LavaFlow {
    int x[FLOW_POINTS];
    int y[FLOW_POINTS];
    int n;
    float phase;
};

struct SmokePuff {
    float x, y, vx, vy;
    float radius, grow;
    float life, maxLife;
    float wobble;
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

Star stars[MAX_STARS];
LavaBomb bombs[MAX_LAVA];
SmokePuff smoke[MAX_SMOKE];
AshFlake ash[MAX_ASH];
Ember embers[MAX_EMBERS];
LavaFlow flows[NUM_FLOWS];
Meteor meteor;

int  skyPalette[SCREEN_H];
void *backgroundImage = NULL;

float simTime = 0.0f;
float glowLevel = 0.60f;
float spawnAccum = 0.0f;
float smokeAccum = 0.0f;
float ashAccum = 0.0f;
float nextBurst = 2.0f;
float wind = -0.25f;
float fps = 60.0f;
int   intensity = 5;
bool  running = true;
bool  erupting = true;

// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
int makeColor(int r, int g, int b) { return COLOR(r, g, b); }
int redOf(int c)   { return  c        & 0xFF; }
int greenOf(int c) { return (c >> 8)  & 0xFF; }
int blueOf(int c)  { return (c >> 16) & 0xFF; }

int mixColor(int c1, int c2, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return makeColor((int)(redOf(c1)   + (redOf(c2)   - redOf(c1))   * t),
                     (int)(greenOf(c1) + (greenOf(c2) - greenOf(c1)) * t),
                     (int)(blueOf(c1)  + (blueOf(c2)  - blueOf(c1))  * t));
}

float clampFloat(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
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

int cachedDraw = -999, cachedFill = -999, cachedFont = -1;
int cachedTextBg = -999, cachedThickness = -1;

void useDrawColor(int c) { if (c != cachedDraw) { setcolor(c); cachedDraw = c; } }
void useFillColor(int c)
{
    if (c != cachedFill) { setfillstyle(SOLID_FILL, c); cachedFill = c; }
    // filled shapes are outlined with the DRAW colour in WinBGIm
    if (c != cachedDraw) { setcolor(c); cachedDraw = c; }
}
void useFont(int size)
{
    if (size != cachedFont) { settextstyle(SANS_SERIF_FONT, HORIZ_DIR, size); cachedFont = size; }
}
void useTextBackground(int c) { if (c != cachedTextBg) { setbkcolor(c); cachedTextBg = c; } }
void useThickness(int t)
{
    if (t != cachedThickness) { setlinestyle(SOLID_LINE, 0, t); cachedThickness = t; }
}

// ---------------------------------------------------------------------------
//  sky, stars, moon, mountains, ground (same as stage 2)
// ---------------------------------------------------------------------------
void buildSkyPalette()
{
    for (int y = 0; y < SCREEN_H; y++) {
        if (y < 170)       skyPalette[y] = mixColor(SKY_TOP, SKY_MID, y / 170.0f);
        else if (y < 380)  skyPalette[y] = mixColor(SKY_MID, SKY_LOW, (y - 170) / 210.0f);
        else if (y < 520)  skyPalette[y] = mixColor(SKY_LOW, SKY_HORIZON, (y - 380) / 140.0f);
        else               skyPalette[y] = SKY_HORIZON;
    }
}

int skyColorAt(int y)
{
    if (y < 0) y = 0;
    if (y >= SCREEN_H) y = SCREEN_H - 1;
    return skyPalette[y];
}

void drawSky()
{
    for (int y = 0; y < SCREEN_H; y++) {
        useDrawColor(skyPalette[y]);
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
        float haze = 1.0f - stars[i].y / 620.0f;
        if (haze < 0.35f) haze = 0.35f;
        int c = makeColor((int)(b * haze), (int)(b * haze),
                          (int)((b * 0.88f + 22) * haze));
        putpixel(stars[i].x, stars[i].y, c);
        if (b > 225) {
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
    for (int i = 10; i >= 1; i--) {
        useDrawColor(mixColor(skyColorAt(cy), makeColor(186, 196, 224),
                              0.025f + 0.012f * (11 - i)));
        circle(cx, cy, r + i * 5);
    }

    useFillColor(MOON_MAIN);
    fillellipse(cx, cy, r, r);

    useFillColor(MOON_DOT);
    fillellipse(cx - 13, cy - 10, 12, 9);
    fillellipse(cx + 14, cy + 7, 8, 6);
    fillellipse(cx + 1, cy + 20, 6, 4);
    fillellipse(cx - 20, cy + 13, 4, 3);

    useFillColor(MOON_SHADE);
    fillellipse(cx - 12, cy - 11, 6, 4);
    fillellipse(cx + 15, cy + 8, 4, 3);

    useDrawColor(makeColor(215, 214, 198));
    for (int i = 0; i < 12; i++) {
        int yy = cy - r + 6 + i * (2 * r - 12) / 11;
        line(cx + (int)(r * 0.55), yy, cx + r - 2, yy);
    }
}

void drawMountainRange(int baseY, int minHeight, int maxHeight, int color)
{
    int pts[400];
    int n = 0;
    pts[n++] = -20; pts[n++] = baseY;
    int x = -20;
    while (x < SCREEN_W + 20) {
        x += randomInt(24, 58);
        pts[n++] = x; pts[n++] = baseY - randomInt(0, 14);
        x += randomInt(26, 64);
        pts[n++] = x; pts[n++] = baseY - minHeight - randomInt(0, maxHeight - minHeight);
    }
    pts[n++] = SCREEN_W + 20; pts[n++] = baseY;
    pts[n++] = SCREEN_W + 20; pts[n++] = SCREEN_H;
    pts[n++] = -20;           pts[n++] = SCREEN_H;
    useFillColor(color);
    fillpoly(n / 2, pts);
}

void drawDistantRanges()
{
    drawMountainRange(556, 58, 120, COLOR(26, 25, 43));
    drawMountainRange(575, 32,  78, COLOR(16, 16, 29));
}

void drawGround()
{
    for (int y = HORIZON_Y; y < SCREEN_H; y++) {
        float t = (y - HORIZON_Y) / 200.0f;
        if (t > 1.0f) t = 1.0f;
        useDrawColor(mixColor(GROUND_MID, GROUND_DARK, t * t));
        line(0, y, SCREEN_W - 1, y);
    }
}

// ---------------------------------------------------------------------------
//  volcano geometry
// ---------------------------------------------------------------------------
float leftEdge(float y)
{
    float u = clampFloat((y - 266.0f) / (GROUND_Y - 266), 0.0f, 1.0f);
    float x = 546.0f - 426.0f * pow(u, 0.82f);
    x += u * (7.0f * sinf(y * 0.055f) + 3.0f * sinf(y * 0.17f));
    x += 26.0f * expf(-pow((y - 470.0f) / 58.0f, 2)) * u;
    return x;
}

float rightEdge(float y)
{
    float u = clampFloat((y - 266.0f) / (GROUND_Y - 266), 0.0f, 1.0f);
    float x = 654.0f + 436.0f * pow(u, 0.80f);
    x += u * (6.0f * sinf(y * 0.061f + 1.3f) + 3.0f * sinf(y * 0.15f + 0.7f));
    x += 18.0f * expf(-pow((y - 520.0f) / 70.0f, 2)) * u;
    return x;
}

// Height of the solid surface at x - used to make bombs land on the slope.
int surfaceYAt(float x)
{
    if (x < 100.0f || x > 1110.0f) return GROUND_Y - 2;
    if (x >= 552.0f && x <= 648.0f) return CRATER_Y + CRATER_RY;
    if (x < 552.0f) {
        float u = powf(clampFloat((546.0f - x) / 426.0f, 0.0f, 1.0f), 1.0f / 0.82f);
        return 266 + (int)(u * (GROUND_Y - 266));
    }
    float u = powf(clampFloat((x - 654.0f) / 436.0f, 0.0f, 1.0f), 1.0f / 0.80f);
    return 266 + (int)(u * (GROUND_Y - 266));
}

void drawSummitGlow()
{
    for (int i = 16; i >= 1; i--) {
        int rx = 42 + i * 14, ry = 14 + i * 5;
        float a = 0.05f * (1.0f - i / 26.0f);
        useFillColor(mixColor(skyColorAt(250), makeColor(255, 126, 46), a));
        fillellipse(CRATER_X, CRATER_Y + 5, rx, ry);
    }
}

void drawVolcano()
{
    drawSummitGlow();

    int cap[10] = { 546, 266, 566, 255, 600, 250, 634, 255, 654, 266 };
    useFillColor(ROCK_MID);
    fillpoly(5, cap);

    const int BANDS = 14;
    for (int y = 266; y <= GROUND_Y; y++) {
        int xL = (int)leftEdge(y);
        int xR = (int)rightEdge(y);
        if (xR <= xL) continue;
        float depth = (y - 266.0f) / (GROUND_Y - 266);
        for (int b = 0; b < BANDS; b++) {
            int x1 = xL + (xR - xL) * b / BANDS;
            int x2 = xL + (xR - xL) * (b + 1) / BANDS;
            if (x2 <= x1) x2 = x1 + 1;
            float t = (b + 0.5f) / BANDS;
            float light = 0.10f + 0.92f * pow(t, 1.55f);
            light += 0.045f * sinf(x1 * 0.017f + y * 0.051f)
                   + 0.022f * sinf(x1 * 0.083f - y * 0.019f);
            light *= 0.74f + 0.26f * (1.0f - depth);
            if (b == 0) light -= 0.10f;
            if (b == BANDS - 1) light += 0.12f;
            int c = mixColor(ROCK_DARK, ROCK_LIGHT, clampFloat(light, 0.0f, 1.0f));
            if (b >= BANDS - 2) c = mixColor(c, ROCK_MOON, 0.18f);
            useDrawColor(c);
            line(x1, y, x2, y);
        }
    }

    for (int y = 266; y <= GROUND_Y; y += 2) {
        int xL = (int)leftEdge(y);
        useDrawColor(mixColor(ROCK_DARK, makeColor(8, 8, 12), 0.45f));
        line(xL, y, xL + 2, y + 2);
        int xR = (int)rightEdge(y);
        float t = (y - 266.0f) / (GROUND_Y - 266);
        useDrawColor(mixColor(ROCK_MOON, ROCK_LIGHT, 0.35f + 0.4f * t));
        line(xR - 2, y, xR, y + 2);
    }

    for (int k = 0; k < 20; k++) {
        float g = -0.92f + 1.84f * (k / 19.0f);
        int prevX = 0, prevY = 0;
        for (int y = 282; y <= GROUND_Y - 6; y += 9) {
            float u = (y - 266.0f) / (GROUND_Y - 266);
            float f = 0.5f + g * 0.5f * u;
            float x = leftEdge(y) + (rightEdge(y) - leftEdge(y)) * f;
            x += 3.0f * sinf(y * 0.09f + k * 2.1f);
            if (prevX != 0) {
                int c = (g < 0) ? mixColor(ROCK_DARK, makeColor(10, 9, 13), 0.35f)
                                : mixColor(ROCK_LIGHT, ROCK_WARM, 0.25f);
                useDrawColor(c);
                line(prevX, prevY, (int)x, y);
            }
            prevX = (int)x; prevY = y;
        }
    }

    for (int k = 0; k < 26; k++) {
        int y = randomInt(600, 686);
        float xL = leftEdge(y), xR = rightEdge(y);
        int x = (int)(xL + (xR - xL) * (randomInt(8, 92) / 100.0f));
        int w = randomInt(3, 9), h = randomInt(2, 6);
        int pts[8] = { x - w, y + h, x - w / 2, y - h,
                       x + w / 2, y - h, x + w, y + h };
        useFillColor(mixColor(ROCK_DARK, makeColor(12, 11, 15), 0.3f));
        fillpoly(4, pts);
        useDrawColor(mixColor(ROCK_LIGHT, ROCK_MOON, 0.25f));
        line(x - w / 2, y - h, x + w / 2, y - h);
    }
}

void drawForeground()
{
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

    for (int i = 0; i < 30; i++) {
        int x = randomInt(0, SCREEN_W);
        int y = randomInt(GROUND_Y + 8, SCREEN_H - 8);
        int w = randomInt(6, 26), h = randomInt(4, 14);
        int rock[10] = { x - w, y + h, x - w / 2, y - h,
                         x + w / 2, y - h / 2, x + w, y + h, x, y + h + 3 };
        useFillColor(COLOR(15, 14, 19));
        fillpoly(5, rock);
        useDrawColor(COLOR(48, 43, 48));
        line(x - w / 2, y - h, x + w / 2, y - h / 2);
    }
}

// ---------------------------------------------------------------------------
//  background cache
// ---------------------------------------------------------------------------
void buildBackground()
{
    buildSkyPalette();
    srand(20251113);

    drawSky();
    drawStars();
    drawMoon(1030, 210, 42);
    drawDistantRanges();
    drawGround();
    drawVolcano();
    drawForeground();

    unsigned size = imagesize(0, 0, SCREEN_W - 1, SCREEN_H - 1);
    backgroundImage = malloc(size);
    if (backgroundImage == NULL) {
        closegraph();
        printf("Not enough memory for the background image.\n");
        exit(1);
    }
    getimage(0, 0, SCREEN_W - 1, SCREEN_H - 1, backgroundImage);
}

void restoreBackground() { putimage(0, 0, backgroundImage, COPY_PUT); }

// ---------------------------------------------------------------------------
//  lava rivers
// ---------------------------------------------------------------------------
void buildFlows()
{
    for (int f = 0; f < NUM_FLOWS; f++) {
        flows[f].n = FLOW_POINTS;
        flows[f].phase = (f == 0) ? 0.0f : 0.47f;

        for (int i = 0; i < FLOW_POINTS; i++) {
            int y = 279 + i * 27;
            float u = (y - 266.0f) / (GROUND_Y - 266);
            float pull = (u > 0.62f) ? (u - 0.62f) / 0.38f : 0.0f;
            pull *= pull;

            if (f == 0)
                flows[f].x[i] = (int)(leftEdge(y) + 30.0f + 62.0f * pull)
                                + (int)(4.0f * sinf(i * 1.7f));
            else
                flows[f].x[i] = (int)(rightEdge(y) - 30.0f - 112.0f * pull)
                                + (int)(4.0f * sinf(i * 1.4f + 2.0f));
            flows[f].y[i] = y;
        }
    }
}

void drawLavaFlows()
{
    for (int f = 0; f < NUM_FLOWS; f++) {
        LavaFlow &fl = flows[f];

        // crust, body, hot core: three thick passes
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

        // bright blobs sliding down the river
        useFillColor(mixColor(COLOR(255, 190, 80), COLOR(255, 246, 190),
                              0.4f + 0.4f * glowLevel));
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

        useFillColor(mixColor(COLOR(255, 170, 60), COLOR(255, 240, 170), 0.55f));
        fillellipse(fl.x[0], fl.y[0] - 2, 7, 4);
    }
}

// ---------------------------------------------------------------------------
//  crater
// ---------------------------------------------------------------------------
void drawCrater()
{
    float pulse = 0.55f + 0.45f * sinf(simTime * 4.0f) * glowLevel;

    // rays behind the rim
    for (int i = 0; i < 7; i++) {
        float len = (90.0f + 55.0f * sinf(simTime * 1.7f + i * 1.3f))
                    * (0.5f + 0.7f * glowLevel);
        int tipX = CRATER_X + (int)((i - 3) * 38 + 16 * sinf(simTime * 0.9f + i));
        int tipY = (int)(CRATER_Y - 22 - len);
        int baseX = CRATER_X + (i - 3) * 9;
        int ray[6] = { baseX - 14, CRATER_Y - 6, baseX + 14, CRATER_Y - 6, tipX, tipY };
        useFillColor(mixColor(skyColorAt(200), makeColor(255, 138, 48),
                              0.03f + 0.055f * glowLevel));
        fillpoly(3, ray);
    }

    // dome of light
    int ref = skyColorAt(205);
    for (int i = 9; i >= 1; i--) {
        useFillColor(mixColor(ref, makeColor(255, 126, 44),
                              (0.035f + 0.030f * i) * glowLevel));
        fillellipse(CRATER_X, 202, 26 + i * 11, 7 + i * 4);
    }

    // crater bowl
    useFillColor(COLOR(34, 14, 14));
    fillellipse(CRATER_X, CRATER_Y, CRATER_RX, CRATER_RY);

    for (int i = 9; i >= 2; i--) {
        useFillColor(mixColor(COLOR(46, 17, 15), COLOR(150, 52, 22),
                              (10 - i) / 9.0f));
        sector(CRATER_X, CRATER_Y, 0, 180,
               CRATER_RX * i / 10, CRATER_RY * i / 10);
    }

    // hot pool (cools when glowLevel drops - used by stage 5)
    float heat = clampFloat((glowLevel - 0.15f) / 0.65f, 0.0f, 1.0f);
    useFillColor(mixColor(mixColor(COLOR(70, 18, 10), COLOR(220, 74, 18), 0.4f),
                          COLOR(255, 150, 44), pulse * heat));
    fillellipse(CRATER_X, CRATER_Y - 1, 34, 8);
    useFillColor(mixColor(COLOR(140, 44, 14), COLOR(255, 246, 190), pulse * heat));
    fillellipse(CRATER_X, CRATER_Y - 1, 24, 5);
    if (heat > 0.25f) {
        useFillColor(mixColor(COLOR(255, 190, 80), COLOR(255, 252, 215), heat));
        fillellipse(CRATER_X - 4, CRATER_Y - 2, 7, 2);
    }

    // rim
    useThickness(3);
    useDrawColor(mixColor(COLOR(150, 54, 20), COLOR(255, 170, 70), pulse));
    ellipse(CRATER_X, CRATER_Y, 0, 180, CRATER_RX + 1, CRATER_RY + 1);
    useThickness(1);
    useDrawColor(mixColor(COLOR(255, 140, 44), COLOR(255, 226, 140), pulse));
    ellipse(CRATER_X, CRATER_Y, 0, 180, CRATER_RX + 2, CRATER_RY + 2);
    ellipse(CRATER_X, CRATER_Y + 1, 0, 180, CRATER_RX + 5, CRATER_RY + 5);
    useDrawColor(mixColor(COLOR(120, 44, 22), COLOR(255, 128, 46), glowLevel * 0.8f));
    ellipse(CRATER_X, CRATER_Y + 1, 180, 360, CRATER_RX, CRATER_RY);
}

// ---------------------------------------------------------------------------
//  lava bombs
// ---------------------------------------------------------------------------
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
        bombs[i].vy += 0.145f;             // gravity
        bombs[i].vx *= 0.995f;             // air drag
        bombs[i].life -= dt;

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
    // draw in colour groups so the fill colour changes only six times
    for (int c = 0; c < 6; c++) {
        useFillColor(LAVA_COLORS[c]);
        for (int i = 0; i < MAX_LAVA; i++) {
            if (!bombs[i].active) continue;

            float age = 1.0f - bombs[i].life / bombs[i].maxLife;
            int idx = (int)(age * 5.0f + (1.0f - bombs[i].heat) * 1.5f);
            if (idx < 0) idx = 0;
            if (idx > 5) idx = 5;
            if (idx != c) continue;

            int x = (int)bombs[i].x;
            int y = (int)bombs[i].y;

            line(x, y, (int)(x - bombs[i].vx * 2.2f),
                      (int)(y - bombs[i].vy * 2.2f));
            if (bombs[i].size <= 1) {
                fillellipse(x, y, 1, 1);
                putpixel(x, y, mixColor(LAVA_COLORS[c], LAVA_COLORS[0], 0.5f));
            } else {
                fillellipse(x, y, 2, 2);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  ground pools
// ---------------------------------------------------------------------------
void drawGroundPools()
{
    int poolX[2] = { flows[0].x[flows[0].n - 1], flows[1].x[flows[1].n - 1] };
    int poolY[2] = { GROUND_Y + 15, GROUND_Y + 13 };

    for (int i = 0; i < 2; i++) {
        for (int k = 6; k >= 1; k--) {
            useFillColor(mixColor(COLOR(20, 16, 18), COLOR(255, 104, 30),
                                  0.03f + 0.035f * k * glowLevel));
            fillellipse(poolX[i], poolY[i] + 2, 22 + k * 11, 8 + k * 4);
        }

        useFillColor(mixColor(COLOR(120, 30, 10), COLOR(255, 120, 26),
                              0.5f + 0.3f * glowLevel));
        fillellipse(poolX[i], poolY[i], 34, 11);
        useFillColor(mixColor(COLOR(240, 110, 30), COLOR(255, 216, 120), 0.5f));
        fillellipse(poolX[i] - 3, poolY[i] - 1, 22, 7);
        useFillColor(mixColor(COLOR(255, 200, 100), COLOR(255, 250, 205), 0.5f));
        fillellipse(poolX[i] - 5, poolY[i] - 2, 11, 3);

        for (int s = 0; s < 4; s++) {
            int ox = (int)(sinf(simTime * 2.0f + s * 1.7f + i) * 16);
            putpixel(poolX[i] + ox, poolY[i] - 1, COLOR(255, 240, 170));
        }
    }
}

// ---------------------------------------------------------------------------
//  smoke
// ---------------------------------------------------------------------------
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
        smoke[i].vy -= 0.004f * dt * 60.0f;          // buoyancy
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
    int bases[3] = { COLOR(120, 78, 62), COLOR(78, 74, 84), COLOR(48, 48, 58) };

    // three nested layers per puff give a soft, lumpy edge
    for (int layer = 0; layer < 3; layer++) {
        for (int band = 0; band < 3; band++) {
            useFillColor(mixColor(skyRef, bases[band], 0.10f + layer * 0.13f));

            for (int i = 0; i < MAX_SMOKE; i++) {
                if (!smoke[i].active) continue;

                float age = 1.0f - smoke[i].life / smoke[i].maxLife;
                int b = (age < 0.30f) ? 0 : (age < 0.65f) ? 1 : 2;
                if (b != band) continue;

                float scale = (layer == 0) ? 1.0f : (layer == 1) ? 0.72f : 0.42f;
                int r = (int)(smoke[i].radius * scale);
                if (r < 2) r = 2;

                int ox = (int)((smoke[i].wobble - 0.5f) * smoke[i].radius * 0.55f
                               * (1.0f - layer * 0.3f));
                int oy = (int)((0.5f - smoke[i].wobble) * smoke[i].radius * 0.30f);
                fillellipse((int)smoke[i].x + ox, (int)smoke[i].y + oy,
                            r, (int)(r * 0.86f));
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  ash
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
//  embers
// ---------------------------------------------------------------------------
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
        embers[i].x += embers[i].vx * dt * 60.0f
                     + 0.25f * sinf(simTime * 2.0f + embers[i].phase);
        embers[i].y += embers[i].vy * dt * 60.0f;
        embers[i].life -= dt;
        if (embers[i].life <= 0 || embers[i].y < -20) embers[i].active = false;
    }
}

void drawEmbers()
{
    for (int i = 0; i < MAX_EMBERS; i++) {
        if (!embers[i].active) continue;
        float flicker = 0.5f + 0.5f * sinf(simTime * 11.0f + embers[i].phase);
        if (flicker > 0.35f) {
            putpixel((int)embers[i].x, (int)embers[i].y,
                     (flicker > 0.75f) ? COLOR(255, 226, 130)
                                       : COLOR(244, 130, 40));
        }
    }
}

// ---------------------------------------------------------------------------
//  meteor
// ---------------------------------------------------------------------------
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
        useDrawColor(mixColor(makeColor(255, 244, 210), skyColorAt((int)y), t * 1.1f));
        line((int)x, (int)y, (int)(x - meteor.vx * 0.35f),
             (int)(y - meteor.vy * 0.35f));
    }
    useDrawColor(COLOR(255, 252, 232));
    line((int)meteor.x, (int)meteor.y,
         (int)(meteor.x - meteor.vx), (int)(meteor.y - meteor.vy));
}

// ---------------------------------------------------------------------------
//  simulation
// ---------------------------------------------------------------------------
void resetSimulation()
{
    for (int i = 0; i < MAX_LAVA; i++)  bombs[i].active = false;
    for (int i = 0; i < MAX_SMOKE; i++) smoke[i].active = false;
    for (int i = 0; i < MAX_ASH; i++)   ash[i].active = false;
    for (int i = 0; i < MAX_EMBERS; i++) embers[i].active = false;
    meteor.active = false;

    simTime = 0.0f;
    glowLevel = 0.60f;
    spawnAccum = 0.0f;
    smokeAccum = 0.0f;
    ashAccum = 0.0f;
    nextBurst = 2.0f;
    wind = -0.25f;
}

void updateSimulation(float dt)
{
    simTime += dt;

    // slowly changing wind pushes the plume sideways
    wind = -0.22f + 0.30f * sinf(simTime * 0.07f);

    float targetGlow = erupting ? (0.45f + 0.055f * intensity) : 0.18f;
    glowLevel += (targetGlow - glowLevel) * 0.045f;

    if (erupting) {
        spawnAccum += (0.6f + intensity * 0.55f) * dt * 60.0f;
        while (spawnAccum >= 1.0f) {
            spawnLavaBomb();
            spawnAccum -= 1.0f;
        }

        if (simTime > nextBurst) {
            int burst = 10 + intensity * 2;
            for (int i = 0; i < burst; i++) spawnLavaBomb();
            nextBurst = simTime + randomFloat(2.6f, 5.2f);
        }

        smokeAccum += (0.30f + intensity * 0.075f) * dt * 60.0f;
        while (smokeAccum >= 1.0f) {
            spawnSmoke();
            smokeAccum -= 1.0f;
        }
    }

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

// ---------------------------------------------------------------------------
//  label
// ---------------------------------------------------------------------------
void drawHud(float fps)
{
    // title panel
    useFillColor(PANEL_BG);
    bar(24, 20, 430, 132);
    useDrawColor(COLOR(74, 48, 38));
    rectangle(24, 20, 430, 132);
    useDrawColor(COLOR(46, 38, 44));
    rectangle(27, 23, 427, 129);

    for (int i = 0; i < 6; i++) {
        useDrawColor(mixColor(ACCENT, PANEL_BG, i / 6.0f));
        line(24 + i, 26, 24 + i, 126);
    }

    useTextBackground(PANEL_BG);
    useFont(4);
    useDrawColor(ACCENT);
    outtextxy(44, 30, (char *)"STAGE 4");

    useFont(1);
    useDrawColor(TEXT_MAIN);
    outtextxy(46, 70, (char *)"Smoke, ash, embers and a meteor");
    useDrawColor(TEXT_DIM);
    outtextxy(46, 100, (char *)"ESC = exit");

    // statistics panel
    useFillColor(PANEL_BG);
    bar(884, 20, 1176, 124);
    useDrawColor(COLOR(74, 48, 38));
    rectangle(884, 20, 1176, 124);
    useDrawColor(COLOR(46, 38, 44));
    rectangle(887, 23, 1173, 121);

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
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    bool shotMode = (argc > 1 && strcmp(argv[1], "--shot") == 0);

    initwindow(SCREEN_W, SCREEN_H, "Stage 4 - Particles", 90, 15, true, true);
    setbkcolor(COLOR(0, 0, 0));
    cleardevice();
    setrefreshingbgi(false);

    srand((unsigned)time(NULL));
    buildBackground();
    buildFlows();
    resetSimulation();

    unsigned long last = GetTickCount();
    int frames = 0;
    float fps = 60.0f;

    while (running) {
        unsigned long frameStart = GetTickCount();
        float dt = (frameStart - last) / 1000.0f;
        last = frameStart;
        if (shotMode) dt = 1.0f / 60.0f;
        dt = clampFloat(dt, 0.001f, 0.05f);

        if (kbhit()) {
            int key = getch();
            if (key == 0 || key == 224) getch();
            else if (key == 27) running = false;
        }

        updateSimulation(dt);
        restoreBackground();

        // plume behind the crater rim, particles in front of the cone
        drawSmoke();
        drawCrater();
        drawLavaFlows();
        drawGroundPools();
        drawLavaBombs();
        drawAsh();
        drawEmbers();
        drawMeteor();

        fps = fps * 0.9f + (1.0f / dt) * 0.1f;
        drawHud(fps);
        swapbuffers();

        frames++;
        if (shotMode) {
            if (frames >= 360) {
                writeimagefile("stage4_shot.bmp", 0, 0,
                               SCREEN_W - 1, SCREEN_H - 1, true);
                running = false;
            }
        } else {
            unsigned long spent = GetTickCount() - frameStart;
            if (spent < 16) delay((int)(16 - spent));
        }
    }

    free(backgroundImage);
    closegraph();
    return 0;
}
