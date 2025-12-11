/* Esta librería, junto con su correspondiente "Tacho.cpp" define las funciones que leen los voltajes que 
generan cada cuno de los tacogeneradores acoplados a sus motores correspondientes, y obtiene la velocidad
a la que están girando los mismos a partir de estos */

// Tacho.cpp
#include "Tacho.h"

// Almacenamos configuración global del módulo
static TachoConfig s_cfg;

/**
 * @brief Convierte valor ADC a voltaje en ESP32
 */
static float adcToVolts(uint16_t raw)
{
    return (raw / 4095.0f) * 3.3f; // ADC 12 bits → 0..3.3 V
}

/**
 * @brief El voltaje que recibe el ESP32 es un voltaje adaptado después de pasar por un circuito de adaptación. 
 *        Esta función permite obtener el valor de voltaje real, generado por el tacómetro
 */
static float voltsAdapter(float volts)
{
    return (2*(1.65-volts));
}

/**
 * @brief Convierte voltios → RPM según la curva del tacómetro
 */
static float voltsToRpm(float volts)
{
    if (s_cfg.voltsPer1000RPM <= 0.0f) return 0.0f;

    return (volts / s_cfg.voltsPer1000RPM) * 1000.0f;
}


void Tacho_begin(const TachoConfig &config)
{
    s_cfg = config;

    // Configurar pines como entrada analógica
    pinMode(s_cfg.pinRotor, INPUT);
    pinMode(s_cfg.pinMotor, INPUT);

    Serial.println("Tacho inicializado (ADC y conversión RPM listos).");
}

void Tacho_update()
{
    // Leer ADC
    uint16_t raw_rotor = analogRead(s_cfg.pinRotor);
    //Serial.print("Tacho_rotor -> DAC = ");
    //Serial.println(raw_rotor); 
    uint16_t raw_motor = analogRead(s_cfg.pinMotor);
    //Serial.print("Tacho_m.p -> DAC = ");
    //Serial.println(raw_motor); 

    // Convertir a voltaje
    float volts_rotor = adcToVolts(raw_rotor);
    //Serial.print("Tacho_rotor -> volts = ");
    //Serial.println(volts_rotor);  
    float volts_motor = adcToVolts(raw_motor);
    //Serial.print("Tacho_motor -> volts = ");
    //Serial.println(volts_motor);  

    // Obtener voltajes reales
    float real_volts_rotor = voltsAdapter(volts_rotor);
    float real_volts_motor = voltsAdapter(volts_motor);
    
    if ((real_volts_rotor < 0.25) && (real_volts_rotor > 0)) {
        real_volts_rotor = 0;
    }

    if ((real_volts_motor < 0.25) && (real_volts_motor > 0)) {
        real_volts_motor = 0;
    }
    /*
    Serial.print("Tacho_rotor -> real_volts = ");
    Serial.println(real_volts_rotor); 
    Serial.print("Tacho_m.p -> real_volts = ");
    Serial.println(real_volts_motor);
    */
    // Convertir a RPM
    float rpm_rotor = voltsToRpm(real_volts_rotor);
    float rpm_motor = voltsToRpm(real_volts_motor);

    // Mostrar en LVGL
    char buf[32];

    snprintf(buf, sizeof(buf), "        V.rotor = %.0f rpm", rpm_rotor);
    lv_label_set_text(ui_V_rotor_1, buf);

    snprintf(buf, sizeof(buf), "        V.mp = %.0f rpm", rpm_motor);
    lv_label_set_text(ui_V_motor_principal_1, buf);

    snprintf(buf, sizeof(buf), "        V.rotor = %.0f rpm", rpm_rotor);
    lv_label_set_text(ui_V_rotor_2, buf);

    snprintf(buf, sizeof(buf), "        V.mp = %.0f rpm", rpm_motor);
    lv_label_set_text(ui_V_motor_principal_2, buf);
}
