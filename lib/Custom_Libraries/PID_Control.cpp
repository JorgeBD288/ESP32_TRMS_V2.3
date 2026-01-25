#include "PID_Control.h"
#include "Encoders.h"
#include "PID_Parameters.h"
#include "MotorControl.h"

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251994329577f   // pi/180
#endif

extern int Registro_MP;
extern int Registro_RDC;

// Estado global del PID-4
static PID4Params s_pid4_params;
static PID4State  s_pid4_state;
static PID_CURR   s_pidCurrCopy;

static bool  s_pid4_enabled = false;
static float s_refVertDeg   = 0.0f; // ref display (grados)
static float s_refHorDeg    = 0.0f; // ref display (grados)

/**
 * @brief 
 * Obtiene los ángulos actuales del TRMS en grados.
 * @note
 * Utiliza la librería de encoders
 * para leer los ángulos vertical y horizontal
 * medidos por los encoders.
 * @param vertDeg 
 * @param horzDeg 
 */

// Lee ángulos reales del TRMS EN GRADOS usando la librería de encoders
static void TRMS_GetAnglesDeg(float &vertDeg, float &horzDeg)
{
    EncoderAngles ang = Encoders_readAngles();
    vertDeg = ang.verticalDeg;
    horzDeg = ang.horizontalDeg;
}

/**
 * @brief 
 * Pone a cero el integrador y la memoria de derivada
 * de un controlador PID simple.
 * @note
 * Implementa el reseteo del estado interno
 * de un PID individual.
 * @param state
 * Estado del PID a resetear.
 * @return void
 */

void PID_Reset(PIDState &state)
{
    state.integrator = 0.0f;
    state.prevInput  = 0.0f;
    state.firstRun   = true;
}

/**
 * @brief 
 * Actualiza un controlador PID simple.
 * @note
 * Implementa la ecuación del PID con integrador
 * y derivada, incluyendo saturación del integrador.
 * @param p
 * Parámetros del PID.
 * @param s
 * Estado del PID.
 * @param u
 * Entrada del PID (error).
 * @param dt
 * Intervalo de tiempo desde la última actualización (segundos).
 * @return float
 * Salida del PID.
 */

/**
 * Implementa:
 *  out = Kp*u + I + Kd*du/dt
 *
 * donde:
 *  I[k] = saturación( I[k-1] + Ki * u[k] * dt, ±I_sat )
 */
float PID_Update(const PIDParams &p,
                 PIDState        &s,
                 float            u,
                 float            dt)
{
    // Protección básica
    if (dt <= 0.0f) dt = 1e-3f;

    // ---- Derivada du/dt ----
    float du_dt = 0.0f;
    if (!s.firstRun) {
        du_dt = (u - s.prevInput) / dt;
    } else {
        du_dt = 0.0f;
        s.firstRun = false;
    }
    s.prevInput = u;

    // ---- Integrador con saturación ----
    // En Simulink: Ki -> integrador -> saturador Ihhsat
    s.integrator += p.Ki * u * dt;

    // Saturación simétrica ±I_sat
    if (p.I_sat > 0.0f) {
        if (s.integrator >  p.I_sat) s.integrator =  p.I_sat;
        if (s.integrator < -p.I_sat) s.integrator = -p.I_sat;
    }

    // ---- Salida total: Kp*u + I + Kd*du/dt ----
    float outP = p.Kp * u;
    float outI = s.integrator;
    float outD = p.Kd * du_dt;

    float out = outP + outI + outD;

    return out;
}

// =====================================================
// Bloque PID-4 (MIMO 2x2)
// =====================================================

/**
 * @brief 
 * Carga los parámetros del PID-4 desde la estructura PID_CURR.
 * @note
 * Copia los parámetros de la estructura PID_CURR
 * a la estructura interna PID4Params utilizada
 * por el controlador PID-4.
 * @param c
 * Estructura PID_CURR con los parámetros actuales.
 */

void PID4_LoadFromCurr(const PID_CURR &c)
{
    s_pidCurrCopy = c;

    s_pid4_params.vv = { c.KpvvCurr, c.KivvCurr, c.KdvvCurr, c.IsatvvCurr };
    s_pid4_params.hv = { c.KphvCurr, c.KihvCurr, c.KdhvCurr, c.IsathvCurr };
    s_pid4_params.vh = { c.KpvhCurr, c.KivhCurr, c.KdvhCurr, c.IsatvhCurr };
    s_pid4_params.hh = { c.KphhCurr, c.KihhCurr, c.KdhhCurr, c.IsathhCurr };

    s_pid4_params.Uv_max = c.UvmaxCurr;
    s_pid4_params.Uh_max = c.UhmaxCurr;

    PID4_Reset(s_pid4_state);
}

/**
 * @brief 
 * Establece las referencias de posición para el PID-4.
 * @note
 * Actualiza las referencias vertical y horizontal
 * en grados que el PID-4 debe alcanzar.
 * @param refVertDeg 
 * Referencia vertical en grados.
 * @param refHorDeg 
 * Referencia horizontal en grados.
 */

void PID4_SetReferences(float refVertDeg, float refHorDeg)
{
    s_refVertDeg = refVertDeg;
    s_refHorDeg  = refHorDeg;
}

/**
 * @brief 
 * Habilita o deshabilita el controlador PID-4.
 * @note
 * Cuando se habilita, el PID-4 se resetea.
 * @param enable 
 * true para habilitar, false para deshabilitar.
 */

void PID4_SetEnabled(bool enable)
{
    s_pid4_enabled = enable;
    if (enable) {
        PID4_Reset(s_pid4_state);
    }
}

/**
 * @brief 
 * Consulta si el PID-4 está habilitado.
 * @note
 * Devuelve el estado actual de habilitación
 * del controlador PID-4.
 * @return true 
 * Si el PID-4 está habilitado.
 * @return false 
 * Si el PID-4 está deshabilitado.
 */

bool PID4_IsEnabled()
{
    return s_pid4_enabled;
}

/**
 * @brief 
 * Convierte una señal de control U
 * a un valor de registro para el motor.
 * @note
 * Normaliza la señal U respecto a Umax,
 * la limita al rango [-1, 1],
 * y la escala al rango de registro [-100, 100].
 * @param U 
 * Señal de control.
 * @param Umax 
 * Valor máximo absoluto permitido para U.
 * @return int 
 * Valor de registro correspondiente en el rango [-100, 100].
 */

static int U_to_Register(float U, float Umax)
{
    if (Umax <= 0.0f) return 0;
    float norm = U / Umax;       // -1..1 idealmente
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;
    int reg = (int)(norm * 100.0f);
    if (reg > 100)  reg = 100;
    if (reg < -100) reg = -100;
    return reg;
}

/**
 * @brief 
 * Resetea el estado de un controlador PID-4.
 * @note
 * Pone a cero los integradores y memorias de derivada
 * de los cuatro controladores PID individuales
 * que componen el PID-4.
 * @param s
 * Estado del PID-4 a resetear.
 */

void PID4_Reset(PID4State &s)
{
    PID_Reset(s.hh);
    PID_Reset(s.hv);
    PID_Reset(s.vh);
    PID_Reset(s.vv);
}

/**
 * @brief 
 * Actualiza el controlador PID-4.
 * @note
 * Ejecuta los cuatro controladores PID individuales,
 * suma sus salidas según la configuración MIMO,
 * y aplica las saturaciones de salida.
 * @param p
 * Parámetros del PID-4.
 * @param s
 * Estado del PID-4.
 * @param in1
 * Entrada 1 (error horizontal).
 * @param in2
 * Entrada 2 (error vertical).
 * @param dt
 * Intervalo de tiempo desde la última actualización (segundos).
 * @param out1
 * Salida 1 (control motor principal).
 * @param out2
 * Salida 2 (control rotor de cola).
 */

void PID4_Update(const PID4Params &p,
                 PID4State        &s,
                 float             in1,
                 float             in2,
                 float             dt,
                 float            &out1,
                 float            &out2)
{
    // in1 = error horizontal (eh)
    // in2 = error vertical  (ev)

    // PIDhh: in1 -> out1
    float u_hh = PID_Update(p.hh, s.hh, in1, dt);

    // PIDhv: in1 -> out2
    float u_hv = PID_Update(p.hv, s.hv, in1, dt);

    // PIDvh: in2 -> out1
    float u_vh = PID_Update(p.vh, s.vh, in2, dt);

    // PIDvv: in2 -> out2
    float u_vv = PID_Update(p.vv, s.vv, in2, dt);

    // Sumas MIMO
    float Uh = u_hh + u_vh;   // out_1
    float Uv = u_hv + u_vv;   // out_2

    // Saturaciones Uhmax / Uvmax (simétricas ±)
    if (p.Uh_max > 0.0f) {
        if (Uh >  p.Uh_max) Uh =  p.Uh_max;
        if (Uh < -p.Uh_max) Uh = -p.Uh_max;
    }
    if (p.Uv_max > 0.0f) {
        if (Uv >  p.Uv_max) Uv =  p.Uv_max;
        if (Uv < -p.Uv_max) Uv = -p.Uv_max;
    }

    out1 = Uh;
    out2 = Uv;
}

/**
 * @brief 
 * Ejecuta un paso del controlador PID-4.
 * @note
 * Lee los ángulos medidos,
 * calcula los errores respecto a las referencias,
 * actualiza el PID-4,
 * convierte las salidas a registros de motor,
 * y aplica los valores a los DACs/sliders.
 * @param dt
 * Intervalo de tiempo desde la última actualización (segundos).
 */

void PID4_Step(float dt)
{
    if (!s_pid4_enabled) return;
    if (dt <= 0.0f) dt = 1e-3f;

    // 1) Leer ángulos medidos del TRMS EN GRADOS
    float measVertDeg, measHorDeg;
    TRMS_GetAnglesDeg(measVertDeg, measHorDeg);

    // 2) Convertir referencias y medidas a RADIANES
    float refV_rad  = s_refVertDeg * DEG_TO_RAD;
    float refH_rad  = s_refHorDeg  * DEG_TO_RAD;
    float measV_rad = measVertDeg  * DEG_TO_RAD;
    float measH_rad = measHorDeg   * DEG_TO_RAD;

    // 3) Errores en radianes (como en el modelo original)
    float ev = refV_rad - measV_rad;
    float eh = refH_rad - measH_rad;

    // 4) Ejecutar PID-4
    float Uh, Uv;
    PID4_Update(s_pid4_params, s_pid4_state, eh, ev, dt, Uh, Uv);

    // 5) Convertir Uh/Uv a registros de motor [-100..100]
    Registro_RDC = U_to_Register(Uh, s_pid4_params.Uh_max);  // rotor de cola (horizontal)
    Registro_MP  = U_to_Register(Uv, s_pid4_params.Uv_max);  // motor principal (vertical)

    // 6) Aplicar a los DACs / sliders
    MotorControl_update(Registro_MP, Registro_RDC);
}

