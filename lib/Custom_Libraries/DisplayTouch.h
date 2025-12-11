/* Esta librería, junto con su correspondiente "DisplayTouch.cpp", se encarga de generar las funciones que
controlan la respresentación de imágenes en la pantalla LCD, así como las funciones que detectan los contactos
con la pantalla táctil, ejecutando los comandos correspondientes a los elementos que se seleccionan a través
de esta */

// DisplayTouch.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

/**
 * @brief Configuración del display + táctil
 */
struct DisplayTouchConfig {
    uint16_t width;    // Ancho de la pantalla (ej: 480)
    uint16_t height;   // Alto de la pantalla (ej: 320)
    uint8_t  t_irq_pin;  // Pin T_IRQ del táctil (ej: 35)

    /**
     * Puntero a los datos de calibración (5 valores uint16_t).
     * Si es nullptr, no se aplica calibración.
     */
    const uint16_t *calibrationData;
};

/**
 * @brief Inicializa la TFT, el táctil y LVGL.
 *
 * Debes llamar a esto en setup(), DESPUÉS de SPI.begin() y ANTES de ui_init().
 *
 * @param tft  Referencia al objeto global TFT_eSPI
 * @param cfg  Configuración del display/táctil
 */
void DisplayTouch_begin(TFT_eSPI &tft, const DisplayTouchConfig &cfg);

/**
 * @brief Llama al manejador de LVGL.
 *
 * Es básicamente un wrapper de lv_timer_handler().
 * Útil si quieres que todo lo gráfico pase por esta librería.
 */
void DisplayTouch_taskHandler();
