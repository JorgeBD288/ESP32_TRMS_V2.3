/* Esta librería, junto con su correspondiente "Encoders.h", sirve para declarar las funciones
que controlan los HCTL-2016, el TCA9539 y la representación en la pantalla LCD
de los ángulos obtenidos por los encoders */

/*  Encoders.cpp
    Librería para controlar HCTL-2016 + TCA9539 y convertir a ángulos.

    Cambios incluidos:
    - Lectura TCA más robusta (STOP en endTransmission + requestFrom con stop)
    - Secuencia de lectura 16-bit tipo “cronograma”: SEL low -> leer low, (opcional toggle OE), SEL high -> leer high
    - Soporta swap IN0/IN1 (tu caso: IN0=Horizontal, IN1=Vertical)
    - Soporta toggle de OE entre bytes (porque tu montaje lo necesita)
    - Offsets internos para que 0° sea la posición en el primer sample (o tras Encoders_resetCounters())
*/

#include "Encoders.h"
#include <Wire.h>
#include <lvgl.h>

// Dirección I2C del TCA9539
#define TCA_ADDR 0x74

// Reset del TCA9539 (activo LOW)
#define TCA_RESET 32

// Debug
#define TCA_DEBUG 1
#if TCA_DEBUG
  #define TCA_LOGF(...) Serial.printf(__VA_ARGS__)
#else
  #define TCA_LOGF(...) do{}while(0)
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

// Pines de control HCTL (guardados internamente)
static uint8_t s_pinSEL = 0;
static uint8_t s_pinRST = 0;
static uint8_t s_pinOE  = 0;

// Configuración cuentas por vuelta
static EncoderConfig s_config;

// Series del chart
static lv_chart_series_t * s_serHorizontal = nullptr;
static lv_chart_series_t * s_serVertical   = nullptr;

// -------------------------
// Ajustes de lectura (us)
// -------------------------
static uint16_t s_afterSelUs = 5;  // margen >> 65ns
static uint16_t s_afterOeUs  = 5;  // margen >> 65ns
static uint16_t s_triUs      = 5;  // margen >> 40ns

// Si tu montaje necesita “pulsar” OE entre bytes
static bool s_toggleOE = true;

// Si necesitas intercambiar puertos (tu caso real: IN0=H, IN1=V)
static bool s_swapPorts = true;

// Offsets para 0° en posición inicial
static bool    s_offsetsReady = false;
static int16_t s_offH = 0;
static int16_t s_offV = 0;

// ============================================================
// Funciones internas HCTL
// ============================================================

static void hctl_setSEL(bool level) {
  // En tu estado actual: sin inversión aquí
  digitalWrite(s_pinSEL, level);
}

static void hctl_setRST(bool level) {
  // En tu lógica actual: RST activo HIGH
  digitalWrite(s_pinRST, level);
}

static void hctl_setOE(bool level) {
  // En tu lógica actual: OE_ENABLE = HIGH, OE_DISABLE = LOW
  digitalWrite(s_pinOE, level);
}

// ============================================================
// Funciones internas TCA9539
// ============================================================

static bool tca_writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission(true); // STOP

#if TCA_DEBUG
  TCA_LOGF("[TCA9539][W] reg 0x%02X <= 0x%02X -> endTx=%u (%s)\n",
           reg, value, err, i2cErrToStr(err));
#endif

  return (err == 0);
}

// Lectura robusta (STOP tras setear registro + requestFrom con stop)
static bool tca_readReg(uint8_t reg, uint8_t &out) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(true); // STOP
  if (err != 0) return false;

  uint8_t n = Wire.requestFrom((uint16_t)TCA_ADDR, (uint8_t)1, (uint8_t)true);
  if (n != 1) return false;

  out = Wire.read();
  return true;
}

static bool tca_readPort(uint8_t port, uint8_t &out) {
  // Input Port 0 = 0x00, Input Port 1 = 0x01
  return tca_readReg(port == 0 ? 0x00 : 0x01, out);
}

static bool tca_init() {
#if TCA_DEBUG
  TCA_LOGF("[TCA9539] Init: probando direccion 0x%02X\n", TCA_ADDR);
#endif

  // Configuración: todos los pines como entradas + sin inversión de polaridad
  struct StepW { uint8_t reg; uint8_t val; const char* name; };
  const StepW stepsW[] = {
    {0x06, 0xFF, "Configuration Port 0"},
    {0x07, 0xFF, "Configuration Port 1"},
    {0x04, 0x00, "Polarity Port 0"},
    {0x05, 0x00, "Polarity Port 1"},
  };

  for (auto &s : stepsW) {
#if TCA_DEBUG
    TCA_LOGF("[TCA9539] Write %s...\n", s.name);
#endif
    if (!tca_writeReg(s.reg, s.val)) {
#if TCA_DEBUG
      TCA_LOGF("[TCA9539] FAIL escribiendo %s\n", s.name);
#endif
      return false;
    }
  }

  // Verificación por lectura
  struct StepR { uint8_t reg; uint8_t expected; const char* name; };
  const StepR stepsR[] = {
    {0x06, 0xFF, "Configuration Port 0"},
    {0x07, 0xFF, "Configuration Port 1"},
    {0x04, 0x00, "Polarity Port 0"},
    {0x05, 0x00, "Polarity Port 1"},
  };

  for (auto &s : stepsR) {
    uint8_t v = 0;
#if TCA_DEBUG
    TCA_LOGF("[TCA9539] Verify %s...\n", s.name);
#endif
    if (!tca_readReg(s.reg, v)) {
#if TCA_DEBUG
      TCA_LOGF("[TCA9539] FAIL leyendo %s\n", s.name);
#endif
      return false;
    }
    if (v != s.expected) {
#if TCA_DEBUG
      TCA_LOGF("[TCA9539] FAIL verify %s esperado=0x%02X leido=0x%02X\n",
               s.name, s.expected, v);
#endif
      return false;
    }
  }

#if TCA_DEBUG
  TCA_LOGF("[TCA9539] Init OK\n");
#endif
  return true;
}

// ============================================================
// Reset HCTL
// ============================================================

static void hctl_resetCounters_internal() {
  // En tu lógica: reset activo HIGH
  hctl_setRST(1);
  delayMicroseconds(5);
  hctl_setRST(0);
}

// ============================================================
// Lectura 16-bit “tipo cronograma”
// SEL=0 -> LOW byte, SEL=1 -> HIGH byte (según tu prueba)
// ============================================================

static bool hctl_read16_ports(uint8_t &lo0, uint8_t &lo1,
                              uint8_t &hi0, uint8_t &hi1)
{
  // (A) Tri-state breve para “abrir ventana”
  hctl_setOE(0);
  delayMicroseconds(s_triUs);

  // (B) LOW byte
  hctl_setSEL(0); // LOW byte
  delayMicroseconds(s_afterSelUs);

  hctl_setOE(1);
  delayMicroseconds(s_afterOeUs);

  if (!tca_readPort(0, lo0)) return false;
  if (!tca_readPort(1, lo1)) return false;

  // (B2) Opcional: “pulsar” OE entre bytes si tu hardware lo necesita
  if (s_toggleOE) {
    hctl_setOE(0);
    delayMicroseconds(s_triUs);
    hctl_setOE(1);
    delayMicroseconds(s_afterOeUs);
  }

  // (C) HIGH byte
  hctl_setSEL(1); // HIGH byte
  delayMicroseconds(s_afterSelUs);

  if (!tca_readPort(0, hi0)) return false;
  if (!tca_readPort(1, hi1)) return false;

  // (D) Tri-state final (opcional)
  hctl_setOE(0);
  delayMicroseconds(s_triUs);

  return true;
}

// ============================================================
// Helpers
// ============================================================

static float countsToDegreesSigned(int16_t signedCount, float countsPerRev) {
  if (countsPerRev <= 0.0f) return 0.0f;
  return (float)signedCount * (360.0f / countsPerRev);
}

// ============================================================
// API pública
// ============================================================

bool Encoders_begin(uint8_t pinSEL,
                    uint8_t pinRST,
                    uint8_t pinOE,
                    const EncoderConfig &config)
{
  s_pinSEL = pinSEL;
  s_pinRST = pinRST;
  s_pinOE  = pinOE;
  s_config = config;

  pinMode(s_pinSEL, OUTPUT);
  pinMode(s_pinRST, OUTPUT);
  pinMode(s_pinOE,  OUTPUT);

  // Reset pin del TCA
  pinMode(TCA_RESET, OUTPUT);

  // Reset del TCA9539 (activo LOW)
  digitalWrite(TCA_RESET, LOW);
  delay(5);
  digitalWrite(TCA_RESET, HIGH);
  delay(5);

  // Estado inicial recomendado
  hctl_setOE(0);   // tri-state
  hctl_setRST(0);  // no reset
  hctl_setSEL(0);  // low byte por defecto

  // Init TCA
  bool ok = tca_init();
  if (!ok) {
    Serial.println("Encoders: ERROR inicializando TCA9539 (I2C).");
    return false;
  } else {
    Serial.println("Encoders: TCA9539 inicializado correctamente.");
  }

  // Defaults coherentes con tu caso
  s_toggleOE     = true;
  s_swapPorts    = true;
  s_afterSelUs   = 5;
  s_afterOeUs    = 5;
  s_triUs        = 5;
  s_offsetsReady = false;

  // Reset contadores (opcional, pero normalmente deseado)
  hctl_resetCounters_internal();
  s_offsetsReady = false;

  return true;
}

void Encoders_resetCounters() {
  hctl_resetCounters_internal();
  s_offsetsReady = false; // para que 0° sea la posición tras el reset
}

bool Encoders_readCounts(int16_t &countV, int16_t &countH)
{
  uint8_t lo0=0, lo1=0, hi0=0, hi1=0;

  if (!hctl_read16_ports(lo0, lo1, hi0, hi1)) return false;

  // u0 = IN0, u1 = IN1
  uint16_t u0 = ((uint16_t)hi0 << 8) | lo0;
  uint16_t u1 = ((uint16_t)hi1 << 8) | lo1;

  int16_t s0 = (int16_t)u0; // complemento a2
  int16_t s1 = (int16_t)u1;

  // Tu caso real: IN0=Horizontal, IN1=Vertical
  int16_t sH = s_swapPorts ? s0 : s1;
  int16_t sV = s_swapPorts ? s1 : s0;

  countH = sH;
  countV = sV;

  // Captura offsets en el primer sample tras begin/reset
  if (!s_offsetsReady) {
    s_offH = countH;
    s_offV = countV;
    s_offsetsReady = true;
  }

  return true;
}

EncoderAngles Encoders_readAngles() {
  EncoderAngles ang{0.0f, 0.0f};

  int16_t cV=0, cH=0;
  if (!Encoders_readCounts(cV, cH)) {
    return ang;
  }

  int16_t relH = (int16_t)(cH - s_offH);
  int16_t relV = (int16_t)(cV - s_offV);

  ang.horizontalDeg = countsToDegreesSigned(relH, s_config.countsPerRevHorizontal);
  ang.verticalDeg   = countsToDegreesSigned(relV, s_config.countsPerRevVertical);

  return ang;
}

// --- Configuración runtime ---
void Encoders_setSwapPorts(bool swap) { s_swapPorts = swap; }
void Encoders_setToggleOE(bool enable) { s_toggleOE = enable; }
void Encoders_setTimingsUs(uint16_t afterSelUs, uint16_t afterOeUs, uint16_t triUs) {
  s_afterSelUs = afterSelUs;
  s_afterOeUs  = afterOeUs;
  s_triUs      = triUs;
}

// --- Debug: leer puertos directamente ---
bool Encoders_readTcaPorts(uint8_t &port0, uint8_t &port1) {
  bool ok0 = tca_readPort(0, port0);
  bool ok1 = tca_readPort(1, port1);
  return ok0 && ok1;
}

// --- Wrapper públicos de control (si los necesitas) ---
void Encoders_setOE(bool level)  { hctl_setOE(level); }
void Encoders_setRST(bool level) { hctl_setRST(level); }
void Encoders_setSEL(bool level) { hctl_setSEL(level); }

void Encoders_pulseReset(uint16_t high_us) {
  // reset activo HIGH (según tu lógica)
  Encoders_setRST(1);
  delayMicroseconds(high_us);
  Encoders_setRST(0);
}

// ============================================================
// LVGL chart (sin cambios funcionales)
// ============================================================

void Encoders_chartBindSeries(lv_chart_series_t *serH, lv_chart_series_t *serV) {
    s_serHorizontal = serH;
    s_serVertical   = serV;
}

void Encoders_chartInit(lv_obj_t *chart) {
    if (!chart) return;

    // NO creamos series aquí (las crea SquareLine)
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    // Rango en grados
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,   -180, 180);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, -180, 180);
}

void Encoders_chartAddSample(lv_obj_t *chart, const EncoderAngles &angles) {
    if (!chart) return;
    if (!s_serHorizontal || !s_serVertical) return;

    lv_chart_set_next_value(chart, s_serHorizontal, (lv_coord_t)angles.horizontalDeg);
    lv_chart_set_next_value(chart, s_serVertical,   (lv_coord_t)angles.verticalDeg);
}

// ============================================================
// Utilidad opcional: scan I2C (igual que lo tenías)
// ============================================================

void i2cScan() {
  Serial.println("Escaneando I2C...");
  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Serial.print("  Dispositivo en 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  (No se ha encontrado ningun dispositivo I2C)");
  }
  Serial.println("Fin escaneo I2C.");
}
