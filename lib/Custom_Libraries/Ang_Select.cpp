#include "Ang_Select.h"
#include <Arduino.h>

// ======================================================
// CONFIGURACIÓN DE LA CUADRÍCULA
// ======================================================

// Esquina superior izquierda de la cuadrícula DENTRO de la imagen
// (los valores que mediste: ~62.5, 5.74 → redondeamos)
static const int GRID_X0 = 62;   // px
static const int GRID_Y0 = 6;    // px

// Tamaño de cada cuadrado
static const int CELL_W   = 20;  // px (ancho)
static const int CELL_H   = 20;  // px (alto)

// Número de celdas (de -180 a +180 en pasos de 30º → 360/30 = 12)
static const int GRID_COLS = 12;
static const int GRID_ROWS = 12;

static const int GRID_W = GRID_COLS * CELL_W;
static const int GRID_H = GRID_ROWS * CELL_H;

// 30º por cuadrado
static const float DEG_PER_CELL = 30.0f;
static const float DEG_PER_PX_X = DEG_PER_CELL / CELL_W;  // 30 / 15  = 2.0 º/px
static const float DEG_PER_PX_Y = DEG_PER_CELL / CELL_H;  // 30 / 16 ≈ 1.875 º/px

// Ejes gráficos dentro de la imagen (en coordenadas locales)
static const int AXIS_X = 62;   // x del eje vertical
static const int AXIS_Y = 248;  // y del eje horizontal

// Ángulos asignados como referencia para el cálculo de la consigna del PID
static float g_refH = 0.0f;   // referencia horizontal (grados)
static float g_refV = 0.0f;   // referencia vertical (grados)

// ======================================================
// OBJETOS LVGL A ACTUALIZAR
// ======================================================

static lv_obj_t * g_labelH = nullptr;   // A. horizontal
static lv_obj_t * g_labelV = nullptr;   // A. vertical
static lv_obj_t * g_gridImage = nullptr;   // Puntero a la imagen de la cuadrícula


// Objetos gráficos para marcar el punto y las líneas
static lv_obj_t * g_marker = nullptr;  // punto rojo
static lv_obj_t * g_lineH  = nullptr;  // línea horizontal verde
static lv_obj_t * g_lineV  = nullptr;  // línea vertical azul



/**
 * @brief 
 * @param img
 * @param local_x
 * @param local_y
 * @return void
 * @note
 * Calcula y dibuja/mueve el punto rojo (marcador) y las líneas verde (horizontal)
 * y azul (vertical) en la imagen de la cuadrícula, basándose en las
 * coordenadas locales del toque dentro de la imagen.
 * El punto rojo se coloca en la posición del toque,
 * la línea verde se extiende horizontalmente desde el eje vertical
 * hasta el punto, y la línea azul se extiende verticalmente
 * desde el eje horizontal hasta el punto.
*/

// Dibuja/mueve el punto rojo y las líneas verde/azul
// img      → imagen de la cuadrícula (ui_Image12)
// local_x  → x del toque en coordenadas locales de la imagen
// local_y  → y del toque en coordenadas locales de la imagen

static void AngSelect_UpdateGraphics(lv_obj_t * img, int local_x, int local_y)
{
    // Crear los objetos la primera vez
    if (!g_marker) {
        g_marker = lv_obj_create(img);
        lv_obj_set_size(g_marker, 6, 6);
        lv_obj_set_style_bg_color(g_marker, lv_color_hex(0xFF0000), 0); // rojo
        lv_obj_set_style_bg_opa(g_marker, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_marker, 0, 0);
    }

    if (!g_lineH) {
        g_lineH = lv_obj_create(img);
        lv_obj_set_style_bg_color(g_lineH, lv_color_hex(0x00FF00), 0); // verde
        lv_obj_set_style_bg_opa(g_lineH, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_lineH, 0, 0);
    }

    if (!g_lineV) {
        g_lineV = lv_obj_create(img);
        lv_obj_set_style_bg_color(g_lineV, lv_color_hex(0x0000FF), 0); // azul
        lv_obj_set_style_bg_opa(g_lineV, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_lineV, 0, 0);
    }

    // Asegurarnos de que se ven por encima de la imagen
    lv_obj_move_foreground(g_marker);
    lv_obj_move_foreground(g_lineH);
    lv_obj_move_foreground(g_lineV);

    // ------------------------------
    // 1) Punto rojo (marcador)
    // ------------------------------
    const int marker_size = 6;
    lv_obj_set_pos(g_marker, local_x - marker_size / 2, local_y - marker_size / 2);

    // ------------------------------
    // 2) Línea horizontal (verde)
    //    desde el eje vertical gráfico (x = AXIS_X)
    //    hasta el punto (local_x, local_y)
    // ------------------------------
    int x1_h = (local_x < AXIS_X) ? local_x : AXIS_X;
    int x2_h = (local_x > AXIS_X) ? local_x : AXIS_X;
    int w_h  = x2_h - x1_h;
    if (w_h < 0) w_h = 0;

    lv_obj_set_pos(g_lineH, x1_h, local_y - 1);  // un pelín centrada
    lv_obj_set_size(g_lineH, w_h, 3);            // grosor 3 px

    if (w_h == 0) {
        lv_obj_add_flag(g_lineH, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(g_lineH, LV_OBJ_FLAG_HIDDEN);
    }

    // ------------------------------
    // 3) Línea vertical (azul)
    //    desde el eje horizontal gráfico (y = AXIS_Y)
    //    hasta el punto (local_x, local_y)
    // ------------------------------
    int y1_v = (local_y < AXIS_Y) ? local_y : AXIS_Y;
    int y2_v = (local_y > AXIS_Y) ? local_y : AXIS_Y;
    int h_v  = y2_v - y1_v;
    if (h_v < 0) h_v = 0;

    lv_obj_set_pos(g_lineV, local_x - 1, y1_v);
    lv_obj_set_size(g_lineV, 3, h_v);            // grosor 3 px

    if (h_v == 0) {
        lv_obj_add_flag(g_lineV, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(g_lineV, LV_OBJ_FLAG_HIDDEN);
    }
}



// ======================================================
// CALLBACK DE EVENTO
// ======================================================


/**
 * @brief 
 * @param img
 * @param local_x
 * @param local_y
 * @return void
 * @note
 * Maneja los eventos táctiles en la imagen de la cuadrícula.
 * Calcula los ángulos horizontales y verticales
 * basándose en la posición del toque,
 * actualiza los gráficos (punto y líneas)
 * y muestra los ángulos en las etiquetas correspondientes.
 * Además, guarda los ángulos como referencias globales.
 * @param e 
 */

static void AngSelect_Event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * img = lv_event_get_target(e);

    // Para depuración básica: ver si entra
    if (code == LV_EVENT_PRESSED) {
        Serial.println("[AngSelect] LV_EVENT_PRESSED");
    } else if (code == LV_EVENT_PRESSING) {
        Serial.println("[AngSelect] LV_EVENT_PRESSING");
    } else if (code == LV_EVENT_RELEASED) {
        Serial.println("[AngSelect] LV_EVENT_RELEASED");
    } else {
        return;
    }

    // Solo hacemos el cálculo cuando está presionado o arrastrando
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) {
        return;
    }

    // 1) Posición del toque en coordenadas de pantalla
    lv_point_t p;
    lv_indev_t * indev = lv_indev_get_act();
    if (!indev) {
        Serial.println("[AngSelect] indev nullptr");
        return;
    }

    lv_indev_get_point(indev, &p);
    Serial.print("[AngSelect] Touch screen coords: x=");
    Serial.print(p.x);
    Serial.print(" y=");
    Serial.println(p.y);

    // 2) Coordenadas reales de la imagen en pantalla
    lv_area_t img_area;
    lv_obj_get_coords(img, &img_area);  // x1,y1,x2,y2 en coordenadas de pantalla

    // 3) Coordenadas locales dentro de la imagen
    int local_x = p.x - img_area.x1;
    int local_y = p.y - img_area.y1;

    Serial.print("[AngSelect] Local in image: x=");
    Serial.print(local_x);
    Serial.print(" y=");
    Serial.println(local_y);

    // 4) Coordenadas relativas a la cuadrícula
    int grid_x = local_x - GRID_X0;
    int grid_y = local_y - GRID_Y0;

    Serial.print("[AngSelect] In grid: x=");
    Serial.print(grid_x);
    Serial.print(" y=");
    Serial.println(grid_y);

    // 5) Si tocamos fuera de la cuadrícula, no hay nada que hacer
    if (grid_x < 0 || grid_x >= GRID_W || grid_y < 0 || grid_y >= GRID_H) {
        Serial.println("[AngSelect] Fuera de la cuadrícula");
        return;
    }

    // 6) ACTUALIZAR GRAFICOS: punto rojo + líneas
    AngSelect_UpdateGraphics(img, local_x, local_y);

    // 7) Centro de la cuadrícula en píxeles (origen angular 0,0)
    float cx = GRID_W / 2.0f;
    float cy = GRID_H / 2.0f;

    // 8) Desplazamiento relativo al centro (en píxeles)
    float dx = (float)grid_x - cx;   // + derecha
    float dy = cy - (float)grid_y;   // + arriba (invertimos Y)

    // 9) Conversión a grados
    float angle_h = dx * DEG_PER_PX_X;  // eje horizontal
    float angle_v = dy * DEG_PER_PX_Y;  // eje vertical

    Serial.print("[AngSelect] Angles: H=");
    Serial.print(angle_h);
    Serial.print("  V=");
    Serial.println(angle_v);

    // 10) Mostrar en labels
    if (g_labelH) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A. horizontal = %.1f°", angle_h);
        lv_label_set_text(g_labelH, buf);
    }

    if (g_labelV) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A. vertical = %.1f°", angle_v);
        lv_label_set_text(g_labelV, buf);
    }
    
    // 11) Guardar como últimas referencias (en grados)
    g_refH = angle_h;
    g_refV = angle_v;

}

// ======================================================
// INICIALIZACIÓN PÚBLICA
// ======================================================

/**
 * @brief 
 * @note
 * Inicializa el selector de ángulos táctil.
 * Configura la imagen de la cuadrícula y las etiquetas
 * para mostrar los ángulos horizontal y vertical.
 * Además, registra los callbacks de eventos táctiles
 * para manejar la interacción del usuario.
 * @param gridImage 
 * @param labelH 
 * @param labelV 
 */

void AngSelect_Init(lv_obj_t * gridImage,
                    lv_obj_t * labelH,
                    lv_obj_t * labelV)
{
    g_labelH = labelH;
    g_labelV = labelV;
    g_gridImage = gridImage;

    // Aseguramos que la imagen es "clicable"
    lv_obj_add_flag(gridImage, LV_OBJ_FLAG_CLICKABLE);

    // Registramos el callback para varios eventos táctiles
    lv_obj_add_event_cb(gridImage, AngSelect_Event, LV_EVENT_PRESSED,   nullptr);
    lv_obj_add_event_cb(gridImage, AngSelect_Event, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(gridImage, AngSelect_Event, LV_EVENT_RELEASED, nullptr);

    Serial.println("[AngSelect] Inicializado");
}

/**
 * @brief 
 * @return float
 * @note
 * Obtiene el ángulo de referencia horizontal actual en grados.
 */

float AngSelect_GetRefHorizontal()
{
    return g_refH;
}

/**
 * @brief 
 * @return float
 * @note
 * Obtiene el ángulo de referencia vertical actual en grados.
 */

float AngSelect_GetRefVertical()
{
    return g_refV;
}

/**
 * @brief
 * @note 
 * Establece el ángulo de referencia horizontal
 * y actualiza la etiqueta correspondiente.
 * @param angle 
 */

void AngSelect_SetRefHorizontal(float angle)
{
    g_refH = angle;

    if (g_labelH) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A. horizontal = %.1f°", g_refH);
        lv_label_set_text(g_labelH, buf);
    }
}

/**
 * @brief 
 * @note
 * Establece el ángulo de referencia vertical
 * y actualiza la etiqueta correspondiente.
 * @param angle 
 */

void AngSelect_SetRefVertical(float angle)
{
    g_refV = angle;

    if (g_labelV) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A. vertical = %.1f°", g_refV);
        lv_label_set_text(g_labelV, buf);
    }
}

/**
 * @brief 
 * @note
 * Esta función ajusta las referencias de ángulo horizontal
 * y vertical a los valores proporcionados,
 * asegurándose de que estén dentro del rango [-180, 180] grados.
 * Luego, actualiza las etiquetas de la interfaz
 * para mostrar los nuevos ángulos de referencia
 * y ajusta los gráficos en la imagen de la cuadrícula
 * para reflejar las nuevas referencias.
 * @param angle_h 
 * @param angle_v 
 */

void AngSelect_ApplyRefFromAngles(float angle_h, float angle_v)
{
    // Limitar a [-180, 180]
    angle_h = constrain(angle_h, -180.0f, 180.0f);
    angle_v = constrain(angle_v, -180.0f, 180.0f);

    // Guardar referencias
    g_refH = angle_h;
    g_refV = angle_v;

    // Actualizar labels
    if (g_labelH) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A. horizontal = %.1f°", g_refH);
        lv_label_set_text(g_labelH, buf);
    }
    if (g_labelV) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A. vertical = %.1f°", g_refV);
        lv_label_set_text(g_labelV, buf);
    }

    // Actualizar gráficos (punto rojo + líneas)
    if (!g_gridImage) return;

    // Centro de la cuadrícula
    float cx = GRID_W / 2.0f;
    float cy = GRID_H / 2.0f;

    // Inversa de lo que haces al tocar:
    // angle_h = dx * DEG_PER_PX_X  → dx = angle_h / DEG_PER_PX_X
    // angle_v = dy * DEG_PER_PX_Y  → dy = angle_v / DEG_PER_PX_Y
    float dx = angle_h / DEG_PER_PX_X;
    float dy = angle_v / DEG_PER_PX_Y;

    float grid_x_f = cx + dx;
    float grid_y_f = cy - dy;

    // Asegurar dentro de la cuadrícula
    if (grid_x_f < 0)        grid_x_f = 0;
    if (grid_x_f > GRID_W)   grid_x_f = GRID_W;
    if (grid_y_f < 0)        grid_y_f = 0;
    if (grid_y_f > GRID_H)   grid_y_f = GRID_H;

    int grid_x = (int)(grid_x_f + 0.5f);
    int grid_y = (int)(grid_y_f + 0.5f);

    int local_x = GRID_X0 + grid_x;
    int local_y = GRID_Y0 + grid_y;

    // Esto dibuja el punto y las líneas como cuando tocas
    AngSelect_UpdateGraphics(g_gridImage, local_x, local_y);
}

//-------------------------------------------------------------//
//                      Funciones de RESET
//-------------------------------------------------------------//

/**
 * @brief 
 * @note
 * Resetea ambas referencias de ángulo (horizontal y vertical) a 0 grados.
 * Actualiza las etiquetas y los gráficos en la imagen de la cuadrícula
 * para reflejar las referencias reseteadas.
 * @param void
 */


void AngSelect_ResetRefBoth()
{
    AngSelect_ApplyRefFromAngles(0.0f, 0.0f);
}

/**
 * @brief
 * @note
 * Resetea el ángulo de referencia horizontal a 0 grados.
 * Actualiza la etiqueta y los gráficos en la imagen de la cuadrícula
 * para reflejar la referencia reseteada.
 * @param void
 * @return void 
 */


void AngSelect_ResetRefHorizontal()
{
    AngSelect_ApplyRefFromAngles(0.0f, g_refV);
}

/**
 * @brief 
 * @note
 * Resetea el ángulo de referencia vertical a 0 grados.
 * Actualiza la etiqueta y los gráficos en la imagen de la cuadrícula
 * para reflejar la referencia reseteada.
 * @param void
 * @return void
 */


void AngSelect_ResetRefVertical()
{
    AngSelect_ApplyRefFromAngles(g_refH, 0.0f);
}
