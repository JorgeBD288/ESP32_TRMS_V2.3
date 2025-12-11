/*Este archivo, y su correspondiente ".cpp" son para establecer los parámetros que controlan el 
funcionamiento del control PID*/

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"

// ==============================
// Estructuras de parámetros PID
// ==============================

// Valores actuales (CURR)
struct PID_CURR {
    float KpvvCurr;
    float KpvhCurr;
    float KivvCurr;
    float KivhCurr;
    float KdvvCurr;
    float KdvhCurr;

    float KphvCurr;
    float KphhCurr;
    float KihvCurr;
    float KihhCurr;
    float KdhvCurr;
    float KdhhCurr;

    float IsatvvCurr;
    float IsatvhCurr;
    float IsathvCurr;
    float IsathhCurr;

    float UvmaxCurr;
    float UhmaxCurr;
};

// Valores mínimos (MIN)
struct PID_MIN {
    float KpvvMin;
    float KpvhMin;
    float KivvMin;
    float KivhMin;
    float KdvvMin;
    float KdvhMin;

    float KphvMin;
    float KphhMin;
    float KihvMin;
    float KihhMin;
    float KdhvMin;
    float KdhhMin;

    float IsatvvMin;
    float IsatvhMin;
    float IsathvMin;
    float IsathhMin;

    float UvmaxMin;
    float UhmaxMin;
};

// Valores máximos (MAX)
struct PID_MAX {
    float KpvvMax;
    float KpvhMax;
    float KivvMax;
    float KivhMax;
    float KdvvMax;
    float KdvhMax;

    float KphvMax;
    float KphhMax;
    float KihvMax;
    float KihhMax;
    float KdhvMax;
    float KdhhMax;

    float IsatvvMax;
    float IsatvhMax;
    float IsathvMax;
    float IsathhMax;

    float UvmaxMax;
    float UhmaxMax;
};

// ==============================
// Declaración de instancias
// (se definirán en Parameters.cpp)
// ==============================

extern PID_CURR g_pidCurr;
extern PID_MIN  g_pidMin;
extern PID_MAX  g_pidMax;

void PID_LoadDefaults();
void PID_UpdateParamLabel(lv_obj_t * label, float minVal, float currVal, float maxVal);

// Manejo de la EEPROM

void PID_SaveToNVS();
void PID_LoadFromNVS();
