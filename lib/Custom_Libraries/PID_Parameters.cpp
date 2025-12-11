/* PID_Parameters.cpp
   Definición e inicialización de los parámetros PID
*/

#include "PID_Parameters.h"
#include <stdio.h>
#include "ui.h"
#include <Preferences.h>

// NAMESPACE/NOMBRE en NVS para estos parámetros
static const char * PID_NVS_NAMESPACE = "pid_params";

// Prototipo helper interno
static void PID_SyncUIFromCurr();

// ==============================
// Valores iniciales (CURR)
// ==============================

PID_CURR g_pidCurr = {
    .KpvvCurr = 0.0f,
    .KpvhCurr = 0.0f,
    .KivvCurr = 0.0f,
    .KivhCurr = 0.0f,
    .KdvvCurr = 0.0f,
    .KdvhCurr = 0.0f,

    .KphvCurr = 0.0f,
    .KphhCurr = 0.0f,
    .KihvCurr = 0.0f,
    .KihhCurr = 0.0f,
    .KdhvCurr = 0.0f,
    .KdhhCurr = 0.0f,

    .IsatvvCurr = 0.0f,
    .IsatvhCurr = 0.0f,
    .IsathvCurr = 0.0f,
    .IsathhCurr = 0.0f,

    .UvmaxCurr = 0.0f,
    .UhmaxCurr = 0.0f
};


// ==============================
// Valores mínimos (MIN)
// ==============================

PID_MIN g_pidMin = {
    .KpvvMin = 0.0f,
    .KpvhMin = 0.0f,
    .KivvMin = 0.0f,
    .KivhMin = 0.0f,
    .KdvvMin = 0.0f,
    .KdvhMin = 0.0f,

    .KphvMin = 0.0f,
    .KphhMin = 0.0f,
    .KihvMin = 0.0f,
    .KihhMin = 0.0f,
    .KdhvMin = 0.0f,
    .KdhhMin = 0.0f,

    .IsatvvMin = 0.0f,
    .IsatvhMin = 0.0f,
    .IsathvMin = 0.0f,
    .IsathhMin = 0.0f,

    .UvmaxMin = 0.0f,
    .UhmaxMin = 0.0f
};


// ==============================
// Valores máximos (MAX)
// ==============================

PID_MAX g_pidMax = {
    .KpvvMax = 10.0f,
    .KpvhMax = 10.0f,
    .KivvMax = 10.0f,
    .KivhMax = 10.0f,
    .KdvvMax = 10.0f,
    .KdvhMax = 10.0f,

    .KphvMax = 10.0f,
    .KphhMax = 10.0f,
    .KihvMax = 10.0f,
    .KihhMax = 10.0f,
    .KdhvMax = 10.0f,
    .KdhhMax = 10.0f,

    .IsatvvMax = 5.0f,
    .IsatvhMax = 5.0f,
    .IsathvMax = 5.0f,
    .IsathhMax = 5.0f,

    .UvmaxMax = 3.3f,
    .UhmaxMax = 3.3f
};

// ------------------------------------------------------
// Función para establecer parámetros predeterminados
// ------------------------------------------------------
void PID_LoadDefaults() {

    //1) Establecer conjunto parámetros predeterminados
    g_pidCurr = {
        .KpvvCurr = 1.1f,
        .KpvhCurr = 0.0451f,
        .KivvCurr = 0.0f,
        .KivhCurr = 0.021f,
        .KdvvCurr = 0.0f,
        .KdvhCurr = 0.0f,

        .KphvCurr = 0.2321f,
        .KphhCurr = 2.196f,
        .KihvCurr = 0.1f,
        .KihhCurr = 1.394f,
        .KdhvCurr = 0.1f,
        .KdhhCurr = 1.98f,

        .IsatvvCurr = 1.3f,
        .IsatvhCurr = 1.0f,
        .IsathvCurr = 1.0f,
        .IsathhCurr = 1.87f,

        .UvmaxCurr = 1.0f,
        .UhmaxCurr = 1.0f
    };

    g_pidMin = {
        .KpvvMin = 0.0f,
        .KpvhMin = 0.0f,
        .KivvMin = 0.0f,
        .KivhMin = 0.0f,
        .KdvvMin = 0.0f,
        .KdvhMin = 0.0f,

        .KphvMin = 0.0f,
        .KphhMin = 0.0f,
        .KihvMin = 0.0f,
        .KihhMin = 0.0f,
        .KdhvMin = 0.0f,
        .KdhhMin = 0.0f,

        .IsatvvMin = 0.0f,
        .IsatvhMin = 0.0f,
        .IsathvMin = 0.0f,
        .IsathhMin = 0.0f,

        .UvmaxMin = 0.0f,
        .UhmaxMin = 0.0f
    };

    g_pidMax = {
        .KpvvMax = 10.0f,
        .KpvhMax = 10.0f,
        .KivvMax = 10.0f,
        .KivhMax = 10.0f,
        .KdvvMax = 10.0f,
        .KdvhMax = 10.0f,

        .KphvMax = 10.0f,
        .KphhMax = 10.0f,
        .KihvMax = 10.0f,
        .KihhMax = 10.0f,
        .KdhvMax = 10.0f,
        .KdhhMax = 10.0f,

        .IsatvvMax = 5.0f,
        .IsatvhMax = 5.0f,
        .IsathvMax = 5.0f,
        .IsathhMax = 5.0f,

        .UvmaxMax = 3.3f,
        .UhmaxMax = 3.3f
    };

    // 2) Sincronizar sliders + labels con los valores CURR/MIN/MAX
    PID_SyncUIFromCurr();

    Serial.println("PID parameters loaded with default values.");
}

void PID_UpdateParamLabel(lv_obj_t * label, float minVal, float currVal, float maxVal)
{
    if (!label) return;

    char buf[48];  // más que suficiente para "xx.xxx xx.xxx xx.xxx"

    // min  curr  max, separados por espacios
    snprintf(buf, sizeof(buf), "%.2f   %.2f   %.2f", minVal, currVal, maxVal);

    lv_label_set_text(label, buf);
}

// ======================================================
// Helper interno: aplicar g_pidCurr/g_pidMin/g_pidMax a la UI
// ======================================================
static void PID_SyncUIFromCurr()
{
    // -------- SLIDERS (0..100 → valor = param*10) --------
    lv_slider_set_value(ui_SliderKpvv,   (int)(g_pidCurr.KpvvCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKpvh,   (int)(g_pidCurr.KpvhCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKivv,   (int)(g_pidCurr.KivvCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKivh,   (int)(g_pidCurr.KivhCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKdvv,   (int)(g_pidCurr.KdvvCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKdvh,   (int)(g_pidCurr.KdvhCurr   * 10.0f), LV_ANIM_OFF);

    lv_slider_set_value(ui_SliderKphv,   (int)(g_pidCurr.KphvCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKphh,   (int)(g_pidCurr.KphhCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKihv,   (int)(g_pidCurr.KihvCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKihh,   (int)(g_pidCurr.KihhCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKdhv,   (int)(g_pidCurr.KdhvCurr   * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderKdhh,   (int)(g_pidCurr.KdhhCurr   * 10.0f), LV_ANIM_OFF);

    lv_slider_set_value(ui_SliderIsatvv, (int)(g_pidCurr.IsatvvCurr * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderIsatvh, (int)(g_pidCurr.IsatvhCurr * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderIsathv, (int)(g_pidCurr.IsathvCurr * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderIsathh, (int)(g_pidCurr.IsathhCurr * 10.0f), LV_ANIM_OFF);

    lv_slider_set_value(ui_SliderUvmax,  (int)(g_pidCurr.UvmaxCurr  * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderUhmax,  (int)(g_pidCurr.UhmaxCurr  * 10.0f), LV_ANIM_OFF);

    // -------- LABELS (min curr max) --------
    PID_UpdateParamLabel(ui_Kpvv,   g_pidMin.KpvvMin,   g_pidCurr.KpvvCurr,   g_pidMax.KpvvMax);
    PID_UpdateParamLabel(ui_Kpvh,   g_pidMin.KpvhMin,   g_pidCurr.KpvhCurr,   g_pidMax.KpvhMax);
    PID_UpdateParamLabel(ui_Kivv,   g_pidMin.KivvMin,   g_pidCurr.KivvCurr,   g_pidMax.KivvMax);
    PID_UpdateParamLabel(ui_Kivh,   g_pidMin.KivhMin,   g_pidCurr.KivhCurr,   g_pidMax.KivhMax);
    PID_UpdateParamLabel(ui_Kdvv,   g_pidMin.KdvvMin,   g_pidCurr.KdvvCurr,   g_pidMax.KdvvMax);
    PID_UpdateParamLabel(ui_Kdvh,   g_pidMin.KdvhMin,   g_pidCurr.KdvhCurr,   g_pidMax.KdvhMax);

    PID_UpdateParamLabel(ui_Kphv,   g_pidMin.KphvMin,   g_pidCurr.KphvCurr,   g_pidMax.KphvMax);
    PID_UpdateParamLabel(ui_Kphh,   g_pidMin.KphhMin,   g_pidCurr.KphhCurr,   g_pidMax.KphhMax);
    PID_UpdateParamLabel(ui_Kihv,   g_pidMin.KihvMin,   g_pidCurr.KihvCurr,   g_pidMax.KihvMax);
    PID_UpdateParamLabel(ui_Kihh,   g_pidMin.KihhMin,   g_pidCurr.KihhCurr,   g_pidMax.KihhMax);
    PID_UpdateParamLabel(ui_Kdhv,   g_pidMin.KdhvMin,   g_pidCurr.KdhvCurr,   g_pidMax.KdhvMax);
    PID_UpdateParamLabel(ui_Kdhh,   g_pidMin.KdhhMin,   g_pidCurr.KdhhCurr,   g_pidMax.KdhhMax);

    PID_UpdateParamLabel(ui_Isatvv, g_pidMin.IsatvvMin, g_pidCurr.IsatvvCurr, g_pidMax.IsatvvMax);
    PID_UpdateParamLabel(ui_Isatvh, g_pidMin.IsatvhMin, g_pidCurr.IsatvhCurr, g_pidMax.IsatvhMax);
    PID_UpdateParamLabel(ui_Isathv, g_pidMin.IsathvMin, g_pidCurr.IsathvCurr, g_pidMax.IsathvMax);
    PID_UpdateParamLabel(ui_Isathh, g_pidMin.IsathhMin, g_pidCurr.IsathhCurr, g_pidMax.IsathhMax);

    PID_UpdateParamLabel(ui_Uvmax,  g_pidMin.UvmaxMin,  g_pidCurr.UvmaxCurr,  g_pidMax.UvmaxMax);
    PID_UpdateParamLabel(ui_Uhmax,  g_pidMin.UhmaxMin,  g_pidCurr.UhmaxCurr,  g_pidMax.UhmaxMax);
}

// ======================================================
// NVS (Preferences): cargar / guardar PID
// ======================================================

void PID_SaveToNVS()
{
    Preferences prefs;
    if (!prefs.begin(PID_NVS_NAMESPACE, false)) {
        Serial.println("[PID] ERROR: no se pudo abrir NVS para escritura");
        return;
    }

    prefs.putBool("hasData", true);

    // --- CURR ---
    prefs.putFloat("KpvvCurr",   g_pidCurr.KpvvCurr);
    prefs.putFloat("KpvhCurr",   g_pidCurr.KpvhCurr);
    prefs.putFloat("KivvCurr",   g_pidCurr.KivvCurr);
    prefs.putFloat("KivhCurr",   g_pidCurr.KivhCurr);
    prefs.putFloat("KdvvCurr",   g_pidCurr.KdvvCurr);
    prefs.putFloat("KdvhCurr",   g_pidCurr.KdvhCurr);

    prefs.putFloat("KphvCurr",   g_pidCurr.KphvCurr);
    prefs.putFloat("KphhCurr",   g_pidCurr.KphhCurr);
    prefs.putFloat("KihvCurr",   g_pidCurr.KihvCurr);
    prefs.putFloat("KihhCurr",   g_pidCurr.KihhCurr);
    prefs.putFloat("KdhvCurr",   g_pidCurr.KdhvCurr);
    prefs.putFloat("KdhhCurr",   g_pidCurr.KdhhCurr);

    prefs.putFloat("IsatvvCurr", g_pidCurr.IsatvvCurr);
    prefs.putFloat("IsatvhCurr", g_pidCurr.IsatvhCurr);
    prefs.putFloat("IsathvCurr", g_pidCurr.IsathvCurr);
    prefs.putFloat("IsathhCurr", g_pidCurr.IsathhCurr);

    prefs.putFloat("UvmaxCurr",  g_pidCurr.UvmaxCurr);
    prefs.putFloat("UhmaxCurr",  g_pidCurr.UhmaxCurr);

    // --- MIN ---
    prefs.putFloat("KpvvMin",    g_pidMin.KpvvMin);
    prefs.putFloat("KpvhMin",    g_pidMin.KpvhMin);
    prefs.putFloat("KivvMin",    g_pidMin.KivvMin);
    prefs.putFloat("KivhMin",    g_pidMin.KivhMin);
    prefs.putFloat("KdvvMin",    g_pidMin.KdvvMin);
    prefs.putFloat("KdvhMin",    g_pidMin.KdvhMin);

    prefs.putFloat("KphvMin",    g_pidMin.KphvMin);
    prefs.putFloat("KphhMin",    g_pidMin.KphhMin);
    prefs.putFloat("KihvMin",    g_pidMin.KihvMin);
    prefs.putFloat("KihhMin",    g_pidMin.KihhMin);
    prefs.putFloat("KdhvMin",    g_pidMin.KdhvMin);
    prefs.putFloat("KdhhMin",    g_pidMin.KdhhMin);

    prefs.putFloat("IsatvvMin",  g_pidMin.IsatvvMin);
    prefs.putFloat("IsatvhMin",  g_pidMin.IsatvhMin);
    prefs.putFloat("IsathvMin",  g_pidMin.IsathvMin);
    prefs.putFloat("IsathhMin",  g_pidMin.IsathhMin);

    prefs.putFloat("UvmaxMin",   g_pidMin.UvmaxMin);
    prefs.putFloat("UhmaxMin",   g_pidMin.UhmaxMin);

    // --- MAX ---
    prefs.putFloat("KpvvMax",    g_pidMax.KpvvMax);
    prefs.putFloat("KpvhMax",    g_pidMax.KpvhMax);
    prefs.putFloat("KivvMax",    g_pidMax.KivvMax);
    prefs.putFloat("KivhMax",    g_pidMax.KivhMax);
    prefs.putFloat("KdvvMax",    g_pidMax.KdvvMax);
    prefs.putFloat("KdvhMax",    g_pidMax.KdvhMax);

    prefs.putFloat("KphvMax",    g_pidMax.KphvMax);
    prefs.putFloat("KphhMax",    g_pidMax.KphhMax);
    prefs.putFloat("KihvMax",    g_pidMax.KihvMax);
    prefs.putFloat("KihhMax",    g_pidMax.KihhMax);
    prefs.putFloat("KdhvMax",    g_pidMax.KdhvMax);
    prefs.putFloat("KdhhMax",    g_pidMax.KdhhMax);

    prefs.putFloat("IsatvvMax",  g_pidMax.IsatvvMax);
    prefs.putFloat("IsatvhMax",  g_pidMax.IsatvhMax);
    prefs.putFloat("IsathvMax",  g_pidMax.IsathvMax);
    prefs.putFloat("IsathhMax",  g_pidMax.IsathhMax);

    prefs.putFloat("UvmaxMax",   g_pidMax.UvmaxMax);
    prefs.putFloat("UhmaxMax",   g_pidMax.UhmaxMax);

    prefs.end();

    Serial.println("[PID] Configuración guardada en NVS");
}

void PID_LoadFromNVS()
{
    Preferences prefs;
    if (!prefs.begin(PID_NVS_NAMESPACE, true)) {
        Serial.println("[PID] ERROR: no se pudo abrir NVS para lectura");
        return;
    }

    bool hasData = prefs.getBool("hasData", false);
    if (!hasData) {
        // Primera vez: guardamos los valores actuales (defaults)
        prefs.end();
        PID_SaveToNVS();
        Serial.println("[PID] NVS vacío, guardando valores por defecto");
        return;
    }

    // --- CURR ---
    g_pidCurr.KpvvCurr   = prefs.getFloat("KpvvCurr",   g_pidCurr.KpvvCurr);
    g_pidCurr.KpvhCurr   = prefs.getFloat("KpvhCurr",   g_pidCurr.KpvhCurr);
    g_pidCurr.KivvCurr   = prefs.getFloat("KivvCurr",   g_pidCurr.KivvCurr);
    g_pidCurr.KivhCurr   = prefs.getFloat("KivhCurr",   g_pidCurr.KivhCurr);
    g_pidCurr.KdvvCurr   = prefs.getFloat("KdvvCurr",   g_pidCurr.KdvvCurr);
    g_pidCurr.KdvhCurr   = prefs.getFloat("KdvhCurr",   g_pidCurr.KdvhCurr);

    g_pidCurr.KphvCurr   = prefs.getFloat("KphvCurr",   g_pidCurr.KphvCurr);
    g_pidCurr.KphhCurr   = prefs.getFloat("KphhCurr",   g_pidCurr.KphhCurr);
    g_pidCurr.KihvCurr   = prefs.getFloat("KihvCurr",   g_pidCurr.KihvCurr);
    g_pidCurr.KihhCurr   = prefs.getFloat("KihhCurr",   g_pidCurr.KihhCurr);
    g_pidCurr.KdhvCurr   = prefs.getFloat("KdhvCurr",   g_pidCurr.KdhvCurr);
    g_pidCurr.KdhhCurr   = prefs.getFloat("KdhhCurr",   g_pidCurr.KdhhCurr);

    g_pidCurr.IsatvvCurr = prefs.getFloat("IsatvvCurr", g_pidCurr.IsatvvCurr);
    g_pidCurr.IsatvhCurr = prefs.getFloat("IsatvhCurr", g_pidCurr.IsatvhCurr);
    g_pidCurr.IsathvCurr = prefs.getFloat("IsathvCurr", g_pidCurr.IsathvCurr);
    g_pidCurr.IsathhCurr = prefs.getFloat("IsathhCurr", g_pidCurr.IsathhCurr);

    g_pidCurr.UvmaxCurr  = prefs.getFloat("UvmaxCurr",  g_pidCurr.UvmaxCurr);
    g_pidCurr.UhmaxCurr  = prefs.getFloat("UhmaxCurr",  g_pidCurr.UhmaxCurr);

    // --- MIN ---
    g_pidMin.KpvvMin     = prefs.getFloat("KpvvMin",    g_pidMin.KpvvMin);
    g_pidMin.KpvhMin     = prefs.getFloat("KpvhMin",    g_pidMin.KpvhMin);
    g_pidMin.KivvMin     = prefs.getFloat("KivvMin",    g_pidMin.KivvMin);
    g_pidMin.KivhMin     = prefs.getFloat("KivhMin",    g_pidMin.KivhMin);
    g_pidMin.KdvvMin     = prefs.getFloat("KdvvMin",    g_pidMin.KdvvMin);
    g_pidMin.KdhvMin     = prefs.getFloat("KdvhMin",    g_pidMin.KdvhMin);

    g_pidMin.KphvMin     = prefs.getFloat("KphvMin",    g_pidMin.KphvMin);
    g_pidMin.KphhMin     = prefs.getFloat("KphhMin",    g_pidMin.KphhMin);
    g_pidMin.KihvMin     = prefs.getFloat("KihvMin",    g_pidMin.KihvMin);
    g_pidMin.KihhMin     = prefs.getFloat("KihhMin",    g_pidMin.KihhMin);
    g_pidMin.KdhvMin     = prefs.getFloat("KdhvMin",    g_pidMin.KdhvMin);
    g_pidMin.KdhhMin     = prefs.getFloat("KdhhMin",    g_pidMin.KdhhMin);

    g_pidMin.IsatvvMin   = prefs.getFloat("IsatvvMin",  g_pidMin.IsatvvMin);
    g_pidMin.IsatvhMin   = prefs.getFloat("IsatvhMin",  g_pidMin.IsatvhMin);
    g_pidMin.IsathvMin   = prefs.getFloat("IsathvMin",  g_pidMin.IsathvMin);
    g_pidMin.IsathhMin   = prefs.getFloat("IsathhMin",  g_pidMin.IsathhMin);

    g_pidMin.UvmaxMin    = prefs.getFloat("UvmaxMin",   g_pidMin.UvmaxMin);
    g_pidMin.UhmaxMin    = prefs.getFloat("UhmaxMin",   g_pidMin.UhmaxMin);

    // --- MAX ---
    g_pidMax.KpvvMax     = prefs.getFloat("KpvvMax",    g_pidMax.KpvvMax);
    g_pidMax.KpvhMax     = prefs.getFloat("KpvhMax",    g_pidMax.KpvhMax);
    g_pidMax.KivvMax     = prefs.getFloat("KivvMax",    g_pidMax.KivvMax);
    g_pidMax.KivhMax     = prefs.getFloat("KivhMax",    g_pidMax.KivhMax);
    g_pidMax.KdvvMax     = prefs.getFloat("KdvvMax",    g_pidMax.KdvvMax);
    g_pidMax.KdvhMax     = prefs.getFloat("KdvhMax",    g_pidMax.KdvhMax);

    g_pidMax.KphvMax     = prefs.getFloat("KphvMax",    g_pidMax.KphvMax);
    g_pidMax.KphhMax     = prefs.getFloat("KphhMax",    g_pidMax.KphhMax);
    g_pidMax.KihvMax     = prefs.getFloat("KihvMax",    g_pidMax.KihvMax);
    g_pidMax.KihhMax     = prefs.getFloat("KihhMax",    g_pidMax.KihhMax);
    g_pidMax.KdhvMax     = prefs.getFloat("KdhvMax",    g_pidMax.KdhvMax);
    g_pidMax.KdhhMax     = prefs.getFloat("KdhhMax",    g_pidMax.KdhhMax);

    g_pidMax.IsatvvMax   = prefs.getFloat("IsatvvMax",  g_pidMax.IsatvvMax);
    g_pidMax.IsatvhMax   = prefs.getFloat("IsatvhMax",  g_pidMax.IsatvhMax);
    g_pidMax.IsathvMax   = prefs.getFloat("IsathvMax",  g_pidMax.IsathvMax);
    g_pidMax.IsathhMax   = prefs.getFloat("IsathhMax",  g_pidMax.IsathhMax);

    g_pidMax.UvmaxMax    = prefs.getFloat("UvmaxMax",   g_pidMax.UvmaxMax);
    g_pidMax.UhmaxMax    = prefs.getFloat("UhmaxMax",   g_pidMax.UhmaxMax);

    prefs.end();

    // Aplicar a la interfaz
    PID_SyncUIFromCurr();

    Serial.println("[PID] Configuración cargada desde NVS");
}
