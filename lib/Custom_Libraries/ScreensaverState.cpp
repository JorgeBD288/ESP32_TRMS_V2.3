#include "ScreensaverState.h"

// Definición de variables globales
PrevScreenId g_prevScreenId = SCR_NONE;
bool         g_screensaverActive = false;
uint32_t     g_lastActivityMs = 0;   // contador de actividad

/**
 * @brief 
 * Registra actividad del usuario.
 * @note
  Actualiza el contador de última actividad
  al tiempo actual (millis()).
 */

void RegisterActivity()
{
    g_lastActivityMs = millis();
}
