#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <ui.h>
#include <Wire.h>

// ==== Librerías personalizadas ====
#include "DisplayTouch.h"
#include "Encoders.h"
#include "Tacho.h"
#include "MotorControl.h"
#include "IRControl.h"
#include "PID_Parameters.h"
#include "Ang_Select.h"
#include "General_Diagram.h"
#include "SD_Control.h"
#include "Element_Modifier.h"
#include "ScreensaverState.h"
#include "Load_Screen.h"
#include "Navigation.h"
#include "Remote_Diagram.h"

// ================================
// Definiciones de pines y constantes
// ================================

// Pines DAC
#define G1_DAC_PIN  25  // DAC1 (GPIO 25) - Motor / G1
#define G2_DAC_PIN  26  // DAC2 (GPIO 26) - Rotor / G2

// Pines ADC (tacómetros)
#define G39_PIN 39   // V rotor de cola
#define G36_PIN 36   // V motor principal

// Pantalla LCD
#define TFT_WIDTH   480
#define TFT_HEIGHT  320
#define IR_RECV_PIN 34

// Pin de interrupción del táctil (T_IRQ del XPT2046)
#define T_IRQ_PIN 35

// Pines del ESP32 que van a los inversores de SEL / RST / OE del HCTL
#define PIN_SEL 14
#define PIN_RST 12
#define PIN_OE  13

// Constante para conversión de voltaje a RPM para las lecturas de los tacómetros
const float TACH_VOLTS_PER_1000RPM = 0.52f;

// Cuentas por vuelta
const float COUNTS_PER_REV_VERTICAL   = 2000.0f;  // ejemplo
const float COUNTS_PER_REV_HORIZONTAL = 2000.0f;  // ejemplo

// ================================
// Objetos y configuración global
// ================================

TFT_eSPI tft = TFT_eSPI();

// Configuración de encoders
EncoderConfig g_encoderConfig = {
    COUNTS_PER_REV_VERTICAL,
    COUNTS_PER_REV_HORIZONTAL
};

// Configuración de tacómetro
TachoConfig g_tachoCfg = {
    .pinRotor        = G39_PIN,
    .pinMotor        = G36_PIN,
    .voltsPer1000RPM = TACH_VOLTS_PER_1000RPM
};

// Datos de calibración táctil
uint16_t g_touchCalData[5] = { 327, 3460, 264, 3497, 7 };

// Configuración del display + táctil
DisplayTouchConfig g_displayCfg = {
    .width           = TFT_WIDTH,
    .height          = TFT_HEIGHT,
    .t_irq_pin       = T_IRQ_PIN,
    .calibrationData = g_touchCalData
};

// Registros
extern int  Registro_MP;
extern int  Registro_RDC;

//Variables para la temporización del salvapantallas

extern PrevScreenId g_prevScreenId;
extern bool         g_screensaverActive;
extern uint32_t     g_lastActivityMs;

// Timeout de inactividad (ms)
static const uint32_t INACTIVITY_TIMEOUT_MS = 60000;  // 60 s

// Grupo pata navegación en pantalla utilizando las flechas del mando
lv_group_t * g_navGroup = nullptr;

// Estilo para cuando un objeto tiene el foco
lv_style_t   style_focus;

// ================================
// setup()
// ================================

void setup() {

    pinMode(LED, OUTPUT);
    pinMode (Buzzer, OUTPUT);

    //Encdendemos el LED durante el arranque
    digitalWrite(LED, HIGH);
    
    //Hacemos sonar el Buzzer durante un segundo en el inicio a toda potencia
    BuzzerWrite(HIGH);
    delay(1000);
    BuzzerWrite(LOW);

    Serial.begin(115200);
    Serial.println("Inicializando ESP32...");

    // Buses
    SPI.begin(); 
    Wire.begin();

    // CS de SD y TFT
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);

    // Inicializar pantalla, táctil y LVGL (como antes)
    DisplayTouch_begin(tft, g_displayCfg);

    // -----------------------------------------------------------------
    //  MENSAJES DE "CARGA" SIMULADOS ANTES DE ui_init()
    // -----------------------------------------------------------------
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(1);
    tft.setTextSize(2);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setCursor(10, 20);

    //----------------------------------------------------------------------------------------------//
    //                     Grupo pata navegar por el menú utilizanzo las flechas
    //----------------------------------------------------------------------------------------------//

    g_navGroup = lv_group_create();
    lv_group_set_wrap(g_navGroup, true);    // al llegar al final, vuelve al primero

    // Estilo de foco (borde verde)
    lv_style_init(&style_focus);
    lv_style_set_outline_width(&style_focus, 4);
    lv_style_set_outline_color(&style_focus, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_outline_opa(&style_focus, LV_OPA_COVER);

    // Montar la tarjeta SD
    tft.print("Montando tarjeta SD... ");
    delay(1000);    
    if (SD.begin(SD_CS)) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("SD OK");
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("ERROR");
    }
    delay(1000);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.println("Inicializando control de motores...");
    delay(1000);

    tft.println("Inicializando encoders...");
    delay(1000);

    tft.println("Cargando ajustes PID...");
    delay(1000);

    tft.println("Inicializando UI...");
    delay(500);

    // Esquema del TRMS para la pantalla de inicio
    int trmsCenterX = TFT_WIDTH / 2;
    int trmsBaseY   = 210;
    DrawTRMSFigure(tft, trmsCenterX, trmsBaseY);
    
    //Barra de carga con 3 segundos de duración
    Load_bar(tft);
    
    // -----------------------------------------------------------------
    //  AHORA SÍ: INICIALIZAR LA UI REAL DE LVGL
    // -----------------------------------------------------------------

    ui_init();
    Serial.println("UI cargada correctamente.");

    // Abilitar Labels como obejtos clicables para ahorrar elementos en uso de botones
    EnableConfigLabelsClickable();

    // Incializar navegación con control remoto
    SetupScreen1Nav();

    //---------------------------------------------------------------------------------------------//
    //----------------------------------------------------------------------------------------------//

    // Ocultar de inicio las imágenes de detalle
    lv_obj_add_flag(ui_EsquemaPIDCompleto,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_EsquemaTRMSCompleto, LV_OBJ_FLAG_HIDDEN);

    // Inicializar manejo del esquema general
    GeneralDiagram_Init(ui_EsquemaGeneral);

    // Inicializar tacómetros (ADC + RPM + labels)
    Tacho_begin(g_tachoCfg);

    // Inicializar control de motores (DAC + sliders)
    MotorControl_begin(G1_DAC_PIN, G2_DAC_PIN);

    // Inicializar receptor IR (vía librería IRControl)
    IRControl_begin(IR_RECV_PIN);

    // Guardar configuración del mando IR desde EEPROM, si existe, de los contrario se cargarán unos datos
    // para un mando predeterminado, guardados en la memoria flash
    IRControl_loadConfigFromNVS(); 

    //Establecimiento de valores iniciales para los prámetros PID
    PID_LoadDefaults(); //Inicializar valores PID a sus valores predeterminados
    PID_LoadFromNVS(); // Si hay algo en EEPROM, lo sobreescribe y actualiza UI

    //Inicializar la función para la detección de parámetros de entrada para el control PID
    AngSelect_Init(ui_Image12, ui_Label65, ui_Label64); // (Cuadrícula, Ang_Horizontal, Ang_Vertical)

    //Reset de encoders y registros antes de empezar
    Reset_Encoders_Registros();

    // Arrancamos contadores de actividad
    g_lastActivityMs = millis();

    Serial.println("Sistema listo (display + encoders + tacho + motores + IR).");

    //Hacemos sonar el Buzzer durante un segundo cuando el proceso de carga ha terminado por completo
    BuzzerWrite(HIGH);
    delay(1000);
    BuzzerWrite(LOW);

    //Apagamos el LED al finalizar el proceso de carga

    digitalWrite(LED, LOW);
}


// ================================
// loop()
// ================================

void loop() {
    // ---------------------------
    // 1) Gestionar LVGL (display + táctil)
    // ---------------------------
    DisplayTouch_taskHandler();
    delay(5);

    // ---------------------------
    // 2) Detectar actividad por TÁCTIL
    //    (cualquier toque en la pantalla cuenta como actividad)
    // ---------------------------
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
        RegisterActivity();   // actualiza g_lastActivityMs
    }

    // ---------------------------
    // 3) Tacómetros: actualizar RPM cada 100 ms
    // ---------------------------
    static uint32_t lastTachoUpdate = 0;
    uint32_t now = millis();
    if (now - lastTachoUpdate > 100) {
        Tacho_update();
        lastTachoUpdate = now;
    }
    
    //Elimininar mensaje de confirmación de carga de datos desde SD en caso de que esta se haya producido, pasado un tiempo
    if (flag_Config_Message == true){
        Show_Config_Message_Selected();
    }

    // ---------------------------
    // 4) IR: leer mando y actuar
    // ---------------------------
    IRControlEvent ev = IRControl_poll();
    if (ev.hasEvent) {  
        
        // 4.1) Primero: si estamos en modo aprendizaje de la pantalla del mando,
        // que consuma este evento y no haga nada más.
            if (RemoteDiagram_HandleIRLearn(ev)) {
                // Solo se ha usado para programar un botón → no navegar, ni sliders, etc.
                return;
            }

        // 4.2) Cambiar de pantalla si procede
        lv_obj_t *scr_act = lv_scr_act();
        switch (ev.screen) {
            case IRScreenTarget::SCREEN1:
                // Solo cambiar si NO estamos en la pantalla 9
                if (scr_act != ui_Screen9) {
                    _ui_screen_change(
                        &ui_Screen1,
                        LV_SCR_LOAD_ANIM_FADE_ON,
                        500, 0,
                        &ui_Screen1_screen_init
                    );
                    // Habilitar control con mando a distancia en la pantalla 1
                    SetupScreen1Nav();
                }
                break;

            case IRScreenTarget::NONE:
            default:
                break;
        }

        // 4.3) Navegación con flechas
        if (ev.nav != IRNavKey::NONE) {
            HandleIRNavigation(ev.nav);
        }

        // 4.4) Control de sliders
        if (ev.deltaSlider != 0) {
            HandleDeltaSlider(ev.deltaSlider);
        }

        // 4.5) Entrada numérica en labels
        if (ev.digit >= 0) {
            HandleNumericDigit(ev.digit);
        }

        // 4.6) Signo negativo
        if (ev.minus) {
            HandleNumericMinus();
        }
    }


    // ---------------------------
    // 5) Comprobar INACTIVIDAD y activar salvapantallas (Screen10)
    // ---------------------------
    now = millis();
    if (now - g_lastActivityMs > INACTIVITY_TIMEOUT_MS) {
        lv_obj_t *act = lv_scr_act();

        // Si no estamos ya en Screen10, cambiamos
        if (act != ui_Screen10) {

            // 1) Guardar de qué pantalla venimos
            if      (act == ui_Screen1) g_prevScreenId = SCR_1;
            else if (act == ui_Screen2) g_prevScreenId = SCR_2;
            else if (act == ui_Screen3) g_prevScreenId = SCR_3;
            else if (act == ui_Screen4) g_prevScreenId = SCR_4;
            else if (act == ui_Screen5) g_prevScreenId = SCR_5;
            else if (act == ui_Screen6) g_prevScreenId = SCR_6;
            else if (act == ui_Screen7) g_prevScreenId = SCR_7;
            else if (act == ui_Screen8) g_prevScreenId = SCR_8;
            else if (act == ui_Screen9) g_prevScreenId = SCR_9;
            else                        g_prevScreenId = SCR_NONE;

            // 2) Activar flag de salvapantallas
            g_screensaverActive = true;

            // 3) Ir a Screen10 (salvapantallas)
            _ui_screen_change(
                &ui_Screen10,
                LV_SCR_LOAD_ANIM_FADE_ON,
                500, 0,
                &ui_Screen10_screen_init
            );
        }
    }
}