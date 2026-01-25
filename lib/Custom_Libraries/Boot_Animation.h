#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#include "Montserrat_Medium_18_Latin.c"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bootanim_done_cb_t)(void);

typedef struct {
    const char* title_big;
    const char* title_small;

    const lv_img_dsc_t* logo_left_src;
    const lv_img_dsc_t* logo_right_src;

    bool logos_on_top; // si no lo usas aún, puedes dejarlo igual
} BootAnimConfig;

void BootAnim_Start(const BootAnimConfig* cfg, bootanim_done_cb_t cb_done);
void BootAnim_Stop(void);
bool BootAnim_IsRunning(void);
bool BootAnim_IsFinished(void);
void BootAnim_SetProgress(uint8_t percent);

#ifdef __cplusplus
}
#endif
