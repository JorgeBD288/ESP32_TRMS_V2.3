/*Este archivo, así como su correspondiente ".cpp", se utiliza pata declarar las funciones que se encargan de establecer la 
interfaz gráfica de la cuadrícula con la que seleccionar el ángulo vertical y horizontal, que servirán como entrada para realizar posteriormente, 
el control PID*/

#pragma once

#include <lvgl.h>

// gridImage  → imagen de la cuadrícula (ej: ui_Image12)
// labelH     → label ángulo horizontal  (ej: ui_Label66)
// labelV     → label ángulo vertical    (ej: ui_Label7)
void AngSelect_Init(lv_obj_t * gridImage,
                    lv_obj_t * labelH,
                    lv_obj_t * labelV);

//Referencias (en grados)
float AngSelect_GetRefHorizontal();   // grados
float AngSelect_GetRefVertical();     // grados

// Setters para controlar los ángulos desde el mando IR
void AngSelect_SetRefHorizontal(float angle);
void AngSelect_SetRefVertical(float angle);

// Aplicar ambos ángulos y actualizar gráficos
void AngSelect_ApplyRefFromAngles(float angle_h, float angle_v);

// Funciones de RESET
// Ambas referencias a 0, labels a 0° y punto/líneas en el centro
void AngSelect_ResetRefBoth();
// O solo un eje:
void AngSelect_ResetRefHorizontal();
void AngSelect_ResetRefVertical();