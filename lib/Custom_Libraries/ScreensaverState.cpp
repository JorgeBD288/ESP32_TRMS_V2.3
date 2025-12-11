#include "ScreensaverState.h"

// Definición de variables globales
PrevScreenId g_prevScreenId = SCR_NONE;
bool         g_screensaverActive = false;
uint32_t     g_lastActivityMs = 0;   // contador de actividad

// Definición de la función
void RegisterActivity()
{
    g_lastActivityMs = millis();
}
