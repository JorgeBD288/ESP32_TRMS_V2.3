/* Esta librería, junto con su correspondiente "DisplayTouch.h", se encarga de generar las funciones que
controlan la respresentación de imágenes en la pantalla LCD, así como las funciones que detectan los contactos
con la pantalla táctil, ejecutando los comandos correspondientes a los elementos que se seleccionan a través
de esta */

// DisplayTouch.cpp
#include "DisplayTouch.h"

// -----------------------------------------------------------------------------
// Variables y buffers internos
// -----------------------------------------------------------------------------

// Guardamos un puntero a la TFT para usarla en el flush
static TFT_eSPI *s_tft = nullptr;

// Configuración de pantalla
static uint16_t s_width  = 480;
static uint16_t s_height = 320;

// Pin de interrupción del táctil
static uint8_t s_t_irq_pin = 35;

// Contador de interrupciones (para ver que realmente saltan)
static volatile uint32_t s_irq_count = 0;

// Buffer de dibujo para LVGL
static lv_disp_draw_buf_t s_draw_buf;

// Ajusta esto si quieres más/menos líneas de buffer
static const uint16_t DISP_BUF_LINES = 20;
static const uint16_t DISP_MAX_WIDTH = 480;

// Array estático para el buffer (480 * 20 = 9600 píxeles)
static lv_color_t s_buf[DISP_MAX_WIDTH * DISP_BUF_LINES];

// -----------------------------------------------------------------------------
// ISR del pin táctil
// -----------------------------------------------------------------------------

// XPT2046: T_IRQ pasa a LOW mientras hay toque.
// Aquí solo incrementamos un contador para depuración.
static void IRAM_ATTR touch_isr()
{
    s_irq_count++;
}

// -----------------------------------------------------------------------------
// Callbacks de LVGL (display flush + lectura táctil)
// -----------------------------------------------------------------------------

/**
 * @brief Función de flush de LVGL -> TFT
 */
static void my_disp_flush(lv_disp_drv_t *disp_drv,
                          const lv_area_t *area,
                          lv_color_t *color_p)
{
    if (!s_tft || !color_p) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    int32_t w = (area->x2 - area->x1 + 1);
    int32_t h = (area->y2 - area->y1 + 1);

    s_tft->startWrite();
    s_tft->setAddrWindow(area->x1, area->y1, w, h);
    s_tft->pushColors((uint16_t *)&color_p->full, w * h, true);
    s_tft->endWrite();

    lv_disp_flush_ready(disp_drv);
}

/**
 * @brief Lectura del táctil para LVGL.
 *
 * - NO dependemos solo de la interrupción (porque ya vimos que es delicado).
 * - Leemos el nivel del pin T_IRQ:
 *      * HIGH  -> no hay toque
 *      * LOW   -> hay posible toque
 * - Cuando detectamos un flanco HIGH -> LOW, medimos el voltaje y lo mostramos.
 * - Solo llamamos a getTouch() cuando T_IRQ está LOW.
 */
static void my_touchpad_read(lv_indev_drv_t *indev_drv,
                             lv_indev_data_t *data)
{
    (void) indev_drv;

    if (!data || !s_tft) return;

    static int last_level = HIGH;

    uint16_t x = 0, y = 0;

    // Leer nivel del pin T_IRQ (G35)
    int level = digitalRead(s_t_irq_pin);   // 1 = reposo, 0 = toque

    // Detectar flanco HIGH -> LOW (inicio de toque)
    if (last_level == HIGH && level == LOW) {
        // Leer voltaje en G35 una vez al principio del toque
        int raw = analogRead(s_t_irq_pin);
        float voltage = (raw / 4095.0f) * 3.3f;

        Serial.print("NUEVO TOQUE - G35 ADC raw = ");
        Serial.print(raw);
        Serial.print("  ->  V = ");
        Serial.print(voltage, 3);
        Serial.print(" V   |  irq_count = ");
        Serial.println(s_irq_count);
    }

    if (level == LOW) {
        // Solo intentamos leer el táctil cuando T_IRQ está en LOW
        bool touched = s_tft->getTouch(&x, &y);

        if (touched) {
            data->state   = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;

            // Si quieres ver coordenadas:
            // Serial.print("Touch: ");
            // Serial.print(x);
            // Serial.print(", ");
            // Serial.println(y);
        } else {
            // T_IRQ LOW pero no hay lectura válida: tratamos como RELEASE
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        // T_IRQ en HIGH -> no hay toque
        data->state = LV_INDEV_STATE_REL;
    }

    last_level = level;
}

// -----------------------------------------------------------------------------
// API pública
// -----------------------------------------------------------------------------

void DisplayTouch_begin(TFT_eSPI &tft, const DisplayTouchConfig &cfg)
{
    s_tft       = &tft;
    s_width     = cfg.width;
    s_height    = cfg.height;
    s_t_irq_pin = cfg.t_irq_pin;

    Serial.println("DisplayTouch_begin(): configurando TFT + táctil (modo híbrido con IRQ)");

    // Pin de IRQ del táctil: entrada con pull-up
    pinMode(s_t_irq_pin, INPUT_PULLUP);

    // Configuración del ADC para G35 (para leer el voltaje cuando haya toque)
    analogReadResolution(12);                             // 0..4095
    analogSetPinAttenuation(s_t_irq_pin, ADC_11db);       // ~0..3.3 V

    // Adjuntamos la interrupción solo para contar eventos (depuración)
    attachInterrupt(
        digitalPinToInterrupt(s_t_irq_pin),
        touch_isr,
        CHANGE   // ya vimos en tu test que así sí dispara
    );

    // Inicializar pantalla TFT
    s_tft->init();
    delay(200);
    s_tft->setRotation(1);
    s_tft->fillScreen(TFT_BLACK);

    // Calibración táctil (si se facilita)
    if (cfg.calibrationData != nullptr) {
        s_tft->setTouch((uint16_t *)cfg.calibrationData);
        Serial.println("Calibración táctil aplicada.");
    } else {
        Serial.println("Sin datos de calibración táctil.");
    }

    // Inicializar LVGL
    lv_init();
    delay(50);

    // Inicializar buffer de dibujo de LVGL
    uint32_t buf_size = s_width * 5;
    if (buf_size > DISP_MAX_WIDTH * DISP_BUF_LINES) {
        buf_size = DISP_MAX_WIDTH * DISP_BUF_LINES;
    }

    lv_disp_draw_buf_init(&s_draw_buf, s_buf, nullptr, buf_size);

    // Configurar el driver de display
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = s_width;
    disp_drv.ver_res  = s_height;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Configurar driver de entrada (táctil)
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("DisplayTouch inicializado (TFT + LVGL + táctil, T_IRQ leído por nivel y IRQ para depuración).");
}

void DisplayTouch_taskHandler()
{
    lv_timer_handler();
}
