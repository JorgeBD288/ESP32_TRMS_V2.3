#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

void DrawTRMSFigure(TFT_eSPI &tft, int centerX, int baseY);
void Load_bar(TFT_eSPI &tft);

