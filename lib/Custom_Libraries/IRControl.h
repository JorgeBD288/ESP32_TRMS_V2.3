/* Esta librería, junto con su correspondiente "IRControl.cpp" define las funciones que leen y decodifican
los comandos recibidos a través del sensor de infrarrojos, utilizando la librería <IRremote.h> y genera la
acción correspondiente que será ejecutada en el código principal */

// IRControl.h
#pragma once

#include <Arduino.h>

/**
 * @brief Pantallas que puede pedir el mando IR.
 */
enum class IRScreenTarget {
    NONE = 0,
    SCREEN1
};

enum class IRNavKey {
    NONE = 0,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ENTER
};

/**
 * @brief Resultado de procesar un comando IR.
 *
 * - hasEvent: true si se ha recibido algún comando válido.
 * - command:  valor crudo del comando (para debug).
 * - deltaMD: incremento/decremento que se debe aplicar a Registro_MD.
 * - deltaR:  incremento/decremento que se debe aplicar a Registro_R.
 * - screen:  pantalla a la que hay que cambiar (o NONE si no aplica).
 */
struct IRControlEvent {
    bool hasEvent;
    uint32_t command;
    int8_t deltaSlider;
    IRScreenTarget screen;
    IRNavKey nav;

    int8_t digit;      // -1 => sin dígito, 0–9 => tecla numérica pulsada
    bool   minus;      // true si se ha pulsado "-"
    bool power;              // true si es el botón POWER
};

/**
 * @brief Inicializa el receptor IR.
 *
 * Debe llamarse en setup().
 *
 * @param irPin Pin donde está conectado el receptor IR.
 */
void IRControl_begin(uint8_t irPin);

/**
 * @brief Lee (no bloqueante) si ha llegado algún comando IR.
 *
 * Si hay comando, devuelve IRControlEvent con hasEvent = true.
 * Si no, hasEvent = false.
 */
IRControlEvent IRControl_poll();

// Configuración de dígitos 0–9
void IRControl_setDigitCode(uint8_t digit, uint32_t code);
uint32_t IRControl_getDigitCode(uint8_t digit);

// Configuración de flechas / ENTER
void IRControl_setNavCode(IRNavKey key, uint32_t code);
uint32_t IRControl_getNavCode(IRNavKey key);

// Configuración de + y -
void IRControl_setPlusCode(uint32_t code);
uint32_t IRControl_getPlusCode();

void IRControl_setMinusCode(uint32_t code);
uint32_t IRControl_getMinusCode();

// Configuración de POWER
void IRControl_setPowerCode(uint32_t code);
uint32_t IRControl_getPowerCode();

// Guardar y cargar datos utilizando la EEPROM
void IRControl_loadConfigFromNVS();
void IRControl_saveConfigToNVS();