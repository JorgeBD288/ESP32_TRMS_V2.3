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

// Funciones auxiliares
//----------------------------------------------------
void motor_girar_a(int16_t angle) {
    lv_img_set_angle(ui_EstructuraMotores, angle); // lv_img_set_angle usa décimas de grado
}

void bloquear_slider(lv_obj_t * slider) {
    lv_obj_add_state(slider, LV_STATE_DISABLED); // Bloquear slider
}

void desbloquear_slider(lv_obj_t * slider) {
    lv_obj_clear_state(slider, LV_STATE_DISABLED); // Desbloquear slider
}

void mostrar_flecha(lv_obj_t * flecha) {
    lv_obj_clear_flag(flecha, LV_OBJ_FLAG_HIDDEN);
}

void ocultar_flecha(lv_obj_t * flecha) {
    lv_obj_add_flag(flecha, LV_OBJ_FLAG_HIDDEN);
}


void MotorControl_begin(uint8_t dacPinG1, uint8_t dacPinG2) {
    s_dacPinG1 = dacPinG1;
    s_dacPinG2 = dacPinG2;
}

/**
 * @brief Convierte un valor de -100..100 en valor DAC (0..255) absoluto.
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

static float computeVoltFromRegister(int regValue) {
    // Magnitud 0..100
    int mag = regValue;
    if (mag > 100) mag = 100;
    if (mag < -100) mag = -100;

    float volt = (mag + 100)*(MAX_VOLTAGE/200); 

    return volt;
}

static float VinTRMSFromRegister(int regValue) {
    // Magnitud 0..100
    int mag = regValue;
    if (mag > 100) mag = 100;
    if (mag < -100) mag = -100;

    float volt = (mag*5.0f)/200.0f;

    return volt;
}



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
    dacWrite(s_dacPinG1, dac_mp);  // Motor / G1
    Serial.print("DAC_motor_Principal = ");
    Serial.println(dac_mp);
    Serial.print("Volt_motor_Principal = ");
    Serial.println(volt_mp);

    dacWrite(s_dacPinG2, dac_rdc);   // Rotor / G2
    Serial.print("DAC_motor_Principal = ");
    Serial.println(dac_mp);
    Serial.print("Volt_rotor_de_cola = ");
    Serial.println(volt_rdc);

}
