/*Esta librería, junto con su correspondiente "MotorControl.cpp" define las funciones que geneneran 
los voltajes que controlan los motores, así como los registros que estas necesitan pata funcionar 
y su control mediante sliders que se muestran en la interfaz gráfica*/

// MotorControl.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"    // Para usar ui_MotorAscendente, ui_RotorDerecha, etc.

/*
// Estructura para devolver las banderas de estado del motor
struct MotorFlags {
    bool FMA;  // Motor Ascendente
    bool FMD;  // Motor Descendente
    bool FRD;  // Rotor Derecha
    bool FRI;  // Rotor Izquierda
}; */

/**
 * @brief Inicializa el módulo de control de motores.
 *
 * @param dacPinG1  Pin DAC para el motor principal (G1_DAC_PIN)
 * @param dacPinG2  Pin DAC para el rotor (G2_DAC_PIN)
 */
void MotorControl_begin(uint8_t dacPinG1, uint8_t dacPinG2);

/**
 * @brief Actualiza sliders, banderas y DACs a partir de los registros.
 *
 * - Limita Registro_MD y Registro_R al rango [-100, 100].
 * - Actualiza los sliders de la UI (ui_MotorAscendente, etc.).
 * - Escribe a los DACs los voltajes correspondientes.
 *
 * @param Registro_MP  Referencia al registro del motor principal (-100..100)
 * @param Registro_RDC   Referencia al registro del rotor (-100..100)
 * @return MotorFlags  Banderas de estado (direcciones activas)
 */
void MotorControl_update(int &Registro_MP, int &Registro_RDC);

//Funciones auxiliares
void motor_girar_a(int16_t angle);
void bloquear_slider(lv_obj_t * slider);
void desbloquear_slider(lv_obj_t * slider);
void mostrar_flecha(lv_obj_t * flecha);
void ocultar_flecha(lv_obj_t * flecha);