#include "Element_Modifier.h"


void EnableConfigLabelsClickable() {
    lv_obj_add_flag(ui_Config1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config3, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config4, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Config5, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(ui_Label33, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label32, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label31, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label30, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label28, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(ui_Label92, LV_OBJ_FLAG_HIDDEN);
}
