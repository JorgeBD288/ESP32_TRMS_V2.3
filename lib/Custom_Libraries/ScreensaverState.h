// ScreensaverState.h
#pragma once

#include <Arduino.h>

// Identificador de la pantalla anterior
enum PrevScreenId {
    SCR_NONE = 0,
    SCR_1,
    SCR_2,
    SCR_3,
    SCR_4,
    SCR_5,
    SCR_6,
    SCR_7,
    SCR_8,
    SCR_9,
    SCR_11,
    // añade más si tienes más pantallas
};


// Variables globales compartidas
extern PrevScreenId g_prevScreenId;
extern PrevScreenId g_prevScreenId_temp;
extern bool g_screensaverActive;

// Declaración de la función
void RegisterActivity();