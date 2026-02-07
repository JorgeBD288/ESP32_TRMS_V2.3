/* Esta librería, junto con su correspondiente "Tacho.cpp" define las funciones que leen los voltajes que 
generan cada cuno de los tacogeneradores acoplados a sus motores correspondientes, y obtiene la velocidad
a la que están girando los mismos a partir de estos */

// Tacho.cpp
#include "Tacho.h"
#include "MotorControl.h"   // Para comprobar cambios en los DAC antes de publicar promedios

// Almacenamos configuración global del módulo
static TachoConfig s_cfg;

/**
 * @brief 
 * Convierte valor ADC a voltaje.
 * @note
  Toma un valor bruto del ADC (0..4095)
  y lo convierte a voltaje (0..3.3 V).
  Utiliza una resolución de 12 bits típica del ESP32.
 * @param raw 
 * @return float 
 */
//Convierte valor ADC a voltaje en ESP32
 
static float adcToVolts(uint16_t raw)
{
    return (raw / 4095.0f) * 3.3f; // ADC 12 bits → 0..3.3 V
}

/**
 * @brief 
 * Convierte voltaje adaptado a voltaje real.
 * @note
  Toma un voltaje adaptado leído por el ESP32
  y lo convierte al voltaje real generado por el tacómetro.
  Según el circuito de adaptación utilizado.
 * @param volts
 * @return float
 * /


/*El voltaje que recibe el ESP32 es un voltaje adaptado después de pasar por un circuito de adaptación. 
Esta función permite obtener el valor de voltaje real, generado por el tacómetro
*/
static float voltsAdapter(float volts)
{
    return (2*(1.65-volts));
}

/**
 * @brief 
 * Convierte voltaje a RPM.
 * @note
  Toma un voltaje real generado por el tacómetro
*/

//Convierte voltios → RPM según la curva del tacómetro

static float voltsToRpm(float volts)
{
    if (s_cfg.voltsPer1000RPM <= 0.0f) return 0.0f;

    return (volts / s_cfg.voltsPer1000RPM) * 1000.0f;
}

/**
 * @brief
 * Acumulador de promedio con rechazo de saltos.
 * @note
 * - Se acumulan 10 lecturas "válidas" y se muestra su promedio.
 * - Se descartan saltos > 1000 rpm si ocurren en menos de 0.1 s (100 ms)
 *   respecto a la última lectura aceptada.
 * - Antes de publicar un nuevo promedio (actualizar el valor mostrado),
 *   se comprueba si han cambiado las salidas DAC del control de motores:
 *   si NO han cambiado, no se actualiza el valor mostrado.
 */
struct RpmAvgState {
    float sum = 0.0f;
    uint8_t count = 0;

    bool hasLast = false;
    float lastAccepted = 0.0f;
    uint32_t lastAcceptedMs = 0;

    float lastOutput = 0.0f; // último valor mostrado (por si aún no hay 10)

    // Para saber si hubo cambios en control (DAC) desde el último promedio publicado
    uint32_t lastPublishedSeq = 0;
    bool hasPublishedSeq = false;
};

// Estados del promedio (uno por tacómetro)
static RpmAvgState s_rotorAvg;
static RpmAvgState s_motorAvg;

static inline float absf(float x) { return (x < 0.0f) ? -x : x; }

/**
 * @brief
 * Mete una muestra al acumulador y devuelve el valor a mostrar.
 * @note
 * - Si aún no hay 10 muestras válidas, devuelve el último valor mostrado (hold).
 * - Si se completa un grupo de 10 muestras válidas, calcula el promedio.
 * - Solo "publica" ese promedio (actualiza lastOutput) si se detecta
 *   un cambio en el control de motores (DAC seq).
 */
static float pushAvgWithSpikeRejectAndDacGate(float sampleRpm, RpmAvgState &st)
{
    const uint32_t now = millis();

    // Rechazo de saltos: >300 rpm en menos de 200 ms
    if (st.hasLast) {
        const float drpm = absf(sampleRpm - st.lastAccepted);
        const uint32_t dt = now - st.lastAcceptedMs;

        if (dt < 100 && drpm > 300.0f) {
            // Descarta directamente esta muestra
            return st.lastOutput;
        }
    }

    // Acepta muestra
    st.hasLast = true;
    st.lastAccepted = sampleRpm;
    st.lastAcceptedMs = now;

    st.sum += sampleRpm;
    st.count++;

    // Cuando llegamos a 10, calculamos promedio
    if (st.count >= 5) {
        const float avg = st.sum / 10.0f;

        // Consultar si ha habido cambios en los DAC desde el último promedio publicado
        const uint32_t seq = MotorControl_getDacUpdateSeq();

        if (!st.hasPublishedSeq) {
            // Primera vez: publica sin exigir cambio
            st.lastOutput = avg;
            st.lastPublishedSeq = seq;
            st.hasPublishedSeq = true;
        } else {
            // Solo publica si cambió el control (DAC)
            if (seq != st.lastPublishedSeq) {
                st.lastOutput = avg;
                st.lastPublishedSeq = seq;
            }
            // Si NO cambió, NO actualiza lastOutput (se queda el anterior)
        }

        // Reiniciar acumulación (siempre) para que el siguiente promedio
        // sea de las 10 muestras más recientes.
        st.sum = 0.0f;
        st.count = 0;
    }

    return st.lastOutput;
}

/**
 * @brief 
 * Inicializa el módulo Tacho.
 * @note
 * Configura los pines analógicos
 * para leer los tacómetros
 * y almacena la configuración.
 * @param config 
 */

void Tacho_begin(const TachoConfig &config)
{
    s_cfg = config;

    // Configurar pines como entrada analógica
    pinMode(s_cfg.pinRotor, INPUT);
    pinMode(s_cfg.pinMotor, INPUT);

    Serial.println("Tacho inicializado (ADC y conversión RPM listos).");
}

/**
 * @brief 
 * Actualiza las lecturas de los tacómetros.
 * @note
 * Lee los valores ADC de los tacómetros,
 * convierte a voltaje real y luego a RPM,
 * promedia 10 lecturas válidas (con rechazo de saltos),
 * y actualiza las etiquetas LVGL correspondientes.
 * Antes de actualizar el valor mostrado (publicar promedio),
 * se comprueba si han cambiado los valores escritos a los DAC
 * por el control de motores.
 */

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

    // Convertir a RPM (crudo)
    float rpm_rotor_raw = voltsToRpm(real_volts_rotor);
    float rpm_motor_raw = voltsToRpm(real_volts_motor);

    // Promediar 10 lecturas válidas con rechazo de saltos y "gate" por cambios de DAC
    float rpm_rotor = pushAvgWithSpikeRejectAndDacGate(rpm_rotor_raw, s_rotorAvg);
    float rpm_motor = pushAvgWithSpikeRejectAndDacGate(rpm_motor_raw, s_motorAvg);

    // Mostrar en LVGL (bloque EXACTO que pediste)
    char buf[32];

    snprintf(buf, sizeof(buf), "        V.mp = %.0f rpm", rpm_rotor);
    lv_label_set_text(ui_V_motor_principal_1, buf);

    snprintf(buf, sizeof(buf), "        V.rotor = %.0f rpm", rpm_motor);
    lv_label_set_text(ui_V_rotor_1, buf);

    snprintf(buf, sizeof(buf), "        V.mp = %.0f rpm", rpm_rotor);
    lv_label_set_text(ui_V_motor_principal_2, buf);

    snprintf(buf, sizeof(buf), "        V.rotor = %.0f rpm", rpm_motor);
    lv_label_set_text(ui_V_rotor_2, buf);
}
