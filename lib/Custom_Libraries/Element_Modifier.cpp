#include "Element_Modifier.h"

/**
 * @brief 
 * Habilita la capacidad de hacer clic en las etiquetas de configuración
 * @note
  Esta función añade la bandera LV_OBJ_FLAG_CLICKABLE
  a varios objetos de etiqueta de configuración
  para permitir la interacción táctil.
  También oculta la etiqueta de mensaje de error
  y limpia el texto de la etiqueta de estado.
 */

void EnableConfigLabelsClickable() {
    lv_obj_add_flag(ui_Config1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config3, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config4, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_SaveEEPROM, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(ui_Label33, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label32, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label31, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label30, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label28, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(ui_Label92, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_Label36, " ");
}
