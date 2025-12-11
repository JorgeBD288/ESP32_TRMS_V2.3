#pragma once
#include <Arduino.h>
#include "PID_Parameters.h"
#include "MotorControl.h"

/**
 * Estructura de parámetros de un PID
 * (equivalente a un bloque de Simulink: Kp, Ki, Kd, Isat).
 */
struct PIDParams {
    float Kp;      // Kp* u
    float Ki;      // Ki* u -> integrado
    float Kd;      // Kd* du/dt
    float I_sat;   // saturación de la PARTE INTEGRAL (±I_sat)
};

/**
 * Estado interno del PID.
 * Un objeto de estos por cada PID (PIDvv, PIDvh, PIDhv, PIDhh).
 */
struct PIDState {
    float integrator;  // estado del integrador (ya incluye Ki)
    float prevInput;   // u[k-1]
    bool  firstRun;    // para inicializar derivada la primera vez
};

/**
 * Inicializa el estado del PID.
 * Llama a esto una vez al arrancar o cuando quieras resetearlo.
 */
void PID_Reset(PIDState &state);

/**
 * Calcula la salida del PID para una muestra.
 *
 * @param params  Ganancias Kp, Ki, Kd, I_sat
 * @param state   Estado interno del PID (integrador, memoria)
 * @param input   Entrada u (normalmente el error: ref - medida)
 * @param dt      Tiempo de muestreo [s]
 *
 * @return salida del PID (antes de Umax/Uhmax/Uvmax, eso va aparte)
 */
float PID_Update(const PIDParams &params,
                 PIDState        &state,
                 float            input,
                 float            dt);
// =====================================================
// Bloque PID-4 (MIMO 2x2) para el TRMS
// =====================================================

struct PID4Params {
    PIDParams hh;   // PIDhh: in_1 -> out_1
    PIDParams hv;   // PIDhv: in_1 -> out_2
    PIDParams vh;   // PIDvh: in_2 -> out_1
    PIDParams vv;   // PIDvv: in_2 -> out_2

    float Uh_max;   // saturación simétrica para out_1 (±Uh_max)
    float Uv_max;   // saturación simétrica para out_2 (±Uv_max)
};

struct PID4State {
    PIDState hh;
    PIDState hv;
    PIDState vh;
    PIDState vv;
};

/**
 * Resetea todos los integradores y memorias del bloque PID-4.
 */
void PID4_Reset(PID4State &state);

/**
 * Evalúa el bloque PID-4.
 *
 * @param params  Parámetros de los 4 PID + Uh_max / Uv_max
 * @param state   Estado interno de los 4 PID
 * @param in1     in_1 (error horizontal, por ejemplo)
 * @param in2     in_2 (error vertical, por ejemplo)
 * @param dt      Tiempo de muestreo [s]
 * @param out1    out_1 (Uh) → señal para motor horizontal
 * @param out2    out_2 (Uv) → señal para motor vertical
 */
void PID4_Update(const PID4Params &params,
                 PID4State        &state,
                 float             in1,
                 float             in2,
                 float             dt,
                 float            &out1,
                 float            &out2);

                 // ======================================
// Módulo global de control PID-4 (TRMS)
// ======================================

// Inicializa los parámetros del PID-4 a partir de g_pidCurr
void PID4_LoadFromCurr(const PID_CURR &curr);

// Establece referencias (procedentes del display) en GRADOS
void PID4_SetReferences(float refVertDeg, float refHorDeg);

// Habilita / deshabilita el control PID-4
void PID4_SetEnabled(bool enable);
bool PID4_IsEnabled();

// Ejecuta un paso de control:
//   dt  -> tiempo de muestreo [s]
// Internamente:
//   - lee los ángulos medidos (Encoders_readAngles)
//   - convierte ref y medida a radianes
//   - calcula errores
//   - llama a PID4_Update
//   - convierte Uh/Uv a Registro_MP / Registro_RDC
//   - llama a MotorControl_update(...)
void PID4_Step(float dt);