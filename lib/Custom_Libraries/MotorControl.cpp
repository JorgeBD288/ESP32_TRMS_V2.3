/*Esta librería, junto con su correspondiente "MotorControl.h" define las funciones que geneneran 
los voltajes que controlan los motores, así como los registros que estas necesitan pata funcionar 
y su control mediante sliders que se muestran en la interfaz gráfica*/

// MotorControl.cpp
#include "MotorControl.h"

// Constantes para el DAC
static const float MAX_VOLTAGE   = 3.3f;
static const int   DAC_MAX_VALUE = 255;

// Pines DAC guardados internamente
static uint8_t s_dacPinG1 = 25;
static uint8_t s_dacPinG2 = 26;

// Últimos valores escritos realmente a DAC
static uint8_t s_lastDacG1 = 0;
static uint8_t s_lastDacG2 = 0;

// Contador de cambios (sube cuando cambia G1 o G2)
static uint32_t s_dacUpdateSeq = 0;

// Funciones auxiliares

/**
 * @brief   
 * Gira la imagen del motor a un ángulo específico.
 * @note
 * Utiliza la función lv_img_set_angle
 * para rotar la imagen del motor
 * a el ángulo dado (en décimas de grado).
 * Esto simula el giro del motor en la interfaz de usuario.
 * @param angle 
 */

void motor_girar_a(int16_t angle) {
    lv_img_set_angle(ui_EstructuraMotores, angle); // lv_img_set_angle usa décimas de grado
}

/**
 * @brief 
 * Bloquea un slider deshabilitándolo.
 * @note
 * Añade el estado LV_STATE_DISABLED
 * al objeto slider proporcionado,
 * impidiendo la interacción del usuario.
 * @param slider 
 */

void bloquear_slider(lv_obj_t * slider) {
    lv_obj_add_state(slider, LV_STATE_DISABLED); // Bloquear slider
}

/**
 * @brief 
 * Desbloquea un slider habilitándolo.
 * @note
 * Elimina el estado LV_STATE_DISABLED
 * del objeto slider proporcionado,
 * permitiendo la interacción del usuario.
 * @param slider 
 */

void desbloquear_slider(lv_obj_t * slider) {
    lv_obj_clear_state(slider, LV_STATE_DISABLED); // Desbloquear slider
}

/**
 * @brief 
 * Muestra una flecha en la interfaz.
 * @note
 * Elimina la bandera LV_OBJ_FLAG_HIDDEN
 * del objeto de flecha proporcionado,
 * haciéndolo visible en la interfaz de usuario.
 * @param flecha 
 */

void mostrar_flecha(lv_obj_t * flecha) {
    lv_obj_clear_flag(flecha, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 
 * Oculta una flecha en la interfaz.
 * @note
 * Añade la bandera LV_OBJ_FLAG_HIDDEN
 * al objeto de flecha proporcionado,
 * haciéndolo invisible en la interfaz de usuario.
 * @param flecha 
 */

void ocultar_flecha(lv_obj_t * flecha) {
    lv_obj_add_flag(flecha, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 
 * Inicializa el control de los motores.
 * @note
 * Almacena los pines DAC utilizados
 * para controlar los motores G1 y G2.
 * @param dacPinG1 
 * @param dacPinG2 
 */

void MotorControl_begin(uint8_t dacPinG1, uint8_t dacPinG2) {
    s_dacPinG1 = dacPinG1;
    s_dacPinG2 = dacPinG2;
}

/**
 * @brief 
 * Convierte un valor de registro en un valor DAC.
 * @note
  Toma un valor de registro en el rango [-100, 100]
  y lo convierte en un valor DAC correspondiente
  en el rango [0, 255], considerando el voltaje máximo. 
 * @param regValue 
 * @return float 
 */

static float computeDacFromRegister(int regValue) {
    // Magnitud 0..100
    int mag = regValue;
    if (mag > 100) mag = 100;
    if (mag < -100) mag = -100;

    float volt = (mag + 100)*(MAX_VOLTAGE/200); 
    
    // voltaje -> DAC
    float dacVal = (volt / MAX_VOLTAGE * DAC_MAX_VALUE);
    if (dacVal < 0) dacVal = 0;
    if (dacVal > DAC_MAX_VALUE) dacVal = DAC_MAX_VALUE;

    return dacVal;
}

/**
 * @brief 
 * Convierte un valor de registro en un valor DAC.
 * @note
  Toma un valor de registro en el rango [-100, 100]
  y lo convierte en un valor DAC correspondiente
  en el rango [0, 255], considerando el voltaje máximo. 
 * @param regValue 
 * @return float 
 */

static float computeVoltFromRegister(int regValue) {
    // Magnitud 0..100
    int mag = regValue;
    if (mag > 100) mag = 100;
    if (mag < -100) mag = -100;

    float volt = (mag + 100)*(MAX_VOLTAGE/200); 

    return volt;
}

/**
 * @brief 
 * Convierte un valor de registro en un voltaje de entrada TRMS.
 * @note
  Toma un valor de registro en el rango [-100, 100]
  y lo convierte en un voltaje de entrada TRMS
  en el rango adecuado. 
 * @param regValue 
 * @return float 
 */

static float VinTRMSFromRegister(int regValue) {
    // Magnitud 0..100
    int mag = regValue;
    if (mag > 100) mag = 100;
    if (mag < -100) mag = -100;

    float volt = (mag*5.0f)/200.0f;

    return volt;
}

/**
 * @brief 
 * Devuelve los últimos valores escritos a los DAC (0..255).
 * @note
 * Esta función permite que otros módulos (por ejemplo, el de lectura de tacómetros)
 * puedan consultar el último valor que se ha enviado a los DAC de control.
 * @param dacG1 Referencia donde se devuelve el último valor escrito en G1
 * @param dacG2 Referencia donde se devuelve el último valor escrito en G2
 */
void MotorControl_getLastDacValues(uint8_t &dacG1, uint8_t &dacG2)
{
    dacG1 = s_lastDacG1;
    dacG2 = s_lastDacG2;
}

/**
 * @brief
 * Devuelve un contador que aumenta cada vez que cambia algún valor DAC escrito.
 * @note
 * Útil para que otros módulos detecten si ha habido cambios de control
 * desde la última comprobación (sin necesidad de comparar valores).
 * @return uint32_t Contador de actualizaciones de DAC
 */
uint32_t MotorControl_getDacUpdateSeq()
{
    return s_dacUpdateSeq;
}

/**
 * @brief 
 * Actualiza el control de los motores basado en los registros.
 * @note
 * Toma los valores de los registros de motor principal
 * y rotor de cola, limita sus rangos,
 * actualiza las animaciones de flechas
 * y escribe los valores correspondientes en los DACs.
 * Además:
 *  - Guarda los últimos valores escritos en DAC (G1 y G2)
 *  - Incrementa un contador interno cuando detecta cambios de salida
 *    para que otros módulos puedan saber si ha habido cambios.
 * @param Registro_MP 
 * @param Registro_RDC 
 */

void MotorControl_update(int &Registro_MP, int &Registro_RDC) {

    // 1) Limitar registros al rango [-100, 100]
    if (Registro_MP > 100) Registro_MP = 100;
    if (Registro_MP < -100) Registro_MP = -100;
    if (Registro_RDC  > 100) Registro_RDC  = 100;
    if (Registro_RDC  < -100) Registro_RDC  = -100;

    // 2) Actualizar valores de los sliders en caso de que estos no se correspondan con los valores de
    //los registros, al haber sido establecidos por control IR

    //lv_slider_set_value(ui_MotorPrincipal, Registro_MP, LV_ANIM_OFF);
    //lv_slider_set_value(ui_RotorDeCola, Registro_RDC, LV_ANIM_OFF);

    // 3) Actualizar animaciones
    if (Registro_RDC > 0) {
        motor_girar_a(-120); // Giro hacia "arriba"
        mostrar_flecha(ui_FlechaVerdeCurva);
        ocultar_flecha(ui_FlechaVerdeCurvaGirada);
    }
    else if (Registro_RDC < 0) {
        motor_girar_a(120); // Giro hacia abajo
        ocultar_flecha(ui_FlechaVerdeCurva);
        mostrar_flecha(ui_FlechaVerdeCurvaGirada);
    }
    else if (Registro_RDC == 0) { // Centro
        motor_girar_a(0);
        ocultar_flecha(ui_FlechaVerdeCurva);
        ocultar_flecha(ui_FlechaVerdeCurvaGirada);
    }

    if (Registro_MP > 0) {
        motor_girar_a(-120); // Giro hacia "arriba"
        mostrar_flecha(ui_FlechaVerdeRecta);
        ocultar_flecha(ui_FlechaVerdeRectaGirada);
    }
    else if (Registro_MP < 0) {
        motor_girar_a(120); // Giro hacia abajo
        ocultar_flecha(ui_FlechaVerdeRecta);
        mostrar_flecha(ui_FlechaVerdeRectaGirada);
    }
    else if (Registro_MP == 0) { // Centro
        motor_girar_a(0);
        ocultar_flecha(ui_FlechaVerdeRecta);
        ocultar_flecha(ui_FlechaVerdeRectaGirada);
    }

    // 4) Calcular valores DAC a partir de los registros
    int dac_mp = computeDacFromRegister(Registro_MP);
    float volt_mp = computeVoltFromRegister(Registro_MP);

    int dac_rdc  = computeDacFromRegister(Registro_RDC);
    float volt_rdc  = computeVoltFromRegister(Registro_RDC);

    /* 5) Actualizar labels de Vin. Estas labels no indican directamente el voltaje que sale de los 
    terminales del ESP32, sino lo que va a ver el TRMS en sus entradas para los motores. Es decir,
    las señales de control, una vez pasadas por el circuito de adaptación correspondiente, 
    y por el primer circuito de adaptación interno del propio TRMS, que elevan el rango de estas, 
    de (0-3.3)V, a (0-5)V */
    
    float VinMP = VinTRMSFromRegister(Registro_MP);
    float VinRDC = VinTRMSFromRegister(Registro_RDC);

    char buf[32];
    snprintf(buf, sizeof(buf), "Vin = %.2f V", VinMP);
    lv_label_set_text(ui_VinMP, buf);

    snprintf(buf, sizeof(buf), "Vin = %.2f V", VinRDC);
    lv_label_set_text(ui_VinRDC, buf);

    // 6) Escribir a los DACs
    // Convertimos a uint8_t porque dacWrite usa 0..255
    uint8_t outG1 = (uint8_t)dac_mp;
    uint8_t outG2 = (uint8_t)dac_rdc;

    // Si cambió cualquier salida, incrementamos el contador de cambios
    if (outG1 != s_lastDacG1 || outG2 != s_lastDacG2) {
        s_dacUpdateSeq++;
        s_lastDacG1 = outG1;
        s_lastDacG2 = outG2;
    }

    dacWrite(s_dacPinG1, outG1);  // Motor / G1

    //Serial.print("DAC_motor_Principal = ");
    //Serial.println(Registro_MP);
    //Serial.println(outG1);
    //Serial.print("Volt_motor_Principal = ");
    //Serial.println(volt_mp);

    dacWrite(s_dacPinG2, outG2);   // Rotor / G2
/*
    Serial.print("DAC_rotor_de_cola = ");
    Serial.println(outG2);
    Serial.print("Volt_rotor_de_cola = ");
    Serial.println(volt_rdc);
*/


}
