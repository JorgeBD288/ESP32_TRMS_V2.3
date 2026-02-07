#include "SerialAnsiLogger.h"

SerialAnsiLogger::SerialAnsiLogger(Ansiterm& term, uint32_t samplePeriodMs)
: _term(term),
  _period(samplePeriodMs),
  _lastTick(0),
  _sampleNo(0)
{}

void SerialAnsiLogger::setPeriod(uint32_t samplePeriodMs) {
    _period = samplePeriodMs;
}

void SerialAnsiLogger::begin(bool withHeader) {
    _term.reset();

    // Limpia pantalla SOLO al inicio
    Serial.print("\x1b[2J");
    Serial.print("\x1b[H");

    if (withHeader) {
        printHeader();
    }

    _lastTick = millis();
    _sampleNo = 0;
}

void SerialAnsiLogger::printHeader() {
    _term.boldOn();
    _term.setForegroundColor(WHITE);
    Serial.println(" Tiempo (s) |   V (deg) |   H (deg) | refV (deg) | refH (deg)");
    Serial.println("-------------------------------------------------------------");
    _term.boldOff();
    _term.reset();
}

// Medidas + consignas
void SerialAnsiLogger::update(float degV, float degH, float refV, float refH) {
    uint32_t now = millis();
    if (now - _lastTick < _period) return;
    _lastTick += _period;

    float t = (_sampleNo * _period) / 1000.0f;
    _sampleNo++;

    char buf[32];

    // Tiempo
    _term.setForegroundColor(CYAN);
    snprintf(buf, sizeof(buf), "%9.3f", t);
    Serial.print(buf);

    _term.setForegroundColor(WHITE);
    Serial.print(" | ");

    // Vertical medida
    _term.setForegroundColor(MAGENTA);
    snprintf(buf, sizeof(buf), "%8.2f", degV);
    Serial.print(buf);

    _term.setForegroundColor(WHITE);
    Serial.print(" | ");

    // Horizontal medida
    _term.setForegroundColor(YELLOW);
    snprintf(buf, sizeof(buf), "%8.2f", degH);
    Serial.print(buf);

    _term.setForegroundColor(WHITE);
    Serial.print(" | ");

    // Ref vertical
    _term.setForegroundColor(GREEN);
    snprintf(buf, sizeof(buf), "%9.2f", refV);
    Serial.print(buf);

    _term.setForegroundColor(WHITE);
    Serial.print(" | ");

    // Ref horizontal
    _term.setForegroundColor(GREEN);
    snprintf(buf, sizeof(buf), "%9.2f", refH);
    Serial.print(buf);

    _term.reset();
    Serial.print("\r\n");
}
