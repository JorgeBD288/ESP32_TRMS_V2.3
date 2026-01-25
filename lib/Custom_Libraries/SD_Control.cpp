// SD_Control.cpp
#include "SD_Control.h"
#include <SD.h>
#include <FS.h>
#include "PID_Parameters.h"
#include "ui.h"
#include <TFT_eSPI.h> // Esto arrastra User_Setup_Select.h -> User_Setup.h


// Estructuras globales declaradas en Parameters.cpp
extern PID_CURR g_pidCurr;
extern PID_MIN  g_pidMin;
extern PID_MAX  g_pidMax;

bool flag_Config_Message = false;
bool flag_Save_Message = false;

static const lv_color_t COLOR_RED = lv_color_make(255, 0, 0);
static const lv_color_t COLOR_GREEN = lv_color_make(0, 255, 0);

// Funciones auxiliares para la muestra de mensajes de error en caso de fallo

static lv_timer_t* s_tmrLabel92 = nullptr;

/**
 * @brief 
 * Oculta la label 92 tras un retardo.
 * @note
 * Callback del timer para ocultar
 * la label de mensaje tras 1 segundo. 
 * @param t 
 */

static void hide_label92_cb(lv_timer_t* t)
{
    lv_obj_t* label = (lv_obj_t*)t->user_data;
    if (!label) return;

    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);

    // Si quieres volver a verde:
    lv_obj_set_style_text_color(label, lv_color_make(0,255,0), LV_PART_MAIN);

    s_tmrLabel92 = nullptr;
    lv_timer_del(t);
}

/**
 * @brief 
 * Muestra un mensaje de error en la label 92.
 * @note
 * Actualiza la label ui_Label92
 * para mostrar un mensaje de error
 * en color rojo, y crea un timer
 * para ocultarla tras 1 segundo.
 */

static void show_error_label92(void)
{
    if (!ui_Label92) return;

    lv_label_set_text(ui_Label92, "Error al cargar configuracion");
    lv_obj_set_style_text_color(ui_Label92, lv_color_make(255,0,0), LV_PART_MAIN);
    lv_obj_clear_flag(ui_Label92, LV_OBJ_FLAG_HIDDEN);

    // Si ya había timer activo, lo borramos (evita acumulación)
    if (s_tmrLabel92) {
        lv_timer_del(s_tmrLabel92);
        s_tmrLabel92 = nullptr;
    }

    s_tmrLabel92 = lv_timer_create(hide_label92_cb, 1000, ui_Label92);

    // Forzar a que se llegue a dibujar ahora
    lv_timer_handler();
}


// ======================================================

/**
 * @brief 
 * Asegura que la tarjeta SD está montada.
 * @note
 * Deselecciona otros dispositivos SPI
 * antes de llamar a SD.begin().
 * Si SD.begin() falla, imprime un error por Serial. 
 * @return true 
 * @return false 
 */

static bool SD_EnsureMounted()
{
    // 1) Deseleccionar otros dispositivos SPI
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(TOUCH_CS, HIGH);

    // 2) Asegurar que la SD está montada
    if (!SD.begin(SD_CS)) {
        Serial.println("ERROR: SD.begin() ha fallado en SD_EnsureMounted");
        return false;
    }

    return true;
}

/**
 * @brief 
 * Actualiza una label con los valores PID.
 * @note
 * Muestra en la label el valor mínimo,
 * actual y máximo de un parámetro PID. 
 * @param label 
 * @param minVal 
 * @param currVal 
 * @param maxVal 
 */

// Declarada en Parameters.h (si no, añade allí el prototipo)
extern void PID_UpdateParamLabel(lv_obj_t * label,
                                 float      minVal,
                                 float      currVal,
                                 float      maxVal);

// ===============================================================
// Helper: aplicar g_pidCurr/g_pidMin/g_pidMax a sliders y labels
// ===============================================================

/**
 * @brief 
 * Aplica los valores de g_pidCurr
 * a los sliders y labels de la UI.
 * @note
 * Actualiza todos los sliders
 * para reflejar los valores actuales
 * de g_pidCurr (multiplicados por 10),
 * y actualiza las labels correspondientes
 * con min/curr/max.
 */

static void PID_ApplyCurrToUI()
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
// Guardar configuración en fichero
// ======================================================

/**
 * @brief 
 * Guarda los parámetros PID en un fichero.
 * @note
 * Asegura que la SD está montada,
 * abre el fichero en modo escritura,
 * y escribe los valores de g_pidCurr,
 * g_pidMin y g_pidMax en formato texto.
 * @param path 
 * Ruta del fichero en la SD.
 * @return true 
 * @return false 
 */

static bool PID_SaveConfigToFile(const char *path)
{   
    // Asegurar montaje SD
    if (!SD_EnsureMounted()) {
        Serial.println("No se puede guardar: SD no montada");

        //lv_obj_set_style_text_color(ui_Label36, lv_color_make(255,0,0), LV_PART_MAIN);
        //lv_label_set_text(ui_Label36, "Error de\nguardado");      

        return false;
    }

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.print("ERROR abriendo fichero: ");
        
        //lv_obj_set_style_text_color(ui_Label36, lv_color_make(255,0,0), LV_PART_MAIN);
        //lv_label_set_text(ui_Label36, "Error de\nguardado");

        Serial.println(path);
        return false;
    }

    
    //lv_obj_set_style_text_color(ui_Label36, lv_color_make(0,255,0), LV_PART_MAIN);
    //lv_label_set_text(ui_Label36, "Guardado\nexitoso");
    

    f.println("# CONFIGURACION PID -----------------------");

    // ---------- BLOQUE MIN ----------
    f.println("[PID_MIN]");
    f.print("KpvvMin=");   f.println(g_pidMin.KpvvMin,   6);
    f.print("KpvhMin=");   f.println(g_pidMin.KpvhMin,   6);
    f.print("KivvMin=");   f.println(g_pidMin.KivvMin,   6);
    f.print("KivhMin=");   f.println(g_pidMin.KivhMin,   6);
    f.print("KdvvMin=");   f.println(g_pidMin.KdvvMin,   6);
    f.print("KdvhMin=");   f.println(g_pidMin.KdvhMin,   6);
    f.print("KphvMin=");   f.println(g_pidMin.KphvMin,   6);
    f.print("KphhMin=");   f.println(g_pidMin.KphhMin,   6);
    f.print("KihvMin=");   f.println(g_pidMin.KihvMin,   6);
    f.print("KihhMin=");   f.println(g_pidMin.KihhMin,   6);
    f.print("KdhvMin=");   f.println(g_pidMin.KdhvMin,   6);
    f.print("KdhhMin=");   f.println(g_pidMin.KdhhMin,   6);
    f.print("IsatvvMin="); f.println(g_pidMin.IsatvvMin, 6);
    f.print("IsatvhMin="); f.println(g_pidMin.IsatvhMin, 6);
    f.print("IsathvMin="); f.println(g_pidMin.IsathvMin, 6);
    f.print("IsathhMin="); f.println(g_pidMin.IsathhMin, 6);
    f.print("UvmaxMin=");  f.println(g_pidMin.UvmaxMin,  6);
    f.print("UhmaxMin=");  f.println(g_pidMin.UhmaxMin,  6);
    f.println();

    // ---------- BLOQUE CURR ----------
    f.println("[PID_CURR]");
    f.print("KpvvCurr=");   f.println(g_pidCurr.KpvvCurr,   6);
    f.print("KpvhCurr=");   f.println(g_pidCurr.KpvhCurr,   6);
    f.print("KivvCurr=");   f.println(g_pidCurr.KivvCurr,   6);
    f.print("KivhCurr=");   f.println(g_pidCurr.KivhCurr,   6);
    f.print("KdvvCurr=");   f.println(g_pidCurr.KdvvCurr,   6);
    f.print("KdvhCurr=");   f.println(g_pidCurr.KdvhCurr,   6);
    f.print("KphvCurr=");   f.println(g_pidCurr.KphvCurr,   6);
    f.print("KphhCurr=");   f.println(g_pidCurr.KphhCurr,   6);
    f.print("KihvCurr=");   f.println(g_pidCurr.KihvCurr,   6);
    f.print("KihhCurr=");   f.println(g_pidCurr.KihhCurr,   6);
    f.print("KdhvCurr=");   f.println(g_pidCurr.KdhvCurr,   6);
    f.print("KdhhCurr=");   f.println(g_pidCurr.KdhhCurr,   6);
    f.print("IsatvvCurr="); f.println(g_pidCurr.IsatvvCurr, 6);
    f.print("IsatvhCurr="); f.println(g_pidCurr.IsatvhCurr, 6);
    f.print("IsathvCurr="); f.println(g_pidCurr.IsathvCurr, 6);
    f.print("IsathhCurr="); f.println(g_pidCurr.IsathhCurr, 6);
    f.print("UvmaxCurr=");  f.println(g_pidCurr.UvmaxCurr,  6);
    f.print("UhmaxCurr=");  f.println(g_pidCurr.UhmaxCurr,  6);
    f.println();

    // ---------- BLOQUE MAX ----------
    f.println("[PID_MAX]");
    f.print("KpvvMax=");   f.println(g_pidMax.KpvvMax,   6);
    f.print("KpvhMax=");   f.println(g_pidMax.KpvhMax,   6);
    f.print("KivvMax=");   f.println(g_pidMax.KivvMax,   6);
    f.print("KivhMax=");   f.println(g_pidMax.KivhMax,   6);
    f.print("KdvvMax=");   f.println(g_pidMax.KdvvMax,   6);
    f.print("KdvhMax=");   f.println(g_pidMax.KdvhMax,   6);
    f.print("KphvMax=");   f.println(g_pidMax.KphvMax,   6);
    f.print("KphhMax=");   f.println(g_pidMax.KphhMax,   6);
    f.print("KihvMax=");   f.println(g_pidMax.KihvMax,   6);
    f.print("KihhMax=");   f.println(g_pidMax.KihhMax,   6);
    f.print("KdhvMax=");   f.println(g_pidMax.KdhvMax,   6);
    f.print("KdhhMax=");   f.println(g_pidMax.KdhhMax,   6);
    f.print("IsatvvMax="); f.println(g_pidMax.IsatvvMax, 6);
    f.print("IsatvhMax="); f.println(g_pidMax.IsatvhMax, 6);
    f.print("IsathvMax="); f.println(g_pidMax.IsathvMax, 6);
    f.print("IsathhMax="); f.println(g_pidMax.IsathhMax, 6);
    f.print("UvmaxMax=");  f.println(g_pidMax.UvmaxMax,  6);
    f.print("UhmaxMax=");  f.println(g_pidMax.UhmaxMax,  6);
    f.println("-------------------------------------------");

    f.close();

    Serial.print("Configuracion PID guardada en: ");
    Serial.println(path);
    return true;
}

// ======================================================
// Cargar configuración desde fichero
// ======================================================

enum PIDSection {
    PID_SECTION_NONE = 0,
    PID_SECTION_MIN,
    PID_SECTION_CURR,
    PID_SECTION_MAX
};

/**
 * @brief 
 * Carga los parámetros PID desde un fichero.
 * @note
 * Asegura que la SD está montada,
 * abre el fichero en modo lectura,
 * y lee los valores para g_pidCurr,
 * g_pidMin y g_pidMax.
 * @param path 
 * Ruta del fichero en la SD.
 * @return true 
 * @return false 
 */

static bool PID_LoadConfigFromFile(const char *path)
{   
    // Asegurar montaje SD
    if (!SD_EnsureMounted()) {
        Serial.println("No se puede guardar: SD no montada");
        show_error_label92();
        return false;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
        Serial.print("ERROR abriendo fichero de config: ");
        Serial.println(path);
        show_error_label92();
        return false;
    }

    Serial.print("Leyendo config PID de: ");
    Serial.println(path);

    PIDSection section = PID_SECTION_NONE;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        if (line.startsWith("#")) continue;

        // Sección
        if (line.startsWith("[")) {
            if      (line == "[PID_MIN]")   section = PID_SECTION_MIN;
            else if (line == "[PID_CURR]")  section = PID_SECTION_CURR;
            else if (line == "[PID_MAX]")   section = PID_SECTION_MAX;
            else                            section = PID_SECTION_NONE;
            continue;
        }

        int eqPos = line.indexOf('=');
        if (eqPos < 0) continue;

        String key = line.substring(0, eqPos);
        String val = line.substring(eqPos + 1);
        key.trim();
        val.trim();

        float v = val.toFloat();

        if (section == PID_SECTION_MIN) {
            if      (key == "KpvvMin")   g_pidMin.KpvvMin   = v;
            else if (key == "KpvhMin")   g_pidMin.KpvhMin   = v;
            else if (key == "KivvMin")   g_pidMin.KivvMin   = v;
            else if (key == "KivhMin")   g_pidMin.KivhMin   = v;
            else if (key == "KdvvMin")   g_pidMin.KdvvMin   = v;
            else if (key == "KdvhMin")   g_pidMin.KdvhMin   = v;

            else if (key == "KphvMin")   g_pidMin.KphvMin   = v;
            else if (key == "KphhMin")   g_pidMin.KphhMin   = v;
            else if (key == "KihvMin")   g_pidMin.KihvMin   = v;
            else if (key == "KihhMin")   g_pidMin.KihhMin   = v;
            else if (key == "KdhvMin")   g_pidMin.KdhvMin   = v;
            else if (key == "KdhhMin")   g_pidMin.KdhhMin   = v;

            else if (key == "IsatvvMin") g_pidMin.IsatvvMin = v;
            else if (key == "IsatvhMin") g_pidMin.IsatvhMin = v;
            else if (key == "IsathvMin") g_pidMin.IsathvMin = v;
            else if (key == "IsathhMin") g_pidMin.IsathhMin = v;

            else if (key == "UvmaxMin")  g_pidMin.UvmaxMin  = v;
            else if (key == "UhmaxMin")  g_pidMin.UhmaxMin  = v;
        }
        else if (section == PID_SECTION_CURR) {
            if      (key == "KpvvCurr")   g_pidCurr.KpvvCurr   = v;
            else if (key == "KpvhCurr")   g_pidCurr.KpvhCurr   = v;
            else if (key == "KivvCurr")   g_pidCurr.KivvCurr   = v;
            else if (key == "KivhCurr")   g_pidCurr.KivhCurr   = v;
            else if (key == "KdvvCurr")   g_pidCurr.KdvvCurr   = v;
            else if (key == "KdvhCurr")   g_pidCurr.KdvhCurr   = v;

            else if (key == "KphvCurr")   g_pidCurr.KphvCurr   = v;
            else if (key == "KphhCurr")   g_pidCurr.KphhCurr   = v;
            else if (key == "KihvCurr")   g_pidCurr.KihvCurr   = v;
            else if (key == "KihhCurr")   g_pidCurr.KihhCurr   = v;
            else if (key == "KdhvCurr")   g_pidCurr.KdhvCurr   = v;
            else if (key == "KdhhCurr")   g_pidCurr.KdhhCurr   = v;

            else if (key == "IsatvvCurr") g_pidCurr.IsatvvCurr = v;
            else if (key == "IsatvhCurr") g_pidCurr.IsatvhCurr = v;
            else if (key == "IsathvCurr") g_pidCurr.IsathvCurr = v;
            else if (key == "IsathhCurr") g_pidCurr.IsathhCurr = v;

            else if (key == "UvmaxCurr")  g_pidCurr.UvmaxCurr  = v;
            else if (key == "UhmaxCurr")  g_pidCurr.UhmaxCurr  = v;
        }
        else if (section == PID_SECTION_MAX) {
            if      (key == "KpvvMax")   g_pidMax.KpvvMax   = v;
            else if (key == "KpvhMax")   g_pidMax.KpvhMax   = v;
            else if (key == "KivvMax")   g_pidMax.KivvMax   = v;
            else if (key == "KivhMax")   g_pidMax.KivhMax   = v;
            else if (key == "KdvvMax")   g_pidMax.KdvvMax   = v;
            else if (key == "KdvhMax")   g_pidMax.KdvhMax   = v;

            else if (key == "KphvMax")   g_pidMax.KphvMax   = v;
            else if (key == "KphhMax")   g_pidMax.KphhMax   = v;
            else if (key == "KihvMax")   g_pidMax.KihvMax   = v;
            else if (key == "KihhMax")   g_pidMax.KihhMax   = v;
            else if (key == "KdhvMax")   g_pidMax.KdhvMax   = v;
            else if (key == "KdhhMax")   g_pidMax.KdhhMax   = v;

            else if (key == "IsatvvMax") g_pidMax.IsatvvMax = v;
            else if (key == "IsatvhMax") g_pidMax.IsatvhMax = v;
            else if (key == "IsathvMax") g_pidMax.IsathvMax = v;
            else if (key == "IsathhMax") g_pidMax.IsathhMax = v;

            else if (key == "UvmaxMax")  g_pidMax.UvmaxMax  = v;
            else if (key == "UhmaxMax")  g_pidMax.UhmaxMax  = v;
        }
    }

    f.close();
    Serial.println("Configuracion PID cargada correctamente.");
    return true;
}

// ======================================================
// API pública: guardar/cargar por índice (1..N)
// ======================================================

/**
 * @brief 
 * Construye la ruta del fichero de configuración
 * para un índice dado.
 * @note
 * La ruta tiene el formato:
 * "/Config_PID/Config_<index>.txt"
 * @param index 
 * Índice de configuración (1..5).
 * @return String 
 */

static String buildConfigPath(uint8_t index)
{
    // Index 1..5 → "/Config_PID/Config_1.txt" ...
    String path = "/Config_PID/Config_";
    path += String(index);
    path += ".txt";
    return path;
}

/**
 * @brief 
 * Guarda la configuración PID en la SD.
 * @note
 * Construye la ruta del fichero
 * según el índice dado,
 * y llama a PID_SaveConfigToFile().
 * @param index 
 * Índice de configuración (1..5).
 * @return true 
 * @return false 
 */

bool ControlSD_SaveConfig(uint8_t index)
{
    // Asegurar que la SD está montada
    if (!SD_EnsureMounted()) {
        Serial.println("No se puede guardar: SD no montada");
        return false;
    }

    String path = buildConfigPath(index);  // → "/Config_PID/Config_1.txt", etc.
    Serial.print("Guardando config en: ");
    Serial.println(path);

    return PID_SaveConfigToFile(path.c_str());
}

/**
 * @brief 
 * Carga la configuración PID desde la SD.
 * @note
 * Construye la ruta del fichero
 * según el índice dado,
 * y llama a PID_LoadConfigFromFile().
 * Tras cargar, actualiza los sliders
 * y muestra la label de éxito.
 * @param index 
 * Índice de configuración (1..5).
 * @return true 
 * @return false 
 */

bool ControlSD_LoadConfig(uint8_t index)
{
    // Asegurar que la SD está montada
    if (!SD_EnsureMounted()) {
        Serial.println("No se puede cargar: SD no montada");
        return false;
    }

    String path = buildConfigPath(index);  // → "/Config_PID/Config_1.txt", etc.
    Serial.print("Leyendo config de: ");
    Serial.println(path);

    if (!PID_LoadConfigFromFile(path.c_str())) {
        return false;
    }

    // Tras cargar, actualizar sliders + labels de UI
    PID_ApplyCurrToUI();
    lv_obj_clear_flag(ui_Label92, LV_OBJ_FLAG_HIDDEN);
    flag_Config_Message = true;
    return true;
}
