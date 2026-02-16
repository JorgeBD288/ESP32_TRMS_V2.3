/*  PID_Control.cpp
    Controlador PID para el TRMS (MIMO 2x2) + modos SISO (vertical-only / horizontal-only)

    ------------------------------------------------------------------------------------
    CONTEXTO
    ------------------------------------------------------------------------------------
    Este fichero implementa:
      - Un PID simple (PID_Update) con integrador y derivada
      - Un bloque PID-4 (MIMO 2x2) compuesto por 4 PIDs individuales:
            hh: eh -> Uh
            hv: eh -> Uv
            vh: ev -> Uh
            vv: ev -> Uv
        y dos salidas:
            Uh -> rotor de cola (horizontal)  -> Registro_RDC
            Uv -> motor principal (vertical) -> Registro_MP

    ------------------------------------------------------------------------------------
    CAMBIO IMPORTANTE (NUEVO)
    ------------------------------------------------------------------------------------
    Se añade un selector de modo para poder validar el sistema como SISO antes de usar MIMO:

      - PIDMode::MIMO_FULL       -> usa hh, hv, vh, vv (el 2x2 completo)
      - PIDMode::VERTICAL_ONLY   -> SOLO usa vv (ev -> Uv). El resto de PIDs se anulan (K=0) y se resetean.
                                  Además fuerza Registro_RDC = 0.
      - PIDMode::HORIZONTAL_ONLY -> SOLO usa hh (eh -> Uh). El resto de PIDs se anulan (K=0) y se resetean.
                                  Además fuerza Registro_MP = 0.

    ¿Por qué hay que anular PIDs y resetear estado?
      Porque aunque “no uses” una salida, si dejas integradores/derivadas vivos,
      se acumula memoria interna “fantasma” y luego reaparece al volver a MIMO.

    ------------------------------------------------------------------------------------
    NOTA
    ------------------------------------------------------------------------------------
    Este fichero asume que en PID_Control.h existen (o se han añadido):
      - struct PIDParams, PIDState
      - struct PID4Params { PIDParams vv, hv, vh, hh; float Uv_max, Uh_max; }
      - struct PID4State  { PIDState  vv, hv, vh, hh; }
      - struct PID_CURR con los parámetros actuales (Kp..Ki..Kd..Isat..Umax)
      - enum class PIDMode { MIMO_FULL, VERTICAL_ONLY, HORIZONTAL_ONLY };
      - void PID4_SetMode(PIDMode mode);

    Si aún no has añadido PIDMode y PID4_SetMode al .h, hazlo primero.
*/

#include "PID_Control.h"
#include "Encoders.h"
#include "PID_Parameters.h"
#include "MotorControl.h"
#include "ui.h"

#include <Arduino.h>
#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251994329577f   // pi/180
#endif

// Registros globales (salidas normalizadas -100..100)
extern int Registro_MP;   // Motor principal (vertical)
extern int Registro_RDC;  // Rotor de cola (horizontal)

static constexpr float V_BIAS_REL_K     = 0.1f;  // [reg/deg] cuánto se reduce el bias cuando err<0

enum class VertRefZone {
    ABOVE_REST,   // ref > -37
    BELOW_REST,   // ref < -36
    REST_BAND     // -37 <= ref <= -36
};

static VertRefZone s_vertZone = VertRefZone::ABOVE_REST;

// límites de banda
static constexpr float V_REST_LOW_DEG  = -37.0f;
static constexpr float V_REST_HIGH_DEG = -36.0f;

// -----------------------------------------------------
// Bias vertical (registro que "sostiene" el ángulo)
// -----------------------------------------------------
// Voltaje de offset (o bias) para mantener el sistema estable en un posición diferente a la de reposo
//PID4_SetVerticalEquilibriumRegister(0).
static int s_MP_eq = 53;   // valor experimental de equilibrio (ajustable)
//54 era el valor antiguo
// -----------------------------------------------------
// Parámetros para la lógica "Opción B" (unidireccional + modulación)
// -----------------------------------------------------
// Interpreta tu TRMS así:
// - Existe un "reposo mecánico" alrededor de V_REST_DEG (por ejemplo -36°).
// - Para consignas por encima de V_REST_DEG, preferimos NO invertir el motor: empujar para subir y soltar para bajar.
// - Para consignas por debajo de V_REST_DEG, preferimos lo contrario: empujar para bajar y soltar para subir.
// - EXCEPCIÓN: si el error es grande, permitimos inversión para recuperar.
static constexpr float V_REST_DEG     = -36.0f; // reposo mecánico aproximado
static constexpr float V_HYST_DEG     = 1.0f;   // histéresis para no estar cambiando de zona
static constexpr float V_BIG_ERR_DEG  = 20.0f;  // si |error| > esto, se permite invertir
static constexpr float V_ERR_DB_DEG   = 0.1f;  // deadband alrededor de consigna (evita caza)
static constexpr float V_ERR_DB_DEG_DOWN   = 1.0f;  // deadband alrededor de consigna (evita caza)
// -----------------------------------------------------
// Estado global del PID-4
// -----------------------------------------------------
static PID4Params s_pid4_params;
static PID4State  s_pid4_state;
static PID_CURR   s_pidCurrCopy;

static bool  s_pid4_enabled = false;
static float s_refVertDeg   = 0.0f; // ref display (grados)
static float s_refHorDeg    = 0.0f; // ref display (grados)

static uint32_t last = 0;

// Modo de control (nuevo)
static PIDMode s_pidMode = PIDMode::MIMO_FULL;

/**
 * @brief
 * Obtiene los ángulos actuales del TRMS en grados.
 * @note
 * Utiliza la librería de encoders para leer los ángulos vertical y horizontal medidos.
 */
static void TRMS_GetAnglesDeg(float &vertDeg, float &horzDeg)
{
    EncoderAngles ang = Encoders_readAngles();
    vertDeg = ang.verticalDeg;
    horzDeg = ang.horizontalDeg;
}

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * @brief
 * Pone a cero el integrador y la memoria de derivada de un controlador PID simple.
 * @note
 * Implementa el reseteo del estado interno de un PID individual.
 */
void PID_Reset(PIDState &state)
{
    state.integrator = 0.0f;
    state.prevInput  = 0.0f;
    state.firstRun   = true;
}

// =====================================================
// PID simple
// =====================================================

/**
 * @brief
 * Actualiza un controlador PID simple.
 * @note
 * Implementa la ecuación del PID con integrador y derivada, incluyendo saturación del integrador.
 *
 * out = Kp*u + I + Kd*du/dt
 * I[k] = saturación( I[k-1] + Ki * u[k] * dt, ±I_sat )
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
    s.integrator += p.Ki * u * dt;

    // Saturación simétrica ±I_sat
    if (p.I_sat > 0.0f) {
        if (s.integrator >  p.I_sat) s.integrator =  p.I_sat;
        if (s.integrator < -p.I_sat) s.integrator = -p.I_sat;
    }

    // ---- Salida total ----
    float outP = p.Kp * u;
    float outI = s.integrator;
    float outD = p.Kd * du_dt;

    return (outP + outI + outD);
}

// =====================================================
// Bloque PID-4 (MIMO 2x2)
// =====================================================

/**
 * @brief
 * Resetea el estado de un controlador PID-4.
 * @note
 * Pone a cero los integradores y memorias de derivada de los cuatro PIDs.
 */
void PID4_Reset(PID4State &s)
{
    PID_Reset(s.hh);
    PID_Reset(s.hv);
    PID_Reset(s.vh);
    PID_Reset(s.vv);
}

void PID4_ResetStates()
{
    PID4_Reset(s_pid4_state);
}

/**
 * @brief
 * Carga los parámetros del PID-4 desde la estructura PID_CURR.
 * @note
 * Copia los parámetros actuales a la estructura interna usada por el controlador.
 */
void PID4_LoadFromCurr(const PID_CURR &c)
{
    // Guardamos una copia por si luego queremos volver a MIMO_FULL
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
 * Actualiza las referencias vertical y horizontal en grados.
 */
void PID4_SetReferences(float refVertDeg, float refHorDeg)
{
    s_refVertDeg = refVertDeg;
    s_refHorDeg  = refHorDeg;

    // LATCH del modo vertical según la consigna (solo depende de referencia)
    if (s_refVertDeg > V_REST_LOW_DEG) {
        s_vertZone = VertRefZone::ABOVE_REST;   // ref > -37
    } else if (s_refVertDeg < V_REST_HIGH_DEG) {
        s_vertZone = VertRefZone::BELOW_REST;   // ref < -36
    } else {
        s_vertZone = VertRefZone::REST_BAND;    // -37..-36
    }
}

/**
 * @brief
 * Habilita o deshabilita el controlador PID-4.
 * @note
 * Cuando se habilita, se resetea el estado interno.
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
 */
bool PID4_IsEnabled()
{
    return s_pid4_enabled;
}

/**
 * @brief
 * Selecciona el modo de control.
 * @note
 * - MIMO_FULL:   restaura parámetros completos desde s_pidCurrCopy
 * - VERTICAL_ONLY: anula hh/hv/vh (K=0) y deja solo vv
 * - HORIZONTAL_ONLY: anula vv/hv/vh (K=0) y deja solo hh
 *
 * IMPORTANTE:
 *  - Resetea el estado interno para evitar memoria cruzada.
 */
void PID4_SetMode(PIDMode mode)
{
    s_pidMode = mode;

    // Reset completo siempre (evita integradores/derivadas “fantasma”)
    PID4_Reset(s_pid4_state);

    switch (mode) {
    case PIDMode::VERTICAL_ONLY:
        // Mantener solo PIDvv (ev -> Uv)
        s_pid4_params.hh = {0,0,0,0};
        s_pid4_params.hv = {0,0,0,0};
        s_pid4_params.vh = {0,0,0,0};
        // vv se mantiene como esté cargado (desde PID4_LoadFromCurr)
        break;

    case PIDMode::HORIZONTAL_ONLY:
        // Mantener solo PIDhh (eh -> Uh)
        s_pid4_params.vv = {0,0,0,0};
        s_pid4_params.hv = {0,0,0,0};
        s_pid4_params.vh = {0,0,0,0};
        // hh se mantiene como esté cargado
        break;

    case PIDMode::MIMO_FULL:
    default:
        // Restaurar parámetros completos
        PID4_LoadFromCurr(s_pidCurrCopy);
        break;
    }
}

/**
 * @brief
 * Convierte una señal de control U a un valor de registro para el motor.
 * @note
 * Normaliza U respecto a Umax, limita a [-1,1], y escala a [-100,100].
 */
static int U_to_Register(float U, float Umax)
{
    if (Umax <= 0.0f) return 0;

    float norm = U / Umax;       // ideal -1..1
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;

    int reg = (int)(norm * 100.0f);
    if (reg > 100)  reg = 100;
    if (reg < -100) reg = -100;
    return reg;
}

/**
 * @brief
 * Actualiza el controlador PID-4 (MIMO).
 * @note
 * in1 = error horizontal (eh)
 * in2 = error vertical  (ev)
 * out1 = Uh (horizontal)
 * out2 = Uv (vertical)
 */
void PID4_Update(const PID4Params &p,
                 PID4State        &s,
                 float             in1,
                 float             in2,
                 float             dt,
                 float            &out1,
                 float            &out2)
{
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

    // Saturaciones simétricas ±
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

// =====================================================
// Opción B (vertical): unidireccional + modulación de retorno (sin invertir salvo excepción)
// =====================================================

/**
 * @brief
 * Aplica la lógica "Opción B" para el eje vertical.
 *
 * @param deltaMP     Salida del PID convertida a registro (-100..100), SIN bias.
 * @param refV_deg    Consigna vertical en grados.
 * @param measV_deg   Medida vertical en grados.
 * @return int        Registro final vertical (-100..100), con bias incluido.
 *
 * @note
 * - Se define una frontera en V_REST_DEG (tu reposo mecánico).
 * - Si ref está por encima de V_REST_DEG (con histéresis), se prefiere no invertir:
 *       * se aplica empuje "hacia arriba" (deltaMP > 0) si ayuda,
 *       * si el PID pide lo contrario, se suelta a "base" para que el retorno lo haga la física.
 * - Si ref está por debajo de V_REST_DEG (con histéresis), se hace simétrico (preferir deltaMP < 0).
 * - Si el error es grande (|err| > V_BIG_ERR_DEG), se permite invertir para recuperar.
 * - Deadband cerca de consigna para evitar "cazar".
 */

// Versión actualizada:
// - Mantiene tu estructura (Opción 2)
// - Reintroduce deadband, pero NO vuelve a s_MP_eq fijo: mantiene/actualiza un targetBase
// - targetBase se actualiza SOLO si estamos “cerca del equilibrio” durante varias iteraciones
//   (para no contaminarlo con la inercia al cruzar la consigna)
// - Cuando errDeg<0, la reducción se hace respecto a targetBase (no respecto a s_MP_eq),
//   y se limita para evitar “bajadas” demasiado fuertes.

static int ApplyVerticalUnidirectionalControl_Up(int deltaMP, float errDeg)
{
    // --- Tuning knobs ---
    const float ERR_DB        = V_ERR_DB_DEG;   // deadband “de control”
    const int   MIN_OUT       = 15;             // nunca bajar de este registro
    const int   MAX_NEG       = 10;             // máxima reducción extra vía deltaMP cuando err<0
    const float Kb            = V_BIAS_REL_K;   // [reg/deg] freno al pasarte
    const int   MAX_CUT       = 30;             // recorte máximo por “pasarte” (evita bajadas brutales)

    // --- Aprendizaje de targetBase (por inercia: solo en equilibrio) ---
    const float ERR_LEARN     = 0.5f;           // grados (más grande que ERR_DB)
    const int   HOLD_SAMPLES  = 6;              // nº iteraciones seguidas “estables” antes de actualizar
    const float ALPHA         = 0.05f;          // adaptación lenta del targetBase (0.02..0.1 típico)

    static int  targetBase    = 0;
    static bool inited        = false;
    static int  stableCount   = 0;

    // Base nominal (tu antigua “equilibrio” fijo)
    int base = clampi(s_MP_eq, 0, 100);

    if (!inited) {
        targetBase = base;
        inited = true;
    }

    // 0) Detecta si estamos realmente cerca del equilibrio (evita inercia / cruce rápido)
    //    (sin medida de velocidad aquí, usamos “varias muestras seguidas” como criterio simple)
    if (fabsf(errDeg) < ERR_LEARN) {
        if (stableCount < HOLD_SAMPLES) stableCount++;
    } else {
        stableCount = 0;
    }

    // 1) Deadband: cerca de consigna, mantener targetBase (y solo actualizarlo si llevamos
    //    varias iteraciones estables para que no aprenda durante el cruce por inercia).
    if (fabsf(errDeg) < ERR_DB) {
        if (stableCount >= HOLD_SAMPLES) {
            // Aprendemos lentamente hacia la base nominal (puedes cambiar “base” por “out aplicado”
            // si lo pasas como argumento externo).
            float tb = (1.0f - ALPHA) * (float)targetBase + ALPHA * (float)base;
            targetBase = clampi((int)lroundf(tb), MIN_OUT, 100);
        }
        return clampi(targetBase, MIN_OUT, 100);
    }

    // 2) Si nos pasamos (err < 0): reduce respecto a targetBase (no respecto a s_MP_eq),
    //    y limita recorte total para no “bajar con tanta fuerza”.
    if (errDeg < 0.0f) {
        int cut = (int)lroundf((-errDeg) * Kb);
        cut = clampi(cut, 0, MAX_CUT);

        int base_rel = clampi(targetBase - cut, MIN_OUT, 100);

        // Limita cuánto puede recortar el PD adicionalmente en esta zona
        int delta_limited = clampi(deltaMP, -MAX_NEG, 100);

        int out = base_rel + delta_limited;

        // No permitir invertir ni apagar
        return clampi(out, MIN_OUT, 100);
    }

    // 3) Si estamos por debajo (err > 0): empuja alrededor de targetBase
    {
        int out = targetBase + deltaMP;

        // No permitir cruzar a negativo en este modo unidireccional
        if (out < 0) out = 0;

        return clampi(out, 0, 100);
    }
}

static int ApplyVerticalUnidirectionalControl_Down(int deltaMP, float errDeg)
{
    // Salida unidireccional: [-100..0]
    const int   MAX_DOWN_MAG  = 100;
    const int   MIN_DOWN_MAG  = 15;
    const float ERR_DB        = V_ERR_DB_DEG_DOWN;

    // Base adaptativa
    const int   BASE_MIN      = -60;   // límite más negativo al que puede llegar la base
    const int   BASE_MAX      = 0;     // nunca positiva

    // Ganancia de “adaptación” de la base (cuánto cambia por grado y por iteración)
    const float Kbase_down    = 0.15f; // cuando err<0: más negativo (0.05..0.3)
    const float Kbase_up      = 0.05f; // cuando err>0: hacia 0 más rápido (0.1..0.8)

    // Limita el cambio por iteración (evita saltos)
    const int   STEP_LIMIT    = 4;     // máx cambio de targetBase por ciclo

    static int targetBase = 0;         // empieza suelto

    // Deadband: suelta (o deja base tal cual; recomiendo soltar)
    if (fabsf(errDeg) < ERR_DB) {
        return 0;
    }

    // --- Actualiza base en función del signo del error ---
    // errDeg < 0 => necesito BAJAR: targetBase se hace más negativo
    // errDeg > 0 => me pasé por debajo: targetBase se acerca a 0 (soltar)
    int dBase = 0;

    if (errDeg < 0.0f) {
        dBase = (int)lroundf((-errDeg) * Kbase_down);  // grados -> registros
        dBase = clampi(dBase, 0, STEP_LIMIT);
        targetBase -= dBase;                            // más negativo
    } else {
        dBase = (int)lroundf((errDeg) * Kbase_up);
        dBase = clampi(dBase, 0, STEP_LIMIT);
        targetBase += dBase;                            // hacia 0
    }

    targetBase = clampi(targetBase, BASE_MIN, BASE_MAX);

    // --- Salida final: base + PID (solo si ayuda a bajar) ---
    if (deltaMP > 0) deltaMP = 0;       // nunca permitimos “subida” desde el PID

    int out = targetBase + deltaMP/2;

    // si toca bajar (err<0), asegura mínimo de bajada
    if (errDeg < 0.0f && out > -MIN_DOWN_MAG) out = -MIN_DOWN_MAG;

    // unidireccional estricto
    out = clampi(out, -MAX_DOWN_MAG, 0);
    return out;
}


static int ApplyVerticalBandHold(int deltaMP, float errDeg, float measVertDeg)
{
    // Nota: el modo está “latcheado” como REST_BAND, pero dentro de él
    // decidimos qué acción usar en función de dónde esté la medida.

    if (measVertDeg > V_REST_HIGH_DEG) {
        // estamos por encima de -36 -> queremos bajar hasta banda
        // usar control UP (motor empuja arriba, pero al pasarte reduce y deja caer)
        return ApplyVerticalUnidirectionalControl_Up(deltaMP, errDeg);
    }

    if (measVertDeg < V_REST_LOW_DEG) {
        // estamos por debajo de -37 -> queremos subir hasta banda
        // usar control DOWN (motor empuja abajo, y al “pasarte” reduce dejando subir por gravedad)
        return ApplyVerticalUnidirectionalControl_Down(deltaMP, errDeg);
    }

    // ya estamos en la banda -> mantener según consigna actual
    // (si quieres fijarlo al centro de banda, cambia errDeg a (center - meas))
    return ApplyVerticalUnidirectionalControl_Up(deltaMP, errDeg);
}

static int ApplyHorizontalBidirectionalControl(int deltaRDC,
                                              float errDeg,
                                              float refH_deg)
{
    // --- Tuning knobs ---
    const float ERR_DB         = 0.15f;  // deg: deadband para “no cazar”
    const float ERR_LEARN      = 0.6f;   // deg: ventana para considerar “cerca del equilibrio”
    const int   HOLD_SAMPLES   = 8;      // nº iteraciones seguidas estables antes de aprender
    const float ALPHA          = 0.05f;  // aprendizaje lento del base
    const float REF_STEP_RESET = 2.0f;   // deg: si la consigna cambia más que esto, no aprendas (reset)

    // límites de salida/base
    const int   OUT_MIN = -100;
    const int   OUT_MAX =  100;
    const int   BASE_MIN = -60;          // evita que el base se coma toda la autoridad del PID
    const int   BASE_MAX =  60;

    // Limita cuánto puede cambiar el base por iteración (evita saltos)
    const int   BASE_STEP_LIMIT = 2;

    static int   baseH        = 0;       // base adaptativa (trim), arranca en 0
    static float lastRefH     = 0.0f;
    static bool  inited       = false;
    static int   stableCount  = 0;

    if (!inited) {
        lastRefH = refH_deg;
        baseH = 0;
        stableCount = 0;
        inited = true;
    }

    // 0) Si la consigna pega un salto, no “aprendas” en ese transitorio
    if (fabsf(refH_deg - lastRefH) > REF_STEP_RESET) {
        stableCount = 0;
        lastRefH = refH_deg;
    } else {
        lastRefH = refH_deg;
    }

    // 1) Estabilidad: contamos muestras seguidas con error pequeño
    if (fabsf(errDeg) < ERR_LEARN) {
        if (stableCount < HOLD_SAMPLES) stableCount++;
    } else {
        stableCount = 0;
    }

    // 2) Deadband: si estamos muy cerca, devolvemos base (y opcionalmente aprendemos)
    if (fabsf(errDeg) < ERR_DB) {

        // Aprende base solo si llevamos tiempo estables
        // Intuición: si en equilibrio el PID sigue pidiendo algo,
        // esa “media” debería pasar a base para liberar al PID.
        if (stableCount >= HOLD_SAMPLES) {

            // Queremos que deltaRDC tienda a 0 en equilibrio,
            // así que base := base + (parte de) deltaRDC
            float newBase = (1.0f - ALPHA) * (float)baseH + ALPHA * (float)(baseH + deltaRDC);

            int baseCandidate = (int)lroundf(newBase);

            // Limita cambio por iteración
            int step = baseCandidate - baseH;
            if (step >  BASE_STEP_LIMIT) step =  BASE_STEP_LIMIT;
            if (step < -BASE_STEP_LIMIT) step = -BASE_STEP_LIMIT;
            baseH += step;

            // Limita base
            if (baseH > BASE_MAX) baseH = BASE_MAX;
            if (baseH < BASE_MIN) baseH = BASE_MIN;
        }

        // En deadband: salida = base (sin delta) para evitar caza
        int out = baseH;
        if (out > OUT_MAX) out = OUT_MAX;
        if (out < OUT_MIN) out = OUT_MIN;
        return out;
    }

    // 3) Fuera del deadband: salida bidireccional normal
    int out = baseH + deltaRDC;

    // Saturación de salida
    if (out > OUT_MAX) out = OUT_MAX;
    if (out < OUT_MIN) out = OUT_MIN;

    return out;
}


// =====================================================
// Steps (con encoders y con medidas externas)
// =====================================================

/**
 * @brief
 * Ejecuta un paso del controlador PID-4.
 * @note
 * - Lee ángulos (grados) desde encoders
 * - Convierte a radianes
 * - Calcula errores eh/ev
 * - Ejecuta PID4_Update (que en SISO queda efectivamente con los PIDs anulados)
 * - Convierte a registros y aplica MotorControl_update
 *
 * Además:
 *  - Si modo vertical-only -> fuerza Registro_RDC=0
 *  - Si modo horizontal-only -> fuerza Registro_MP=0
 */
void PID4_Step(float dt)
{
    if (!s_pid4_enabled) return;
    if (dt <= 0.0f) dt = 1e-3f;

    // 1) Leer ángulos medidos en GRADOS
    float measVertDeg, measHorDeg;
    TRMS_GetAnglesDeg(measVertDeg, measHorDeg);

    // 2) Convertir referencias y medidas a RAD
    float refV_rad  = s_refVertDeg * DEG_TO_RAD;
    float refH_rad  = s_refHorDeg  * DEG_TO_RAD;
    float measV_rad = measVertDeg  * DEG_TO_RAD;
    float measH_rad = measHorDeg   * DEG_TO_RAD;

    // 3) Errores (rad)
    float ev = refV_rad - measV_rad;
    float eh = refH_rad - measH_rad;

    // 4) Ejecutar PID-4
    float Uh, Uv;
    PID4_Update(s_pid4_params, s_pid4_state, eh, ev, dt, Uh, Uv);

    // 5) Registro horizontal (igual que antes)
    Registro_RDC = U_to_Register(Uh, s_pid4_params.Uh_max);

    // 6) Registro vertical: aplicar "Opción B"
    int deltaMP = U_to_Register(Uv, s_pid4_params.Uv_max);
    Registro_MP = ApplyVerticalUnidirectionalControl_Up(deltaMP, s_refVertDeg);

    // 7) Forzar salidas según modo SISO
    if (s_pidMode == PIDMode::VERTICAL_ONLY) {
        Registro_RDC = 0;
    } else if (s_pidMode == PIDMode::HORIZONTAL_ONLY) {
        Registro_MP = 0;
    }

    // 8) Aplicar a los DACs / sliders
    MotorControl_update(Registro_MP, Registro_RDC);
}

/**
 * @brief
 * Igual que PID4_Step, pero usando medidas ya proporcionadas (en grados).
 * @note
 * Útil si las lecturas vienen de otro lugar o quieres simular.
 */
void PID4_StepWithMeasurements(float dt, float measVertDeg, float measHorDeg)
{
    if (!s_pid4_enabled) return;
    if (dt <= 0.0f) dt = 1e-3f;

    // 1) Convertir a RAD (PID interno trabaja en rad)
    float refV_rad  = s_refVertDeg * DEG_TO_RAD;
    float refH_rad  = s_refHorDeg  * DEG_TO_RAD;
    float measV_rad = measVertDeg  * DEG_TO_RAD;
    float measH_rad = measHorDeg   * DEG_TO_RAD;

    // 2) Errores (rad)
    float ev = refV_rad - measV_rad;   // vertical
    float errDeg = s_refVertDeg - measVertDeg;
    float errH_deg = s_refHorDeg - measHorDeg;

    float eh = refH_rad - measH_rad;   // horizontal

    // 3) PID-4
    float Uh, Uv;
    PID4_Update(s_pid4_params, s_pid4_state, eh, ev, dt, Uh, Uv);

    // 4) Registro horizontal (con reducción si la consigna es negativa)
    int deltaRDC = U_to_Register(Uh, s_pid4_params.Uh_max);

    // --- Reducción cerca de cero (|ref| <= 35 deg) ---
    static constexpr float H_NEAR0_DEG = 35.0f;
    static constexpr float K_RDC_NEAR0 = 0.80f; // prueba 0.75..0.90
    static constexpr float K_RDC_NEG = 0.65f;

    // 1) Si la consigna está cerca de cero, reduce en ambos sentidos
    if (fabsf(s_refHorDeg) <= H_NEAR0_DEG) {
        // entre -35 y +35
        deltaRDC = (int)lroundf((float)deltaRDC * K_RDC_NEAR0);
    }
    else if (s_refHorDeg < -H_NEAR0_DEG) {
        // por debajo de -35
        deltaRDC = (int)lroundf((float)deltaRDC * K_RDC_NEG);
    }
    
    // Clamp final por seguridad
    if (deltaRDC > 100)  deltaRDC = 100;
    if (deltaRDC < -100) deltaRDC = -100;

    Registro_RDC = deltaRDC;

    // 5) Registro vertical: convertir Uv a delta y aplicar Opción B
    
    int deltaMP = U_to_Register(Uv, s_pid4_params.Uv_max);



    Registro_RDC = ApplyHorizontalBidirectionalControl(deltaRDC, errH_deg, s_refHorDeg);

    switch (s_vertZone) {
    case VertRefZone::ABOVE_REST:
        // ref > -37
        Registro_MP = ApplyVerticalUnidirectionalControl_Up(deltaMP, errDeg);
        break;

    case VertRefZone::BELOW_REST:
        // ref < -36
        Registro_MP = ApplyVerticalUnidirectionalControl_Down(deltaMP, errDeg);
        break;

    case VertRefZone::REST_BAND:
    default:
        // -37 <= ref <= -36
        Registro_MP = ApplyVerticalBandHold(deltaMP, errDeg, measVertDeg);
        break;
    }
    
    // 6) Forzar salidas según modo
    if (s_pidMode == PIDMode::VERTICAL_ONLY) {
        Registro_RDC = 0;
    } else if (s_pidMode == PIDMode::HORIZONTAL_ONLY) {
        Registro_MP = 0;
    }
/*
    // DEBUG: modo actual antes de aplicar motores
        static uint32_t lastModePrint = 0;
        uint32_t nowMode = millis();
        if (nowMode - lastModePrint > 500) {   // cada 500 ms
            lastModePrint = nowMode;

            const char* modeStr =
                (s_pidMode == PIDMode::MIMO_FULL)       ? "MIMO_FULL" :
                (s_pidMode == PIDMode::VERTICAL_ONLY)   ? "VERTICAL_ONLY" :
                (s_pidMode == PIDMode::HORIZONTAL_ONLY) ? "HORIZONTAL_ONLY" :
                                                        "UNKNOWN";

            Serial.printf(
                "[PID MODE] %s | MP=%d | RDC=%d\n",
                modeStr, Registro_MP, Registro_RDC
            );
        }
*/
    // 7) Aplicar
    MotorControl_update(Registro_MP, Registro_RDC);

    // ------------------------------------------------------------
    // 8) DEBUG mínimo: ref/meas/err + Uv + Uvmax + deltaMP + registros
    // ------------------------------------------------------------
    static uint32_t lastPrintMs = 0;
    uint32_t now = millis();

    if (now - lastPrintMs >= 500) { // cada 500 ms
        lastPrintMs = now;
        char buf[256];

        //float errDeg = (s_refVertDeg - measVertDeg);

        if (s_pidMode == PIDMode::VERTICAL_ONLY) {
            // Solo vv (ev -> Uv)
            /*snprintf(buf, sizeof(buf),
                "PID V-ONLY\n"
                "vv: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "Uvmax=%.3f",
                s_pid4_params.vv.Kp, s_pid4_params.vv.Ki, s_pid4_params.vv.Kd, s_pid4_params.vv.I_sat,
                s_pid4_params.Uv_max ); */
/*
            if (logger_start && millis() - last >= 50) {
                last += 50;            // mantiene periodo estable
                Serial.print(measVertDeg);
                Serial.print(" ");
                Serial.println(s_refVertDeg);
        } */
    /*    

        Serial.printf(
            "[PID V] refV=%.2f | measV=%.2f | err=%.2f || "
            "Uv=%.5f/Uvmax=%.5f || deltaMP=%d -> Reg_MP=%d (bias=%d)\n",
            s_refVertDeg, measVertDeg, errDeg,
            Uv, s_pid4_params.Uv_max,
            deltaMP, Registro_MP, s_MP_eq
        );
    

        Serial.printf("measVertDeg=%.2f deg | measV_rad=%.4f rad\n",
              measVertDeg, measV_rad); */

        } else if (s_pidMode == PIDMode::HORIZONTAL_ONLY) {
            /* //Solo hh (eh -> Uh)
            snprintf(buf, sizeof(buf),
                "PID H-ONLY\n"
                "hh: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "Uhmax=%.3f",
                s_pid4_params.hh.Kp, s_pid4_params.hh.Ki, s_pid4_params.hh.Kd, s_pid4_params.hh.I_sat,
                s_pid4_params.Uh_max
            );*/
        } 
    }
        
}



    /*
    // ------------------------------------------------------------
    // DEBUG extendido: imprimir SOLO constantes usadas por el PID
    // (Deja esto comentado y actívalo si lo necesitas)
    // ------------------------------------------------------------
    static uint32_t lastPrintMs2 = 0;
    uint32_t now2 = millis();

    if (now2 - lastPrintMs2 >= 500) { // cada 500 ms
        lastPrintMs2 = now2;

        char buf[256];

        if (s_pidMode == PIDMode::VERTICAL_ONLY) {
            // Solo vv (ev -> Uv)
            snprintf(buf, sizeof(buf),
                "PID V-ONLY\n"
                "vv: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "Uvmax=%.3f",
                s_pid4_params.vv.Kp, s_pid4_params.vv.Ki, s_pid4_params.vv.Kd, s_pid4_params.vv.I_sat,
                s_pid4_params.Uv_max
            );

        } else if (s_pidMode == PIDMode::HORIZONTAL_ONLY) {
            // Solo hh (eh -> Uh)
            snprintf(buf, sizeof(buf),
                "PID H-ONLY\n"
                "hh: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "Uhmax=%.3f",
                s_pid4_params.hh.Kp, s_pid4_params.hh.Ki, s_pid4_params.hh.Kd, s_pid4_params.hh.I_sat,
                s_pid4_params.Uh_max
            );

        } else {
            // MIMO completo
            snprintf(buf, sizeof(buf),
                "PID MIMO\n"
                "hh: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "hv: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "vh: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "vv: Kp=%.4f Ki=%.4f Kd=%.4f Isat=%.4f\n"
                "Uhmax=%.3f Uvmax=%.3f",
                s_pid4_params.hh.Kp, s_pid4_params.hh.Ki, s_pid4_params.hh.Kd, s_pid4_params.hh.I_sat,
                s_pid4_params.hv.Kp, s_pid4_params.hv.Ki, s_pid4_params.hv.Kd, s_pid4_params.hv.I_sat,
                s_pid4_params.vh.Kp, s_pid4_params.vh.Ki, s_pid4_params.vh.Kd, s_pid4_params.vh.I_sat,
                s_pid4_params.vv.Kp, s_pid4_params.vv.Ki, s_pid4_params.vv.Kd, s_pid4_params.vv.I_sat,
                s_pid4_params.Uh_max, s_pid4_params.Uv_max
            );
        }

        Serial.println(buf);
    }
    */

/* ------------------------------------------------------------------------------------
   FUNCIONES “ANTIGUAS” (compatibilidad)
   ------------------------------------------------------------------------------------
   Si en tu proyecto ya estabas llamando a PID4_Vertical_Step / PID4_Horizontal_Step,
   puedes mantenerlas como wrappers.

   IMPORTANTE:
     Estas funciones NO intentan “tocar” el modo global para no romper el resto del control,
     simplemente fuerzan una de las salidas a cero al aplicar MotorControl_update.
   ------------------------------------------------------------------------------------ */

void PID4_Vertical_Step(float dt, float measVertDeg, float measHorDeg)
{
    if (!s_pid4_enabled) return;
    if (dt <= 0.0f) dt = 1e-3f;

    float refV_rad  = s_refVertDeg * DEG_TO_RAD;
    float refH_rad  = s_refHorDeg  * DEG_TO_RAD;
    float measV_rad = measVertDeg  * DEG_TO_RAD;
    float measH_rad = measHorDeg   * DEG_TO_RAD;

    float ev = refV_rad - measV_rad;
    float eh = refH_rad - measH_rad;

    float Uh, Uv;
    PID4_Update(s_pid4_params, s_pid4_state, eh, ev, dt, Uh, Uv);

    // Horizontal a 0
    int rdc_zero = 0;

    // Vertical con Opción B
    int deltaMP = U_to_Register(Uv, s_pid4_params.Uv_max);
    int mp = ApplyVerticalUnidirectionalControl_Up(deltaMP, s_refVertDeg);

    MotorControl_update(mp, rdc_zero);
}

void PID4_Horizontal_Step(float dt, float measVertDeg, float measHorDeg)
{
    if (!s_pid4_enabled) return;
    if (dt <= 0.0f) dt = 1e-3f;

    float refV_rad  = s_refVertDeg * DEG_TO_RAD;
    float refH_rad  = s_refHorDeg  * DEG_TO_RAD;
    float measV_rad = measVertDeg  * DEG_TO_RAD;
    float measH_rad = measHorDeg   * DEG_TO_RAD;

    float ev = refV_rad - measV_rad;
    float eh = refH_rad - measH_rad;

    float Uh, Uv;
    PID4_Update(s_pid4_params, s_pid4_state, eh, ev, dt, Uh, Uv);

    // Vertical a 0
    int mp_zero = 0;

    // Horizontal normal
    int rdc = U_to_Register(Uh, s_pid4_params.Uh_max);

    MotorControl_update(mp_zero, rdc);
}

// Actualización de las series de referencia en la gráfica del UI
void Chart_UpdateReferences(float refH_deg, float refV_deg)
{
    uint16_t pc = lv_chart_get_point_count(ui_GraphEncoder3);

    for (uint16_t i = 0; i < pc; i++) {
        ui_GraphEncoder3_series_refH->y_points[i] = (lv_coord_t)refH_deg;
        ui_GraphEncoder3_series_refV->y_points[i] = (lv_coord_t)refV_deg;
    }

    lv_chart_refresh(ui_GraphEncoder3);
}

// Función para ajustar el registro de equilibrio vertical
void PID4_SetVerticalEquilibriumRegister(int mp_eq)
{
    if (mp_eq > 100)  mp_eq = 100;
    if (mp_eq < -100) mp_eq = -100;
    s_MP_eq = mp_eq;
}
