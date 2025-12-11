#pragma once

#include <lvgl.h>
#include "IRControl.h"

// Botones invisibles colocados sobre la imagen del mando
extern lv_obj_t * ui_RemoteBtn1;
extern lv_obj_t * ui_RemoteBtn2;
extern lv_obj_t * ui_RemoteBtn3;
extern lv_obj_t * ui_RemoteBtn4;
extern lv_obj_t * ui_RemoteBtn5;
extern lv_obj_t * ui_RemoteBtn6;
extern lv_obj_t * ui_RemoteBtn7;
extern lv_obj_t * ui_RemoteBtn8;
extern lv_obj_t * ui_RemoteBtn9;
extern lv_obj_t * ui_RemoteBtn0;

extern lv_obj_t * ui_RemoteBtnEnter;
extern lv_obj_t * ui_RemoteBtnUp;
extern lv_obj_t * ui_RemoteBtnDown;
extern lv_obj_t * ui_RemoteBtnLeft;
extern lv_obj_t * ui_RemoteBtnRight;

extern lv_obj_t * ui_RemoteBtnPlus;
extern lv_obj_t * ui_RemoteBtnMinus;
extern lv_obj_t * ui_RemoteBtnPower;

/**
 * @brief Inicializa las zonas clicables sobre la imagen del mando.
 *
 * @param remoteImg lv_img_t* de la imagen del mando (por ejemplo ui_ImageMandoIR).
 *
 * Crea botones hijos de esa imagen, totalmente transparentes, posicionados
 * en las coordenadas que has pasado (círculos para números/power y
 * rectángulos para flechas, ENTER, +, −).
 */
void RemoteDiagram_Init(lv_obj_t * remoteImg);
bool RemoteDiagram_HandleIRLearn(const IRControlEvent &ev);