#include "Load_Screen.h"
#include <lvgl.h>
#include <TFT_eSPI.h>

const int barX = 40;
const int barY = 260;
const int barW = 400;
const int barH = 20;
const int steps = 60;          // número de pasos de la barra
const int stepDelay = 3000 / steps;  // 3 s repartidos en 60 pasos (~50 ms)


// Dibuja un TRMS esquemático
void DrawTRMSFigure(TFT_eSPI &tft, int centerX, int baseY)
{
    uint16_t col = TFT_RED;

    // --- Parámetros generales (ligeramente más pequeños) ---
    int baseWidth   = 190;   // antes 220
    int baseHeight  = 24;    // antes 30
    int mastHeight  = 50;    // antes 60
    int mastWidth   = 14;    // antes 18

    int beamLength  = 200;   // algo más largo que antes (180)
    int beamY       = baseY - mastHeight - 15;
    int mastTopY    = baseY - mastHeight;

    // --- BASE (paralelepípedo) ---
    int baseX      = centerX - baseWidth / 2;
    int baseYtop   = baseY;
    int baseYbottom= baseY + baseHeight;

    tft.drawRect(baseX, baseYtop, baseWidth, baseHeight, col);

    int depth = 16; // un poco más pequeño
    tft.drawLine(baseX + baseWidth, baseYtop,
                 baseX + baseWidth + depth, baseYtop - depth, col);
    tft.drawLine(baseX + baseWidth, baseYbottom,
                 baseX + baseWidth + depth, baseYbottom - depth, col);
    tft.drawLine(baseX + baseWidth + depth, baseYtop - depth,
                 baseX + baseWidth + depth, baseYbottom - depth, col);

    // --- MÁSTIL ---
    int mastX = centerX - mastWidth / 2;
    tft.fillRect(mastX, mastTopY, mastWidth, mastHeight, TFT_BLACK);
    tft.drawRect(mastX, mastTopY, mastWidth, mastHeight, col);

    // --- CAJA SUPERIOR (prisma hueco) ---
    int boxW      = 70;   // más pequeña
    int boxH      = 18;
    int boxDepth  = 10;

    int boxFrontX = centerX - boxW / 2;
    int boxFrontY = beamY - boxH / 2;

    // Frente
    tft.drawRect(boxFrontX, boxFrontY, boxW, boxH, col);

    // Cara superior
    int boxTopX = boxFrontX + boxDepth;
    int boxTopY = boxFrontY - boxDepth;
    tft.drawRect(boxTopX, boxTopY, boxW, boxH, col);

    // Aristas entre caras
    tft.drawLine(boxFrontX,            boxFrontY,
                 boxTopX,              boxTopY, col);
    tft.drawLine(boxFrontX + boxW,     boxFrontY,
                 boxTopX + boxW,       boxTopY, col);
    tft.drawLine(boxFrontX,            boxFrontY + boxH,
                 boxTopX,              boxTopY + boxH, col);
    tft.drawLine(boxFrontX + boxW,     boxFrontY + boxH,
                 boxTopX + boxW,       boxTopY + boxH, col);

    // --- BRAZO HORIZONTAL (sin pasar por dentro de la caja) ---
    int beamYline  = beamY;
    int beamLeftX  = centerX - beamLength / 2;
    int beamRightX = centerX + beamLength / 2;

    // Lado izquierdo hasta el borde de la caja
    int beamLeftEnd = boxFrontX - 2;               // pequeño margen
    if (beamLeftEnd > beamLeftX) {
        tft.drawLine(beamLeftX, beamYline,
                     beamLeftEnd, beamYline, col);
    }

    // Lado derecho desde el otro borde de la caja
    int beamRightStart = boxFrontX + boxW + 2;     // pequeño margen
    if (beamRightStart < beamRightX) {
        tft.drawLine(beamRightStart, beamYline,
                     beamRightX, beamYline, col);
    }

    // --- ROTORES ---
    // Rotor izquierdo (ligeramente más pequeño para que todo se vea más compacto)
    int leftRotorR = 14;
    int leftRotorX = beamLeftX;
    int leftRotorY = beamYline;

    tft.drawCircle(leftRotorX, leftRotorY, leftRotorR, col);
    tft.drawLine(leftRotorX - leftRotorR, leftRotorY,
                 leftRotorX + leftRotorR, leftRotorY, col);
    tft.drawLine(leftRotorX, leftRotorY - leftRotorR,
                 leftRotorX, leftRotorY + leftRotorR, col);

    // Rotor derecho (más grande que antes)
    int rightRotorR = 32;   // antes 26
    int rightRotorX = beamRightX;
    int rightRotorY = beamYline;

    tft.drawCircle(rightRotorX, rightRotorY, rightRotorR, col);
    tft.drawLine(rightRotorX - rightRotorR, rightRotorY,
                 rightRotorX + rightRotorR, rightRotorY, col);
    tft.drawLine(rightRotorX, rightRotorY - rightRotorR,
                 rightRotorX, rightRotorY + rightRotorR, col);

    // --- Pilar entre caja y mástil ---
    int pillarW    = 8;
    int pillarX    = centerX - pillarW / 2;
    int pillarTopY = boxFrontY + boxH;         // parte inferior de la caja
    int pillarH    = mastTopY - pillarTopY;    // puede ser pequeño

    if (pillarH > 0) {
        tft.fillRect(pillarX, pillarTopY, pillarW, pillarH, TFT_BLACK);
        tft.drawRect(pillarX, pillarTopY, pillarW, pillarH, col);
    }
}

void Load_bar(TFT_eSPI &tft)
{
    // Borde de la barra
    tft.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, TFT_ORANGE);

    // Relleno progresivo
    for (int i = 0; i <= steps; ++i) {
        int w = (barW * i) / steps;
        tft.fillRect(barX, barY, w, barH, TFT_ORANGE);
        delay(stepDelay);
    }
}