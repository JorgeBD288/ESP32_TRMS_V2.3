#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <ui.h>
#include <Wire.h>
#include <Ansiterm.h>

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
#include "Boot_Animation.h"
#include "Ang_Select.h"
#include "PID_Control.h"
#include "SerialAnsiLogger.h"

// Activa/desactiva trazas de depuración del TCA9539
#define TCA_DEBUG 1

#if TCA_DEBUG
  #define TCA_DBG_PRINTLN(x) Serial.println(x)
  #define TCA_DBG_PRINT(x)   Serial.print(x)
#else
  #define TCA_DBG_PRINTLN(x) do{}while(0)
  #define TCA_DBG_PRINT(x)   do{}while(0)
#endif

// Convierte códigos de Wire.endTransmission() a texto (Arduino Wire)
static const char* i2cErrToStr(uint8_t err) {
    switch (err) {
        case 0: return "OK";
        case 1: return "data too long";
        case 2: return "NACK on address";
        case 3: return "NACK on data";
        case 4: return "other error";
        default: return "unknown";
    }
}

// Habilitar monitor Ansi
Ansiterm term;
SerialAnsiLogger logger(term, 50); // 50ms

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
#define PIN_SEL 12
#define PIN_RST 13
#define PIN_OE  14

// Pin para decidir el tipo de arranque
#define PIN_BOOT 0

//Pines I2C
#define PIN_SDA 21
#define PIN_SCL 22

int t = 0;

// Constante para conversión de voltaje a RPM para las lecturas de los tacómetros
const float TACH_VOLTS_PER_1000RPM = 0.52f;

// Cuentas por vuelta en los encoders HCTL-2016
static const float COUNTS_PER_REV = 2000.0f;

// Frecuencia de impresión
static const uint32_t PRINT_EVERY_MS = 50;

// ================================
// Objetos y configuración global
// ================================

TFT_eSPI tft = TFT_eSPI();

// Configuración de encoders
static EncoderConfig g_cfg = {
  .countsPerRevVertical   = COUNTS_PER_REV,
  .countsPerRevHorizontal = COUNTS_PER_REV
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

// Variable para la temporización de la muestra de mensaje de comprobación de guardado y carga de configuración
extern float temp_Save_Message;
extern float temp_Load_Message;

//Variables para la temporización del salvapantallas

extern PrevScreenId g_prevScreenId;
extern PrevScreenId g_prevScreenId_temp;
extern bool         g_screensaverActive;
extern uint32_t     g_lastActivityMs;

// Timeout de inactividad (ms)
static const uint32_t INACTIVITY_TIMEOUT_MS = 60000;  // 60 s

// Grupo pata navegación en pantalla utilizando las flechas del mando
lv_group_t * g_navGroup = nullptr;

// Estilo para cuando un objeto tiene el foco
lv_style_t   style_focus;

// Valores de referencia iniciales para las consignas de la gráfica del PID
float refH_old = 0.0f;
float refV_old = 0.0f;


// ================================
// setup()
// ================================

void setup() {

    pinMode(LED, OUTPUT);
    pinMode (Buzzer, OUTPUT);

    //Encendemos el LED durante el arranque
    digitalWrite(LED, HIGH);

    //Pin para el control de la iluminación de la pantalla (solo funciona si el jumper está conectado correctamente)
    pinMode(BRILLO_PIN, OUTPUT);
    // Configurar PWM
    ledcSetup(BRILLO_PWM_CHANNEL, BRILLO_PWM_FREQ, BRILLO_PWM_RES);
    ledcAttachPin(BRILLO_PIN, BRILLO_PWM_CHANNEL);

    // Brillo inicial al 100%
    ledcWrite(BRILLO_PWM_CHANNEL, 255);
    
    //Hacemos sonar el Buzzer durante un segundo en el inicio a toda potencia
    BuzzerWrite(HIGH);
    delay(500);
    BuzzerWrite(LOW);

    Serial.begin(115200);
    Serial.println("Inicializando ESP32...");
    logger.begin(true); // imprime cabecera
    //logger.enablePlot(true, 70, 16, true); // width=70, height=16, autoscale
    // Muestreo (50ms) y refresh del plot (1000ms)
    //logger.setSamplePeriodMs(50);
    //logger.setPlotRefreshMs(1000);
    // Consigna (línea roja)
    //logger.enableSetpointLine(true);
    
    // Ejes: etiquetas Y cada 4 filas, ticks X cada 20 muestras (1s si Ts=50ms)
    //logger.setYLabelEveryRows(4);
    //logger.setXTickEverySamples(20);

    // Buses
    SPI.begin(); 
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000); // 400 kHz (Fast Mode)
    Wire.setTimeOut(50);

    // --- Ajustes de la librería de encoders ---
    // Puertos de encoders, vertical y horizontal, cruzados: IN0=H y IN1=V
    Encoders_setSwapPorts(true);

    // Para leer bien, es preciso togglear OE entre bytes
    Encoders_setToggleOE(true);

    // Tiempos “tipo cronograma” (en us, con margen)
    Encoders_setTimingsUs(
        5,  // after SEL
        5,  // after OE enable
        5   // tri-state gap
    );

    // CS de SD y TFT
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);

    // Inicializar pantalla, táctil y LVGL
    DisplayTouch_begin(tft, g_displayCfg);
    
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

    //----------------------------------------------------------------------------------------------//
    //                    Modo de arranque ténico con detalles sobre el inicio del sistema
    //----------------------------------------------------------------------------------------------//

    pinMode(PIN_BOOT, INPUT_PULLUP);
    // Lee BOOT al principio (pulsado = LOW)
    bool debugMode = (digitalRead(PIN_BOOT) == LOW);

    if (debugMode){
        Serial.println("Arranque en MODO DEBUG");

        // -----------------------------------------------------------------
        //  MENSAJES DE "CARGA" SIMULADOS ANTES DE ui_init()
        // -----------------------------------------------------------------
        tft.fillScreen(TFT_BLACK);
        tft.setRotation(1);
        tft.setTextSize(2);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.setCursor(10, 20);

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
    }
    else {
        Serial.println("Arranque en MODO COMERCIAL");

        delay(500);
        if (SD.begin(SD_CS)) {
            Serial.println("SD OK");
        } else {
            Serial.println("SD ERROR");
        }
        delay(500);

        //----------------------------------------------------------------------------------------------//
        //                                 Modo de arranque normal (comercial)
        //----------------------------------------------------------------------------------------------//

        // Animación comercial
        BootAnimConfig cfg = {};
        cfg.title_big = "TRMS";
        cfg.title_small = "Jorge Benítez Domingo";
        cfg.logo_left_src  = &ui_img_logo_ugr_redimensionado_png;
        cfg.logo_right_src = &ui_img_logo_granasat_redimensionado_png;
        cfg.logos_on_top = true;

        BootAnim_Start(&cfg, nullptr);

        // IMPORTANTE: mantener vivo LVGL mientras dura la animación
        while (!BootAnim_IsFinished()) {
            DisplayTouch_taskHandler();
            delay(1);
        }
    }
    
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
    PID4_LoadFromCurr(g_pidCurr);    // copia a s_pid4_params y resetea estados
    PID4_SetEnabled(false);           // habilita el control cuando quieras

    //Selección de modo de funcionamiento PID por defecto
    PID4_SetMode(PIDMode::MIMO_FULL);

    //Inicialización con un valor nulo de las series de datos de las consignas del PID
    uint16_t pc = lv_chart_get_point_count(ui_GraphEncoder3);
    for (uint16_t i = 0; i < pc; i++) {
        ui_GraphEncoder3_series_refH->y_points[i] = 0;
        ui_GraphEncoder3_series_refV->y_points[i] = 0;
    }


    //Inicializar la función para la detección de parámetros de entrada para el control PID
    AngSelect_Init(ui_Image12, ui_Label65, ui_Label64); // (Cuadrícula, Ang_Horizontal, Ang_Vertical)

    // Inicializar encoders (HCTL + TCA9539)
    
    bool ok = Encoders_begin(PIN_SEL, PIN_RST, PIN_OE, g_cfg);
    if (!ok) {
        Serial.println("[FATAL] Encoders_begin fallo (I2C/TCA).");
        while (true) delay(1000);
    }

    Serial.println("[OK] Encoders_begin correcto.");
    Serial.println("[INFO] 0° = posicion al arrancar (tras reset interno).");
   
    //Reset de encoders y registros antes de empezar
    Reset_Encoders_Registros();

    //Inicializamos los charts para la muestra de datos de los encoders
    Encoders_chartInit(ui_GraphEncoder);

    // Arrancamos contadores de actividad
    g_lastActivityMs = millis();

    Serial.println("Sistema listo (display + encoders + tacho + motores + IR).");

    //Hacemos sonar el Buzzer durante un segundo cuando el proceso de carga ha terminado por completo
    BuzzerWrite(HIGH);
    delay(500);
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
    
    //Elimininar mensaje de confirmación de carga de datos desde SD y guardado en SD, en caso de que esta se haya producido, pasado un tiempo
   
    if (flag_Save_Message == true){
        if (millis() - temp_Save_Message > 2500){
            Show_Save_Message_Selected();
        }
    }
    
    if (flag_Config_Message == true){
        if (millis() - temp_Load_Message > 2500){
            Show_Config_Message_Selected();
        }
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
/*
    //----------------------------------------------------
    // 5) TEST: 2 senoidales + actualizar charts
    //----------------------------------------------------
    static uint32_t lastRead   = 0;
    static uint32_t lastChart1 = 0;
    static uint32_t lastChart2 = 0;
    static uint32_t lastPrint  = 0;

    now = millis();

    // ¿Está el chart visible?
    bool chart1Visible =
        (lv_scr_act() == ui_Screen2) &&
        (ui_GraphEncoder != nullptr) &&
        !lv_obj_has_flag(ui_GraphEncoder, LV_OBJ_FLAG_HIDDEN);

    bool chart2Visible =
        (lv_scr_act() == ui_Screen6) &&
        (ui_GraphEncoder3 != nullptr) &&
        !lv_obj_has_flag(ui_GraphEncoder3, LV_OBJ_FLAG_HIDDEN);

    bool chartsVisible = chart1Visible || chart2Visible;

    // (A) Generar muestra senoidal (rápido)
    static float degH = 0.0f, degV = 0.0f;
    static float t = 0.0f;

    if (now - lastRead >= 50) {   // 50 ms
        uint32_t dt_ms = now - lastRead;
        lastRead = now;

        t += (float)dt_ms / 1000.0f;

        const float AMP  = 160.0f;
        const float F_HZ = 0.20f;                 // 0.20 Hz = 5 s por ciclo
        const float W    = 2.0f * 3.1415926f * F_HZ;  // <- IMPORTANTE: 2*pi*f

        degH = AMP * sinf(W * t);
        degV = AMP * cosf(W * t);
    }

    // (B) Chart 1 (solo si visible)
    if (chart1Visible && (now - lastChart1 >= 50)) {
        lastChart1 = now;

        // Ojo: usa las series DEL CHART 1
        lv_chart_set_next_value(ui_GraphEncoder, ui_GraphEncoder_series_1, (lv_coord_t)degH);
        lv_chart_set_next_value(ui_GraphEncoder, ui_GraphEncoder_series_2, (lv_coord_t)degV);

        lv_chart_refresh(ui_GraphEncoder);
    }

    // (C) Chart 2 (solo si visible)
    if (chart2Visible && (now - lastChart2 >= 50)) {
        lastChart2 = now;

        // Ojo: usa las series DEL CHART 2
        lv_chart_set_next_value(ui_GraphEncoder3, ui_GraphEncoder3_series_1, (lv_coord_t)degH);
        lv_chart_set_next_value(ui_GraphEncoder3, ui_GraphEncoder3_series_2, (lv_coord_t)degV);

        lv_chart_refresh(ui_GraphEncoder3);
    }

    // (D) Serial más lento (para no bloquear)
    uint32_t printPeriod = chartsVisible ? 400 : 100;
    if (now - lastPrint >= printPeriod) {
        lastPrint = now;
        Serial.printf("[SIM] H=%8.2f deg | V=%8.2f deg\n", degH, degV);
    }

*/

    //----------------------------------------------------
    // 5) Leer encoders (rápido) + actualizar charts (lento)
    //----------------------------------------------------
    static uint32_t lastRead   = 0;
    static uint32_t lastChart1 = 0;
    static uint32_t lastChart2 = 0;
    static uint32_t lastPrint  = 0;

    now = millis();

    // ¿Está el chart visible?
    bool chart1Visible =
        (lv_scr_act() == ui_Screen2) &&
        (ui_GraphEncoder != nullptr) &&
        !lv_obj_has_flag(ui_GraphEncoder, LV_OBJ_FLAG_HIDDEN);

    bool chart2Visible =
        (lv_scr_act() == ui_Screen6) &&
        (ui_GraphEncoder3 != nullptr) &&
        !lv_obj_has_flag(ui_GraphEncoder3, LV_OBJ_FLAG_HIDDEN);

    bool chartsVisible = chart1Visible || chart2Visible;

    // (A) Lectura rápida de encoders
    static int16_t cH = 0, cV = 0;   // guardamos último valor
    static bool lastOk = true;

    if (now - lastRead >= 50) {      // 50 ms lectura
        lastRead = now;

        int16_t tmpV = 0, tmpH = 0;
        bool ok = Encoders_readCounts(tmpV, tmpH);
        lastOk = ok;

        if (ok) {
            cV = tmpV;
            cH = tmpH;
        }
    }

    // (B) Convertir a grados (con tu CPR)
    const float K = 360.0f / COUNTS_PER_REV;
    float degH = (float)cH * K;
    float degV = (float)cV * K;

  //Comento esto temporalmente para usar el logger con los datos reales de los encoders, con menos retardos
    // (C) Chart 1 más lento (solo si visible)
    if (chart1Visible && lastOk && (now - lastChart1 >= 200)) {
        lastChart1 = now;

        // Series del chart 1 (Screen2)
        lv_chart_set_next_value(ui_GraphEncoder, ui_GraphEncoder_series_1, (lv_coord_t)degH);
        lv_chart_set_next_value(ui_GraphEncoder, ui_GraphEncoder_series_2, (lv_coord_t)degV);
        //lv_chart_refresh(ui_GraphEncoder);
    }

    // (D) Chart 2 más lento (solo si visible)
    if (chart2Visible && lastOk && (now - lastChart2 >= 200)) {
        lastChart2 = now;

        // Series del chart 2 (Screen6)
        lv_chart_set_next_value(ui_GraphEncoder3, ui_GraphEncoder3_series_1, (lv_coord_t)degH);
        lv_chart_set_next_value(ui_GraphEncoder3, ui_GraphEncoder3_series_2, (lv_coord_t)degV);
        lv_chart_refresh(ui_GraphEncoder3);
    }


    // (E) Serial
    //uint32_t printPeriod = chartsVisible ? 400 : 100;

    //if (logger_start == true){
        //logger.update(degV, degH); // visual (opcional)
    //}
    
    /*
    uint32_t printPeriod = 50;
    if (now - lastPrint >= printPeriod) {
        lastPrint = now;

        if (!lastOk) {
            Serial.println("[READ] ERROR leyendo counts (I2C).");
        } else {
            Serial.printf("[H] %6d  %8.2f deg | [V] %6d  %8.2f deg\n", cH, degH, cV, degV);
        }
    }
    */
    //----------------------------------------------------
    // 6) Control PID
    //----------------------------------------------------

    // ---- PID timing ----
    static uint32_t lastPidMs = 0;
    uint32_t nowMs = millis();
    float dt = (nowMs - lastPidMs) / 1000.0f;
    if (dt < 0.001f) dt = 0.001f;       // seguridad
    if (dt > 0.100f) dt = 0.100f;       // evita saltos grandes si la UI bloquea
    lastPidMs = nowMs;

    // ---- 1) Consignas desde AngSelect (GRADOS) ----
    float refH = AngSelect_GetRefHorizontal();
    float refV = AngSelect_GetRefVertical();
    PID4_SetReferences(refV, refH); // ojo: tu PID4_SetReferences(refVertDeg, refHorDeg)
    const float Err = 0.1f;  // 0.1°
    if (fabsf(refH - refH_old) > Err || fabsf(refV - refV_old) > Err) {
        Chart_UpdateReferences(refH, refV); 
        PID4_ResetStates();
        refH_old = refH;
        refV_old = refV;
    }

    // ---- 2) Medidas desde el loop (GRADOS) ----
    // degH = horizontal, degV = vertical

    // ---- 3) Ejecutar PID SOLO si lectura ok ----
    if (lastOk) {
        PID4_StepWithMeasurements(dt, degV, degH);
    } else {
        // si falla encoder, opcional: parar motores por seguridad
        // MotorControl_update(0, 0);
    }

    //if (logger_start) {
        //logger.update(degV, degH, refV, refH);
    //}

    // ------------------------------------------------------------
    // 7) Comprobar INACTIVIDAD y activar salvapantallas (Screen10)
    // ------------------------------------------------------------
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
            else if (act == ui_Screen11){
                if (g_prevScreenId != SCR_11) {
                    g_prevScreenId_temp = g_prevScreenId;
                    g_prevScreenId = SCR_11;
                }
            }
            else g_prevScreenId = SCR_NONE;

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

    /*

    uint8_t p0 = 0, p1 = 0;
    
    if (Encoders_readTcaPorts(p0, p1)) {
    Serial.print("P0 = ");
    Serial.print(p0, BIN);
    Serial.print(" | P1 = ");
    Serial.println(p1, BIN);
    } else {
        Serial.println("ERROR leyendo puertos del TCA");
    }

    delay(200);
    */

}