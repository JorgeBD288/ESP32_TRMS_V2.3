/* Esta librería, junto con su correspondiente "IRControl.h" define las funciones que leen y decodifican
los comandos recibidos a través del sensor de infrarrojos, utilizando la librería <IRremote.h> y genera la
acción correspondiente que será ejecutada en el código principal */

// IRControl.cpp
#include "IRControl.h"
#include <IRremote.h>
#include "ScreensaverState.h"
#include "ui.h"
#include <Preferences.h> 

// =====================
// TABLAS DE CÓDIGOS IR
// =====================

// Dígitos 0–9 (Valores por defecto))
static uint32_t s_digitCode[10] = {
    0x11, // 0
    0x4,  // 1
    0x5,  // 2
    0x6,  // 3
    0x8,  // 4
    0x9,  // 5
    0xA,  // 6
    0xC,  // 7
    0xD,  // 8
    0xE   // 9
};

// Flechas + ENTER (Valores por defecto)
static uint32_t s_navCodeLeft   = 0x65;
static uint32_t s_navCodeRight  = 0x62;
static uint32_t s_navCodeUp     = 0x60;
static uint32_t s_navCodeDown   = 0x61;
static uint32_t s_navCodeEnter  = 0x68;

// + / - para sliders (valroes por defecto)
static uint32_t s_plusCode  = 0x7;   // +
static uint32_t s_minusCode = 0xB;   // -

// POWER (Valor por defecto)
static uint32_t s_powerCode = 0x2;   // 0 = sin usar de momento

// Pin del receptor IR (Valor por defecto)
static uint8_t s_irPin = 0;

// Preferences para guardar en NVS
static Preferences s_prefs;

// =====================
// SETTERS / GETTERS
// =====================

/**
 * @brief 
 * Asigna un código IR a un dígito numérico.
 * @note
  Actualiza la tabla interna de códigos
  para el dígito especificado. 
 * @param digit 
 * @param code 
 */

void IRControl_setDigitCode(uint8_t digit, uint32_t code)
{
    if (digit < 10) {
        s_digitCode[digit] = code;
        Serial.print("[IRControl] Asignado código 0x");
        Serial.print(code, HEX);
        Serial.print(" al dígito ");
        Serial.println(digit);
    }
}

/**
 * @brief 
 * Obtiene el código IR asignado a un dígito numérico.
 * @note
  Consulta la tabla interna de códigos
  para el dígito especificado. 
 * @param digit 
 * @return uint32_t 
 */

uint32_t IRControl_getDigitCode(uint8_t digit)
{
    if (digit < 10) return s_digitCode[digit];
    return 0;
}

/**
 * @brief 
 * Asigna un código IR a una tecla de navegación.
 * @note
  Actualiza la tabla interna de códigos
  para la tecla de navegación especificada. 
 * @param key 
 * @param code 
 */

void IRControl_setNavCode(IRNavKey key, uint32_t code)
{
    switch (key) {
        case IRNavKey::LEFT:   s_navCodeLeft   = code; break;
        case IRNavKey::RIGHT:  s_navCodeRight  = code; break;
        case IRNavKey::UP:     s_navCodeUp     = code; break;
        case IRNavKey::DOWN:   s_navCodeDown   = code; break;
        case IRNavKey::ENTER:  s_navCodeEnter  = code; break;
        default: break;
    }
    Serial.print("[IRControl] Asignado código 0x");
    Serial.print(code, HEX);
    Serial.print(" a NAV key ");
    Serial.println((int)key);
}

/**
 * @brief 
 * Obtiene el código IR asignado a una tecla de navegación.
 * @note
  Consulta la tabla interna de códigos
  para la tecla de navegación especificada. 
 * @param key 
 * @return uint32_t 
 */

uint32_t IRControl_getNavCode(IRNavKey key)
{
    switch (key) {
        case IRNavKey::LEFT:   return s_navCodeLeft;
        case IRNavKey::RIGHT:  return s_navCodeRight;
        case IRNavKey::UP:     return s_navCodeUp;
        case IRNavKey::DOWN:   return s_navCodeDown;
        case IRNavKey::ENTER:  return s_navCodeEnter;
        default:               return 0;
    }
}

/**
 * @brief 
 * Asigna un código IR a la tecla PLUS (+).
 * @note
  Actualiza la tabla interna de códigos
  para la tecla PLUS. 
 * @param code 
 */

void IRControl_setPlusCode(uint32_t code)
{
    s_plusCode = code;
    Serial.print("[IRControl] Asignado código 0x");
    Serial.print(code, HEX);
    Serial.println(" a PLUS (+)");
}

/**
 * @brief 
 * Obtiene el código IR asignado a la tecla PLUS (+).
 * @note
  Consulta la tabla interna de códigos
  para la tecla PLUS. 
 * @return uint32_t 
 */

uint32_t IRControl_getPlusCode()
{
    return s_plusCode;
}

/**
 * @brief 
 * Asigna un código IR a la tecla MINUS (-).
 * @note
  Actualiza la tabla interna de códigos
  para la tecla MINUS. 
 * @param code 
 */

void IRControl_setMinusCode(uint32_t code)
{
    s_minusCode = code;
    Serial.print("[IRControl] Asignado código 0x");
    Serial.print(code, HEX);
    Serial.println(" a MINUS (-)");
}

/**
 * @brief 
 * Obtiene el código IR asignado a la tecla MINUS (-).
 * @note
  Consulta la tabla interna de códigos
  para la tecla MINUS. 
 * @return uint32_t 
 */

uint32_t IRControl_getMinusCode()
{
    return s_minusCode;
}

/**
 * @brief 
 * Asigna un código IR a la tecla POWER.
 * @note
  Actualiza la tabla interna de códigos
  para la tecla POWER. 
 * @param code 
 */

void IRControl_setPowerCode(uint32_t code)
{
    s_powerCode = code;
    Serial.print("[IRControl] Asignado código 0x");
    Serial.print(code, HEX);
    Serial.println(" a POWER");
}

/**
 * @brief 
 * Obtiene el código IR asignado a la tecla POWER.
 * @note
  Consulta la tabla interna de códigos
  para la tecla POWER. 
 * @return uint32_t 
 */

uint32_t IRControl_getPowerCode()
{
    return s_powerCode;
}

// =====================
// INICIALIZACIÓN
// =====================

/**
 * @brief 
 * Inicializa el receptor IR.
 * @note
  Configura el pin del receptor
  y arranca la librería IRremote. 
 * @param irPin 
 */

void IRControl_begin(uint8_t irPin) {
    s_irPin = irPin;
    IrReceiver.begin(s_irPin, DISABLE_LED_FEEDBACK);

    Serial.print("IRControl inicializado en pin ");
    Serial.println(s_irPin);
}

// =====================
// LECTURA / DECODIFICACIÓN
// =====================

/**
 * @brief 
 * Lee y decodifica un comando IR recibido.
 * @note
  Si se recibe un comando válido,
  lo interpreta y rellena una estructura
  con la información del evento. 
 * @return IRControlEvent 
 */

IRControlEvent IRControl_poll() {
    IRControlEvent ev{};
    ev.hasEvent    = false;
    ev.command     = 0;
    ev.deltaSlider = 0;
    ev.screen      = IRScreenTarget::NONE; // Cambios entre pantallas
    ev.nav         = IRNavKey::NONE;       // Navegación utilizando flechas
    ev.digit       = -1;                   // Sin dígito
    ev.minus       = false;                // Tecla "-" especial
    ev.power       = false;                // Tecla POWER

    // Si no hay datos IR, salimos
    if (!IrReceiver.decode()) {
        return ev;
    }

    uint32_t cmd = IrReceiver.decodedIRData.command;
    ev.hasEvent  = true;
    ev.command   = cmd;

    // Registrar actividad para cualquier comando NO CERO y NO 0x40 (ruido)
    if (cmd != 0x0 && cmd != 0x40) {
        RegisterActivity();

        // Si el salvapantallas está activo, cualquier tecla lo quita
        // excepto el botón de "inicio" (0x2) que ya gestiona su propia lógica
        if (g_screensaverActive) {
            if (cmd != 0x2) {
                lv_event_send(ui_Screen10, LV_EVENT_CLICKED,  NULL);
            }
        }
    }

    Serial.print("Código IR recibido: 0x");
    Serial.println(cmd, HEX);

    // -----------------------------
    // 1) Botón de cambio de pantalla (home)
    //    Lo dejamos fijo en 0x2, como antes
    // -----------------------------
    if (cmd == 0x2) {
        ev.screen = IRScreenTarget::SCREEN1;
    }

    // -----------------------------
    // 2) Flechas + ENTER (tabla configurable)
    // -----------------------------
    if      (cmd == s_navCodeLeft)   ev.nav = IRNavKey::LEFT;
    else if (cmd == s_navCodeRight)  ev.nav = IRNavKey::RIGHT;
    else if (cmd == s_navCodeUp)     ev.nav = IRNavKey::UP;
    else if (cmd == s_navCodeDown)   ev.nav = IRNavKey::DOWN;
    else if (cmd == s_navCodeEnter)  ev.nav = IRNavKey::ENTER;

    // -----------------------------
    // 3) + / - (para sliders)
    // -----------------------------
    if (cmd == s_plusCode) {
        ev.deltaSlider = +10;
    }
    if (cmd == s_minusCode) {
        ev.deltaSlider = -10;
        ev.minus       = true;   // reutilizado también para signo en labels numéricas
    }

    // -----------------------------
    // 4) POWER
    // -----------------------------
    if (s_powerCode != 0 && cmd == s_powerCode) {
        ev.power = true;
    }

    // -----------------------------
    // 5) Dígitos 0–9 (tabla configurable)
    // -----------------------------
    for (int d = 0; d < 10; ++d) {
        if (cmd == s_digitCode[d]) {
            ev.digit = d;
            break;
        }
    }

    // Preparar para el próximo comando
    IrReceiver.resume();

    return ev;
}

// =====================
// CARGAR / GUARDAR EN NVS
// =====================

/**
 * @brief 
 * Carga la configuración de códigos IR desde NVS.
 * @note
  Lee los códigos almacenados en NVS
  y actualiza la tabla interna de códigos. 
 */

void IRControl_loadConfigFromNVS()
{
    if (!s_prefs.begin("ir_codes", true)) {   // true = solo lectura
        Serial.println("[IRControl] ERROR: no se pudo abrir NVS (ir_codes) para lectura");
        return;
    }

    // Dígitos 0–9
    for (uint8_t d = 0; d < 10; d++) {
        char key[8];
        snprintf(key, sizeof(key), "d%u", d);       // "d0", "d1", ...
        uint32_t def = s_digitCode[d];             // valor por defecto actual
        uint32_t val = s_prefs.getUInt(key, def);  // si no existe, deja el defecto
        s_digitCode[d] = val;
    }

    // Flechas / ENTER
    s_navCodeLeft  = s_prefs.getUInt("navL", s_navCodeLeft);
    s_navCodeRight = s_prefs.getUInt("navR", s_navCodeRight);
    s_navCodeUp    = s_prefs.getUInt("navU", s_navCodeUp);
    s_navCodeDown  = s_prefs.getUInt("navD", s_navCodeDown);
    s_navCodeEnter = s_prefs.getUInt("navE", s_navCodeEnter);

    // + / -
    s_plusCode  = s_prefs.getUInt("plus",  s_plusCode);
    s_minusCode = s_prefs.getUInt("minus", s_minusCode);

    // POWER
    s_powerCode = s_prefs.getUInt("power", s_powerCode);

    s_prefs.end();
    Serial.println("[IRControl] Configuración IR cargada desde NVS");
}

/**
 * @brief 
 * Guarda la configuración de códigos IR en NVS.
 * @note
  Escribe los códigos actuales
  en la memoria NVS. 
 */

void IRControl_saveConfigToNVS()
{
    if (!s_prefs.begin("ir_codes", false)) {   // false = lectura/escritura
        Serial.println("[IRControl] ERROR: no se pudo abrir NVS (ir_codes) para escritura");
        return;
    }

    // Dígitos 0–9
    for (uint8_t d = 0; d < 10; d++) {
        char key[8];
        snprintf(key, sizeof(key), "d%u", d);
        s_prefs.putUInt(key, s_digitCode[d]);
    }

    // Flechas / ENTER
    s_prefs.putUInt("navL", s_navCodeLeft);
    s_prefs.putUInt("navR", s_navCodeRight);
    s_prefs.putUInt("navU", s_navCodeUp);
    s_prefs.putUInt("navD", s_navCodeDown);
    s_prefs.putUInt("navE", s_navCodeEnter);

    // + / -
    s_prefs.putUInt("plus",  s_plusCode);
    s_prefs.putUInt("minus", s_minusCode);

    // POWER
    s_prefs.putUInt("power", s_powerCode);

    s_prefs.end();
    Serial.println("[IRControl] Configuración IR guardada en NVS");
}
