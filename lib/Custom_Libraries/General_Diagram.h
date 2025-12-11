/*Este archivo, y su correspondiente ".cpp", se utiliza para gestionar
  la interacción con el esquema general en Screen5. Permite que, al
  pulsar sobre determinadas zonas del diagrama, se cambie la imagen
  mostrada (PID completo, TRMS completo, etc.). */

#pragma once

#include <lvgl.h>
#include "ui.h"

// Declaración de los botones invisibles para navegación con mando
extern lv_obj_t * ui_BtnPIDZone;
extern lv_obj_t * ui_BtnTRMSZone;

// Coordenadas de las zonas clicables
extern const int ESQ_PID_X1;
extern const int ESQ_PID_X2;
extern const int ESQ_PID_Y1;
extern const int ESQ_PID_Y2;

extern const int ESQ_TRMS_X1;
extern const int ESQ_TRMS_X2;
extern const int ESQ_TRMS_Y1;
extern const int ESQ_TRMS_Y2;

// diagramImg → objeto imagen del esquema general (ej: ui_EsquemaGeneral)
void GeneralDiagram_Init(lv_obj_t * diagramImg);
void GeneralDiagram_SelectPID();
void GeneralDiagram_SelectTRMS();
