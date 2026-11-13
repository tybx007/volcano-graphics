// tools/filltest.cpp - does fillellipse/sector outline use setcolor?
#include <graphics.h>
#include <stdio.h>
#include <windows.h>

int main()
{
    initwindow(420, 320, "fill test", 40, 40, false, true);
    setbkcolor(COLOR(0, 0, 0));
    cleardevice();

    setcolor(COLOR(255, 255, 255));          // white outline
    setfillstyle(SOLID_FILL, COLOR(220, 40, 40)); // red fill
    fillellipse(110, 100, 60, 35);

    setcolor(COLOR(0, 255, 255));            // cyan outline
    setfillstyle(SOLID_FILL, COLOR(40, 220, 40)); // green fill
    sector(300, 100, 0, 180, 60, 35);

    setcolor(COLOR(255, 255, 0));            // yellow outline
    setfillstyle(SOLID_FILL, COLOR(40, 40, 220)); // blue fill
    bar(60, 200, 360, 280);

    writeimagefile("tools/filltest.bmp", 0, 0, 419, 319, false);
    closegraph();
    return 0;
}
