#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Cuentas por vuelta de cada eje
struct EncoderConfig {
    float countsPerRevVertical;
    float countsPerRevHorizontal;
};

// Ángulos en grados
struct EncoderAngles {
    float verticalDeg;
    float horizontalDeg;
};

// Inicialización general (TCA, HCTL, pines)
bool Encoders_begin(uint8_t pinSEL,
                    uint8_t pinRST,
                    uint8_t pinOE,
                    const EncoderConfig &config);

// Reset de contadores
void Encoders_resetCounters();

// Leer ángulos en grados (0° = posición inicial tras begin/reset)
EncoderAngles Encoders_readAngles();

// Leer cuentas crudas (ya en int16_t, C2), con offset interno aplicado en readAngles
// Aquí devuelve el contador “tal cual” (firmado), sin convertir a grados.
bool Encoders_readCounts(int16_t &countV, int16_t &countH);

// Ajustes runtime de lectura (por tu caso hardware)
void Encoders_setSwapPorts(bool swap);              // true: IN0=H, IN1=V
void Encoders_setToggleOE(bool enable);             // true: toggle OE entre bytes
void Encoders_setTimingsUs(uint16_t afterSelUs,
                           uint16_t afterOeUs,
                           uint16_t triUs);

// LVGL
void Encoders_chartInit(lv_obj_t * chart);
void Encoders_chartAddSample(lv_obj_t * chart,
                             const EncoderAngles &angles);

// Debug I2C / TCA
bool Encoders_readTcaPorts(uint8_t &port0, uint8_t &port1);
void i2cScan();

// API pública de control (si sigues queriendo controlar pines desde aquí)
void Encoders_setOE(bool level);
void Encoders_setRST(bool level);
void Encoders_setSEL(bool level);
void Encoders_pulseReset(uint16_t high_us = 5);

void Encoders_chartBindSeries(lv_chart_series_t *serH, lv_chart_series_t *serV);
