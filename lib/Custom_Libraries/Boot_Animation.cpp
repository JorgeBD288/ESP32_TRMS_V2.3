#include "Boot_Animation.h"

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>


// Fuente personalizada para poder incluir tildes
LV_FONT_DECLARE(Montserrat_Medium_18_Latin);

// ------------------------------------------------------------
// AJUSTES RÁPIDOS
// ------------------------------------------------------------
static const int CANVAS_W = 480;
static const int CANVAS_H = 320;

// Fondo blanco
static const lv_color_t COL_BG = lv_color_make(255, 255, 255);

// Colores UI
static const lv_color_t COL_PURPLE_UI = lv_color_make(138, 43, 226);   // morado (bola morada + barra)
static const lv_color_t COL_GREEN_DK  = lv_color_make(0, 100, 60);     // verde oscuro (nombre + bola verde)

// Explosión (morado)
static const lv_color_t COL_EXPLO = lv_color_make(138, 43, 226);

// Tamaños
static const int BALL_R  = 22;
static const int ORBIT_R = 130;

// Estela (por objetos)
static const int TRAIL_SEG   = 16;
static const int TRAIL_R_MIN = 2;

// Tiempos
static const uint32_t TICK_MS     = 16;
static const uint32_t ORBIT_MS    = 1800;
static const uint32_t CONVERGE_MS = 1500;
static const uint32_t EXPLODE_MS  = 650;
static const uint32_t REVEAL_MS   = 900;
static const uint32_t BAR_FILL_MS = 5000;

// ------------------------------------------------------------
// RECURSO IMAGEN TRMS (SquareLine)
// ------------------------------------------------------------

extern const lv_img_dsc_t ui_img_imagen_load_screen_png;

// ------------------------------------------------------------
// ESTADO INTERNO
// ------------------------------------------------------------
enum Phase { PH_ORBIT, PH_CONVERGE, PH_EXPLODE, PH_REVEAL, PH_DONE };

static struct {
    bool running = false;

    lv_obj_t* scr = nullptr;

    lv_obj_t* ball_g = nullptr;
    lv_obj_t* ball_p = nullptr;

    lv_obj_t* trail_g[TRAIL_SEG] = {0};
    lv_obj_t* trail_p[TRAIL_SEG] = {0};

    int tgx[TRAIL_SEG] = {0}, tgy[TRAIL_SEG] = {0};
    int tpx[TRAIL_SEG] = {0}, tpy[TRAIL_SEG] = {0};
    bool trail_init = false;

    lv_obj_t* blast = nullptr;

    // >>> CAMBIO: en vez de lbl_big (texto), usamos img_trms (imagen)
    lv_obj_t* img_trms = nullptr;

    lv_obj_t* lbl_small = nullptr;

    lv_obj_t* img_left = nullptr;
    lv_obj_t* img_right = nullptr;

    lv_obj_t* bar = nullptr;

    lv_timer_t* timer = nullptr;

    uint32_t t0 = 0;
    Phase phase = PH_ORBIT;
    Phase last_phase = PH_DONE;

    uint8_t progress = 0;

    bootanim_done_cb_t cb_done = nullptr;
    BootAnimConfig cfg = {};

    int cx = CANVAS_W / 2;
    int cy = CANVAS_H / 2;

    bool done_cb_fired = false;
} S;

// ------------------------------------------------------------
// HELPERS
// ------------------------------------------------------------
static float lerp(float a, float b, float t){ return a + (b - a) * t; }
static uint32_t now_ms(){ return lv_tick_get(); }

/**
 * @brief 
 * @note
  Diseña un objeto como un círculo de color dado, con radio y opacidad especificados.
  Configura el tamaño, el radio, el color de fondo, la opacidad de fondo
  y el ancho del borde del objeto proporcionado.
  Esta función es útil para crear elementos gráficos circulares
  en la interfaz de usuario.
 * @param o 
 * @param c 
 * @param r 
 * @param opa 
 */

static void style_circle(lv_obj_t* o, lv_color_t c, int r, lv_opa_t opa = LV_OPA_COVER)
{
    lv_obj_set_size(o, r * 2, r * 2);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_border_width(o, 0, 0);
}

/**
 * @brief Set the center object
 * @note
  Centra un objeto en las coordenadas (x, y) especificadas.
  Calcula la posición superior izquierda del objeto
  restando la mitad de su ancho y altura a las coordenadas dadas,
  y luego establece la posición del objeto en esas coordenadas.
  Esta función es útil para centrar objetos en la pantalla
  o dentro de otros contenedores en la interfaz de usuario.
 * 
 * @param o 
 * @param x 
 * @param y 
 */

static void set_center(lv_obj_t* o, int x, int y)
{
    if(!o) return;
    lv_obj_set_pos(o, x - (int)lv_obj_get_width(o) / 2,
                      y - (int)lv_obj_get_height(o) / 2);
}

/**
 * @brief 
 * @note
  Muestra u oculta las estelas de las bolas.
  Itera sobre los objetos de estela verde y morada,
  y añade o elimina la bandera LV_OBJ_FLAG_HIDDEN
  según el valor del parámetro 'hide'.
  Esta función es útil para controlar la visibilidad
  de las estelas durante diferentes fases de la animación.
 * @param hide 
 */

static void trails_hide(bool hide)
{
    for (int i = 0; i < TRAIL_SEG; ++i) {
        if (S.trail_g[i]) {
            if (hide) lv_obj_add_flag(S.trail_g[i], LV_OBJ_FLAG_HIDDEN);
            else      lv_obj_clear_flag(S.trail_g[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (S.trail_p[i]) {
            if (hide) lv_obj_add_flag(S.trail_p[i], LV_OBJ_FLAG_HIDDEN);
            else      lv_obj_clear_flag(S.trail_p[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief 
 * @note
  Muestra u oculta las bolas verde y morada.
  Añade o elimina la bandera LV_OBJ_FLAG_HIDDEN
  de los objetos de bola según el valor del parámetro 'hide'.
  Esta función es útil para controlar la visibilidad
  de las bolas durante diferentes fases de la animación.
 * 
 * @param hide 
 */

static void balls_hide(bool hide)
{
    if (S.ball_g) { if (hide) lv_obj_add_flag(S.ball_g, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(S.ball_g, LV_OBJ_FLAG_HIDDEN); }
    if (S.ball_p) { if (hide) lv_obj_add_flag(S.ball_p, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(S.ball_p, LV_OBJ_FLAG_HIDDEN); }
}

/**
 * @brief
 * @note
  Desplaza las posiciones almacenadas en las estelas
  y añade una nueva posición al inicio de cada estela.
  Mueve todas las posiciones de las estelas verde y morada
  una posición hacia atrás en sus respectivos arrays,
  y luego asigna las nuevas coordenadas (xg, yg) para la estela verde
  y (xp, yp) para la estela morada en la primera posición de los arrays.
  Esta función es útil para actualizar las posiciones de las estelas
  durante la animación de las bolas.
 * 
 * @param xg 
 * @param yg 
 * @param xp 
 * @param yp 
 */

static void trails_shift_and_push(int xg, int yg, int xp, int yp)
{
    for (int i = TRAIL_SEG - 1; i > 0; --i) {
        S.tgx[i] = S.tgx[i - 1]; S.tgy[i] = S.tgy[i - 1];
        S.tpx[i] = S.tpx[i - 1]; S.tpy[i] = S.tpy[i - 1];
    }
    S.tgx[0] = xg; S.tgy[0] = yg;
    S.tpx[0] = xp; S.tpy[0] = yp;
}

/**
 * @brief 
 * @note
  Aplica las posiciones y estilos a los objetos de las estelas.
  Itera sobre los segmentos de las estelas verde y morada,
  calculando el tamaño y la opacidad de cada segmento
  basándose en su índice.
  Luego, establece el tamaño, la opacidad y la posición
  de cada objeto de estela según las posiciones almacenadas.
  Esta función es útil para actualizar visualmente las estelas
  durante la animación de las bolas.
 */

static void trails_apply_to_objs()
{
    for (int i = 0; i < TRAIL_SEG; ++i) {
        float k = (float)i / (float)(TRAIL_SEG - 1);
        int r = (int)lerp((float)BALL_R * 0.70f, (float)TRAIL_R_MIN, k);
        lv_opa_t opa = (lv_opa_t)lerp(140.0f, 0.0f, k);

        if (S.trail_g[i]) {
            lv_obj_set_size(S.trail_g[i], r * 2, r * 2);
            lv_obj_set_style_bg_opa(S.trail_g[i], opa, 0);
            set_center(S.trail_g[i], S.tgx[i], S.tgy[i]);
        }
        if (S.trail_p[i]) {
            lv_obj_set_size(S.trail_p[i], r * 2, r * 2);
            lv_obj_set_style_bg_opa(S.trail_p[i], opa, 0);
            set_center(S.trail_p[i], S.tpx[i], S.tpy[i]);
        }
    }
}

/**
 * @brief 
 * @note
  Asegura que los objetos de las estelas estén creados.
  Si las estelas no han sido inicializadas,
  crea los objetos de estela verde y morada,
  aplica el estilo circular a cada uno
  y establece sus posiciones iniciales en el centro de la pantalla.
  Esta función es útil para preparar los objetos de estela
  antes de iniciar la animación.
 */

static void ensure_trails_created()
{
    if (S.trail_init) return;

    for (int i = 0; i < TRAIL_SEG; ++i) {
        S.trail_g[i] = lv_obj_create(S.scr);
        style_circle(S.trail_g[i], COL_GREEN_DK, TRAIL_R_MIN, LV_OPA_0);
        lv_obj_clear_flag(S.trail_g[i], LV_OBJ_FLAG_SCROLLABLE);

        S.trail_p[i] = lv_obj_create(S.scr);
        style_circle(S.trail_p[i], COL_PURPLE_UI, TRAIL_R_MIN, LV_OPA_0);
        lv_obj_clear_flag(S.trail_p[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    for (int i = 0; i < TRAIL_SEG; ++i) {
        S.tgx[i] = S.cx; S.tgy[i] = S.cy;
        S.tpx[i] = S.cx; S.tpy[i] = S.cy;
    }

    S.trail_init = true;
}

// ------------------------------------------------------------
// FINALIZAR
// ------------------------------------------------------------

/**
 * @brief   
 * @note
  Finaliza la animación de arranque.
  Detiene la animación, elimina el temporizador
  y llama a la función de devolución de llamada
  si no se ha llamado previamente.
  Esta función es útil para limpiar los recursos
  y notificar cuando la animación ha terminado. 
 */

static void finish_anim()
{
    if (!S.running) return;

    S.running = false;

    if (S.timer) { lv_timer_del(S.timer); S.timer = nullptr; }

    if (!S.done_cb_fired && S.cb_done) {
        S.done_cb_fired = true;
        S.cb_done();
    }
}

// ------------------------------------------------------------
// TIMER LOOP
// ------------------------------------------------------------

/**
 * @brief 
 * @note
  Función de callback del temporizador que maneja
  las diferentes fases de la animación de arranque.
  Actualiza la posición de las bolas, las estelas,
  la explosión y la revelación de los elementos UI
  basándose en el tiempo transcurrido y la fase actual.
  Esta función es llamada periódicamente por el temporizador
  para crear una animación fluida y sincronizada. 
 */

static void tick_cb(lv_timer_t*)
{
    if (!S.running) return;

    ensure_trails_created();

    const uint32_t t = now_ms() - S.t0;

    if (S.phase != S.last_phase) {

        if (S.phase == PH_ORBIT) {
            balls_hide(false);
            trails_hide(false);
        }

        if (S.phase == PH_EXPLODE) {
            balls_hide(true);
            trails_hide(true);
            if (S.blast) lv_obj_set_style_bg_opa(S.blast, LV_OPA_0, 0);
        }

        if (S.phase == PH_REVEAL) {
            // >>> CAMBIO: mostramos imagen TRMS en vez de label
            if (S.img_trms)  lv_obj_clear_flag(S.img_trms,  LV_OBJ_FLAG_HIDDEN);

            if (S.lbl_small) lv_obj_clear_flag(S.lbl_small, LV_OBJ_FLAG_HIDDEN);
            if (S.img_left)  lv_obj_clear_flag(S.img_left,  LV_OBJ_FLAG_HIDDEN);
            if (S.img_right) lv_obj_clear_flag(S.img_right, LV_OBJ_FLAG_HIDDEN);
            if (S.bar)       lv_obj_clear_flag(S.bar,       LV_OBJ_FLAG_HIDDEN);
        }

        S.last_phase = S.phase;
    }

    // FASE 1: ORBIT
    if (S.phase == PH_ORBIT) {
        float u = (float)t / (float)ORBIT_MS;
        if (u >= 1.0f) { u = 1.0f; S.phase = PH_CONVERGE; S.t0 = now_ms(); }

        float ang = u * 2.0f * 3.1415926f * 1.5f;

        int xg = S.cx + (int)(ORBIT_R * cosf(ang));
        int yg = S.cy + (int)(ORBIT_R * sinf(ang));

        int xp = S.cx + (int)(ORBIT_R * cosf(ang + 3.1415926f));
        int yp = S.cy + (int)(ORBIT_R * sinf(ang + 3.1415926f));

        set_center(S.ball_g, xg, yg);
        set_center(S.ball_p, xp, yp);

        trails_shift_and_push(xg, yg, xp, yp);
        trails_apply_to_objs();
        return;
    }

    // FASE 2: CONVERGE
    if (S.phase == PH_CONVERGE) {
        float u = (float)t / (float)CONVERGE_MS;
        if (u >= 1.0f) { u = 1.0f; S.phase = PH_EXPLODE; S.t0 = now_ms(); }

        float ease = 1.0f - powf(1.0f - u, 2.0f);
        float r = (float)ORBIT_R * (1.0f - ease);

        float ang = u * 2.0f * 3.1415926f * 2.0f;

        int xg = S.cx + (int)(r * cosf(ang));
        int yg = S.cy + (int)(r * sinf(ang));

        int xp = S.cx + (int)(r * cosf(ang + 3.1415926f));
        int yp = S.cy + (int)(r * sinf(ang + 3.1415926f));

        set_center(S.ball_g, xg, yg);
        set_center(S.ball_p, xp, yp);

        trails_shift_and_push(xg, yg, xp, yp);
        trails_apply_to_objs();
        return;
    }

    // FASE 3: EXPLODE
    if (S.phase == PH_EXPLODE) {
        float u = (float)t / (float)EXPLODE_MS;
        if (u >= 1.0f) { u = 1.0f; S.phase = PH_REVEAL; S.t0 = now_ms(); }

        int rr = (int)lerp(6.0f, 160.0f, u);
        lv_obj_set_size(S.blast, rr * 2, rr * 2);
        set_center(S.blast, S.cx, S.cy);
        lv_obj_set_style_bg_opa(S.blast, (lv_opa_t)lerp(200.0f, 0.0f, u), 0);
        return;
    }

    // FASE 4: REVEAL + barra 5s
    if (S.phase == PH_REVEAL) {
        float u = (float)t / (float)REVEAL_MS;
        if (u > 1.0f) u = 1.0f;

        // >>> CAMBIO: opacidad para imagen TRMS
        if (S.img_trms)  lv_obj_set_style_opa(S.img_trms,  (lv_opa_t)lerp(0.0f, 255.0f, u), 0);

        if (S.lbl_small) lv_obj_set_style_opa(S.lbl_small, (lv_opa_t)lerp(0.0f, 255.0f, u), 0);
        if (S.img_left)  lv_obj_set_style_opa(S.img_left,  (lv_opa_t)lerp(0.0f, 255.0f, u), 0);
        if (S.img_right) lv_obj_set_style_opa(S.img_right, (lv_opa_t)lerp(0.0f, 255.0f, u), 0);
        if (S.bar)       lv_obj_set_style_opa(S.bar,       (lv_opa_t)lerp(0.0f, 255.0f, u), 0);

        uint8_t p = (uint8_t)((t >= BAR_FILL_MS) ? 100 : (t * 100) / BAR_FILL_MS);
        S.progress = p;
        if (S.bar) lv_bar_set_value(S.bar, p, LV_ANIM_OFF);

        if (t >= BAR_FILL_MS) {
            S.phase = PH_DONE;
            finish_anim();
        }
        return;
    }

    if (S.phase == PH_DONE) {
        finish_anim();
        return;
    }
}

// ------------------------------------------------------------
// API
// ------------------------------------------------------------

/**
 * @brief 
    Comprueba si la animación de arranque está en ejecución.
    @note
    Esta función devuelve true si la animación de arranque
    está actualmente en ejecución, y false si ha finalizado.
 * @return true 
 * @return false 
 */

bool BootAnim_IsRunning(void) { return S.running; }

/**
 * @brief 
 * @note
  Comprueba si la animación de arranque ha finalizado.
  Esta función devuelve true si la animación de arranque
  ha terminado, y false si todavía está en ejecución.
 * @return true 
 * @return false 
 */

bool BootAnim_IsFinished(void) { return !S.running; }

/**
 * @brief
 * @note
  Establece el progreso de la barra de carga en porcentaje.
  Actualiza el valor interno de progreso y ajusta
  la barra de carga para reflejar el porcentaje dado.
  Esta función es útil para mostrar el progreso
  de la carga durante la animación de arranque.
  @param percent Porcentaje de progreso (0-100).
  Si la barra no está creada, no hace nada.
  Asegura que el valor de porcentaje esté dentro del rango válido
  antes de actualizar la barra.
  Si la barra no ha sido creada aún, la función simplemente retorna sin hacer nada. 
 * @param percent 
 */

void BootAnim_SetProgress(uint8_t percent)
{
    S.progress = percent;
    if (S.bar) lv_bar_set_value(S.bar, percent, LV_ANIM_OFF);
}

/**
 * @brief 
 * @note
  Limpia y libera los recursos utilizados por la animación de arranque.
  Elimina el temporizador y la pantalla de la animación,
  y restablece todos los punteros y banderas internas a sus valores iniciales.
  Esta función es útil para asegurar que no queden recursos
  colgados después de que la animación haya terminado. 
  Esta función es llamada internamente por BootAnim_Stop()
  y también se utiliza para reiniciar el estado
  si se inicia una nueva animación mientras otra está en curso.
 */

static void cleanup()
{
    if (S.timer) { lv_timer_del(S.timer); S.timer = nullptr; }
    if (S.scr) { lv_obj_del(S.scr); S.scr = nullptr; }

    S.ball_g = nullptr;
    S.ball_p = nullptr;
    S.blast = nullptr;

    S.img_trms = nullptr;
    S.lbl_small = nullptr;

    S.img_left = nullptr;
    S.img_right = nullptr;
    S.bar = nullptr;

    for (int i = 0; i < TRAIL_SEG; ++i) {
        S.trail_g[i] = nullptr;
        S.trail_p[i] = nullptr;
    }

    S.trail_init = false;
    S.running = false;
    S.done_cb_fired = false;
}

/**
 * @brief 
 * @note
  Detiene la animación de arranque y limpia los recursos.
  Llama a la función cleanup() para liberar todos los recursos
  asociados con la animación de arranque.
  Esta función es útil para detener la animación
  en cualquier momento antes de que haya finalizado. 
  Después de llamar a esta función, la animación
  ya no estará en ejecución. 
  Si la animación ya ha sido detenida, no hace nada. 
  Esta función también asegura que la función
  de devolución de llamada de finalización no se llame nuevamente
  si ya se ha llamado previamente.
 */

void BootAnim_Stop(void)
{
    cleanup();
}

/**
 * @brief
 * @note
  Inicia la animación de arranque con la configuración dada.
  Configura los parámetros de la animación según la estructura
  BootAnimConfig proporcionada y establece la función
  de devolución de llamada que se llamará al finalizar la animación.
  Crea los objetos gráficos necesarios para la animación,
  carga la pantalla de animación y comienza el temporizador
  que manejará las actualizaciones de la animación.
  Esta función es útil para iniciar una secuencia
  de animación de arranque personalizada. 
 * @param cfg 
 * @param cb_done 
 */

void BootAnim_Start(const BootAnimConfig* cfg, bootanim_done_cb_t cb_done)
{
    if (S.running) cleanup();

    S.cb_done = cb_done;
    if (cfg) S.cfg = *cfg;

    S.phase = PH_ORBIT;
    S.last_phase = PH_DONE;
    S.t0 = now_ms();
    S.running = true;
    S.progress = 0;
    S.trail_init = false;
    S.done_cb_fired = false;

    S.scr = lv_obj_create(NULL);
    lv_obj_clear_flag(S.scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(S.scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(S.scr, LV_OPA_COVER, 0);

    // Esferas
    S.ball_g = lv_obj_create(S.scr);
    style_circle(S.ball_g, COL_GREEN_DK, BALL_R);

    S.ball_p = lv_obj_create(S.scr);
    style_circle(S.ball_p, COL_PURPLE_UI, BALL_R);

    // Explosión
    S.blast = lv_obj_create(S.scr);
    style_circle(S.blast, COL_EXPLO, 10, LV_OPA_0);
    set_center(S.blast, S.cx, S.cy);

    // ---- TRMS como IMAGEN (oculta al inicio) ----
    S.img_trms = lv_img_create(S.scr);
    lv_img_set_src(S.img_trms, &ui_img_imagen_load_screen_png);
    lv_obj_align(S.img_trms, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_flag(S.img_trms, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(S.img_trms, LV_OPA_0, 0);

    // ---- Nombre ----
    S.lbl_small = lv_label_create(S.scr);
    lv_label_set_text(S.lbl_small, (S.cfg.title_small ? S.cfg.title_small : "Jorge Benítez Domingo"));
    lv_obj_set_style_text_color(S.lbl_small, COL_GREEN_DK, 0);

    lv_obj_set_style_text_font(S.lbl_small, &Montserrat_Medium_18_Latin, LV_PART_MAIN);

    lv_obj_align(S.lbl_small, LV_ALIGN_CENTER, 0, 65);
    lv_obj_add_flag(S.lbl_small, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(S.lbl_small, LV_OPA_0, 0);

    // ---- Logos (opcionales) ----
    if (S.cfg.logo_left_src) {
        S.img_left = lv_img_create(S.scr);
        lv_img_set_src(S.img_left, S.cfg.logo_left_src);
        lv_obj_align(S.img_left, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_add_flag(S.img_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(S.img_left, LV_OPA_0, 0);
    }
    if (S.cfg.logo_right_src) {
        S.img_right = lv_img_create(S.scr);
        lv_img_set_src(S.img_right, S.cfg.logo_right_src);
        lv_obj_align(S.img_right, LV_ALIGN_TOP_RIGHT, -10, 10);
        lv_obj_add_flag(S.img_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(S.img_right, LV_OPA_0, 0);
    }

    // ---- Barra de carga (morado) ----
    S.bar = lv_bar_create(S.scr);
    lv_obj_set_size(S.bar, 320, 18);
    lv_obj_align(S.bar, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_bar_set_range(S.bar, 0, 100);
    lv_bar_set_value(S.bar, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(S.bar, COL_PURPLE_UI, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(S.bar, lv_color_make(230, 230, 230), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(S.bar, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_add_flag(S.bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(S.bar, LV_OPA_0, 0);

    lv_scr_load(S.scr);

    S.timer = lv_timer_create(tick_cb, TICK_MS, nullptr);
}
