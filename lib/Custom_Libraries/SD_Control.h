// Control_SD.h
#pragma once

#include <Arduino.h>

#define SD_CS 5

// Guarda g_pidMin, g_pidCurr y g_pidMax en /Config_PID/Config_N
bool ControlSD_SaveConfig(uint8_t index);

// Carga g_pidMin, g_pidCurr y g_pidMax desde /Config_PID/Config_N
// y actualiza sliders + labels
bool ControlSD_LoadConfig(uint8_t index);

extern bool flag_Config_Message;