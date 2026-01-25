/* Esta librería, junto con su correspondiente "Tacho.h" define las funciones que leen los voltajes que generan
cada cuno de los tacogeneradores acoplados a sus motores correspondientes, y obtiene la velocidad
a la que están girando los mismos a partir de estos */

// Tacho.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"   // Para acceder a ui_V_rotor y ui_V_motor_principal

/**
 * @brief Configuración de tacómetro
 *
 * @param pinRotor  GPIO ADC del tacómetro del rotor
 * @param pinMotor  GPIO ADC del tacómetro del motor principal
 * @param voltsPer1000RPM  Voltaje de salida del tacómetro para 1000 RPM
 */
struct TachoConfig {
    uint8_t pinRotor;
    uint8_t pinMotor;
    float voltsPer1000RPM;
};

/**
 * @brief Inicializa el módulo del tacómetro
 * Configura ADC y almacena parámetros.
 */
void Tacho_begin(const TachoConfig &config);

/**
 * @brief Lee los tacómetros, convierte a RPM y actualiza los labels de LVGL.
 *
 * Debe llamarse periódicamente (ej: cada 100 ms)
 *
 * @note
 * La librería aplica un filtro anti-oscilación/anti-picos:
 *  - Solo actualiza el valor mostrado si la variación respecto al valor mostrado anterior
 *    es de al menos 100 rpm.
 *  - Además, ese nuevo valor debe mantenerse estable durante al menos 0.5 s,
 *    entendiendo estable como que las muestras dentro de esa ventana no superan
 *    un rango (max-min) de 100 rpm.
 */
void Tacho_update();
