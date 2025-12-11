/* Esta librería, junto con su correspondiente "Encoders.h", sirve para declarar las funciones 
que controlan los HCTL-2016, el TCA9539 y la representación en la pantalla LCD 
de los ángulos obtenidos por los encoders */

// Encoders.cpp
#include "Encoders.h"
#include <Wire.h>
#include <lvgl.h>

// Dirección I2C del TCA9539
#define TCA_ADDR 0x74

// Pines hacia inversores (guardados internamente)
static uint8_t s_pinSEL;
static uint8_t s_pinRST;
static uint8_t s_pinOE;

// Configuración de cuentas por vuelta
static EncoderConfig s_config;

// Series del chart
static lv_chart_series_t * s_serHorizontal = nullptr; // rojo
static lv_chart_series_t * s_serVertical   = nullptr; // azul

/*--------------------------------------------------
 * Funciones internas (static) - no visibles fuera
 *-------------------------------------------------*/

static void hctl_setSEL(bool level) {
    // Las señales están invertidas por los inversores de hardware
    digitalWrite(s_pinSEL, !level);
}

static void hctl_setRST(bool level) {
    digitalWrite(s_pinRST, !level);
}

static void hctl_setOE(bool level) {
    digitalWrite(s_pinOE, !level);
}

static void tca_writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(TCA_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

static uint8_t tca_readReg(uint8_t reg) {
    Wire.beginTransmission(TCA_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(TCA_ADDR, (uint8_t)1);
    return Wire.read();
}

static uint8_t tca_readPort(uint8_t port) {
    // Port 0 -> registro 0x00, Port 1 -> registro 0x01
    return tca_readReg(port == 0 ? 0x00 : 0x01);
}

static void tca_init() {
    // Todos los pines como entradas
    tca_writeReg(0x06, 0xFF);  // Configuration Port 0
    tca_writeReg(0x07, 0xFF);  // Configuration Port 1

    // Polaridad normal
    tca_writeReg(0x04, 0x00);  // Polarity Port 0
    tca_writeReg(0x05, 0x00);  // Polarity Port 1
}

static void hctl_resetCounters_internal() {
    hctl_setRST(0);
    delayMicroseconds(5);
    hctl_setRST(1);
}

static uint8_t hctl_readByte(uint8_t port, bool highByte) {
    if (highByte)
        hctl_setSEL(0);   // HIGH BYTE
    else
        hctl_setSEL(1);   // LOW BYTE

    delayMicroseconds(2);

    return tca_readPort(port);
}

static uint16_t hctl_readVertical() {
    hctl_setOE(0);

    uint8_t hi = hctl_readByte(0, true);
    uint8_t lo = hctl_readByte(0, false);

    return ((uint16_t)hi << 8) | lo;
}

static uint16_t hctl_readHorizontal() {
    hctl_setOE(0);

    uint8_t hi = hctl_readByte(1, true);
    uint8_t lo = hctl_readByte(1, false);

    return ((uint16_t)hi << 8) | lo;
}

static float countsToDegrees(uint16_t count, float countsPerRev) {
    if (countsPerRev <= 0.0f) return 0.0f;
    uint16_t modulo = (uint16_t)countsPerRev;
    return 360.0f * (float(count % modulo) / countsPerRev);
}

/*--------------------------------------------------
 * API pública
 *-------------------------------------------------*/

void Encoders_begin(uint8_t pinSEL,
                    uint8_t pinRST,
                    uint8_t pinOE,
                    const EncoderConfig &config) {
    s_pinSEL  = pinSEL;
    s_pinRST  = pinRST;
    s_pinOE   = pinOE;
    s_config  = config;

    pinMode(s_pinSEL, OUTPUT);
    pinMode(s_pinRST, OUTPUT);
    pinMode(s_pinOE,  OUTPUT);

    // Estado inicial
    hctl_setOE(1);
    hctl_setRST(1);
    hctl_setSEL(0);

    // Inicializar expansor TCA9539
    tca_init();

    // Reset de contadores
    hctl_resetCounters_internal();

    // Activar salidas
    hctl_setOE(0);

    Serial.println("Encoders: inicializados (HCTL + TCA9539).");
}

void Encoders_resetCounters() {
    hctl_resetCounters_internal();
}

EncoderAngles Encoders_readAngles() {
    EncoderAngles ang{0.0f, 0.0f};

    uint16_t cntV = hctl_readVertical();
    uint16_t cntH = hctl_readHorizontal();

    ang.verticalDeg   = countsToDegrees(cntV, s_config.countsPerRevVertical);
    ang.horizontalDeg = countsToDegrees(cntH, s_config.countsPerRevHorizontal);

    return ang;
}

void Encoders_chartInit(lv_obj_t *chart) {
    if (!chart) return;

    // Serie roja (horizontal) eje primario Y
    s_serHorizontal = lv_chart_add_series(
        chart,
        lv_palette_main(LV_PALETTE_RED),
        LV_CHART_AXIS_PRIMARY_Y
    );

    // Serie azul (vertical) eje secundario Y
    s_serVertical = lv_chart_add_series(
        chart,
        lv_palette_main(LV_PALETTE_BLUE),
        LV_CHART_AXIS_SECONDARY_Y
    );

    // Modo scroll
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    // Rango en grados [-180, 180] para ambos ejes
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,   -180, 180);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, -180, 180);
}

void Encoders_chartAddSample(lv_obj_t *chart, const EncoderAngles &angles) {
    if (!chart) return;
    if (!s_serVertical || !s_serHorizontal) return;

    // Vertical (azul)
    lv_chart_set_next_value(chart, s_serVertical, angles.verticalDeg);
    // Horizontal (rojo)
    lv_chart_set_next_value(chart, s_serHorizontal, angles.horizontalDeg);
}
