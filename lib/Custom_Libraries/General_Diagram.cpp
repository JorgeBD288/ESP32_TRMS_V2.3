#include "General_Diagram.h"
#include "ui.h"
#include <Arduino.h>

// ====== DEFINICIONES GLOBALES ======

lv_obj_t * ui_BtnPIDZone  = nullptr;
lv_obj_t * ui_BtnTRMSZone = nullptr;

// ===============================
// ZONAS ACTIVAS (coordenadas pantalla)
// ===============================

// Primer cuadrado azul → PID completo
const int ESQ_PID_X1 = 136;
const int ESQ_PID_X2 = 182;
const int ESQ_PID_Y1 = 30;
const int ESQ_PID_Y2 = 127;

// Segundo cuadrado azul → TRMS completo
const int ESQ_TRMS_X1 = 240;
const int ESQ_TRMS_X2 = 286;
const int ESQ_TRMS_Y1 = 30;
const int ESQ_TRMS_Y2 = 127;

// ===============================
// HELPERS INTERNOS
// ===============================

static void ShowPIDBlock()
{
    // Ocultar esquema general
    lv_obj_add_flag(ui_EsquemaGeneral, LV_OBJ_FLAG_HIDDEN);

    // Mostrar imagen del PID completo
    lv_obj_clear_flag(ui_EsquemaPIDCompleto, LV_OBJ_FLAG_HIDDEN);
    // Ocultar la otra
    lv_obj_add_flag(ui_EsquemaTRMSCompleto, LV_OBJ_FLAG_HIDDEN);

    Serial.println("[GeneralDiagram] Seleccionado bloque PID completo");
}

static void ShowTRMSBlock()
{
    // Ocultar esquema general
    lv_obj_add_flag(ui_EsquemaGeneral, LV_OBJ_FLAG_HIDDEN);

    // Mostrar imagen del TRMS completo
    lv_obj_clear_flag(ui_EsquemaTRMSCompleto, LV_OBJ_FLAG_HIDDEN);
    // Ocultar la otra
    lv_obj_add_flag(ui_EsquemaPIDCompleto, LV_OBJ_FLAG_HIDDEN);

    Serial.println("[GeneralDiagram] Seleccionado bloque TRMS completo");
}


// ===============================
// CALLBACK DE EVENTO
// ===============================

static void GeneralDiagram_Event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    // Solo actuamos al SOLTAR el dedo (tap)
    if (code != LV_EVENT_RELEASED) {
        return;
    }

    // Punto táctil en coordenadas de pantalla
    lv_indev_t * indev = lv_indev_get_act();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    Serial.print("[GeneralDiagram] Touch (screen) x=");
    Serial.print(p.x);
    Serial.print(" y=");
    Serial.println(p.y);

    // -----------------------------------------
    // Convertir a coordenadas relativas al objeto
    // -----------------------------------------
    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_coord_t x_rel = p.x - obj_coords.x1;
    lv_coord_t y_rel = p.y - obj_coords.y1;

    Serial.print("[GeneralDiagram] Touch (obj) x=");
    Serial.print(x_rel);
    Serial.print(" y=");
    Serial.println(y_rel);

    // --- 1) ¿Hemos tocado el bloque PID completo? ---
    bool hitPID =
        (x_rel >= ESQ_PID_X1 && x_rel <= ESQ_PID_X2 &&
         y_rel >= ESQ_PID_Y1 && y_rel <= ESQ_PID_Y2);

    // --- 2) ¿Hemos tocado el bloque TRMS completo? ---
    bool hitTRMS =
        (x_rel >= ESQ_TRMS_X1 && x_rel <= ESQ_TRMS_X2 &&
         y_rel >= ESQ_TRMS_Y1 && y_rel <= ESQ_TRMS_Y2);

    if (!hitPID && !hitTRMS) {
        // Pulso fuera de las zonas interesantes
        return;
    }

    // Ocultar esquema general
    lv_obj_add_flag(ui_EsquemaGeneral, LV_OBJ_FLAG_HIDDEN);

    if (hitPID) {
        /*
        Serial.println("[GeneralDiagram] Seleccionado bloque PID completo");
        // Mostrar imagen del PID completo
        lv_obj_clear_flag(ui_EsquemaPIDCompleto, LV_OBJ_FLAG_HIDDEN);
        // Asegurarnos de ocultar la otra, por si estuviera visible
        lv_obj_add_flag(ui_EsquemaTRMSCompleto, LV_OBJ_FLAG_HIDDEN);
        */
       ShowPIDBlock(); 
    }
    else if (hitTRMS) {
        /*
        Serial.println("[GeneralDiagram] Seleccionado bloque TRMS completo");
        // Mostrar imagen del TRMS completo
        lv_obj_clear_flag(ui_EsquemaTRMSCompleto, LV_OBJ_FLAG_HIDDEN);
        // Asegurarnos de ocultar la otra, por si estuviera visible
        lv_obj_add_flag(ui_EsquemaPIDCompleto, LV_OBJ_FLAG_HIDDEN);
        */
       ShowTRMSBlock();
    }
}

// ===============================
// INICIALIZACIÓN PÚBLICA
// ===============================

void GeneralDiagram_Init(lv_obj_t * diagramImg)
{
    // Aseguramos que la imagen del esquema general es clicable
    lv_obj_add_flag(diagramImg, LV_OBJ_FLAG_CLICKABLE);

    // Registramos el callback para el evento RELEASED (tap)
    lv_obj_add_event_cb(diagramImg, GeneralDiagram_Event,
                        LV_EVENT_RELEASED, nullptr);

    Serial.println("[GeneralDiagram] Inicializado");
}

// ===============================
// API PÚBLICA PARA EL MANDO
// ===============================

void GeneralDiagram_SelectPID()
{
    ShowPIDBlock();
}

void GeneralDiagram_SelectTRMS()
{
    ShowTRMSBlock();
}
