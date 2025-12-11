/* Esta librería, junto con su correspondiente "Encoders.cpp", sirve para declarar las funciones 
que controlan los HCTL-2016, el TCA9539 y la representación en la pantalla LCD 
de los ángulos obtenidos por los encoders */

#pragma once
#include <Arduino.h>
#include <lvgl.h>   // <-- necesario para lv_obj_t y lv_chart_series_t

// Cuentas por vuelta de cada eje (puedes ajustar desde el .ino si quieres)
struct EncoderConfig {
    float countsPerRevVertical;
    float countsPerRevHorizontal;
};

// Struct para devolver los ángulos
struct EncoderAngles {
    float verticalDeg;
    float horizontalDeg;
};

// Inicialización general (TCA, HCTL, pines)
// Usa la configuración y la guarda internamente (s_config en el .cpp)
void Encoders_begin(uint8_t pinSEL,
                    uint8_t pinRST,
                    uint8_t pinOE,
                    const EncoderConfig &config);

// Reset de contadores
void Encoders_resetCounters();

// Leer ángulos actualizados (en grados) usando la config interna
EncoderAngles Encoders_readAngles();

// Si se utiliza LVGL: inicializar gráfico y añadir muestras
void Encoders_chartInit(lv_obj_t * chart);
void Encoders_chartAddSample(lv_obj_t * chart,
                             const EncoderAngles &angles);
