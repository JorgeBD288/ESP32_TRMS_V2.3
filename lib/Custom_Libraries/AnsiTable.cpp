#include "AnsiTable.h"

AnsiTable::AnsiTable(Ansiterm& term, uint32_t samplePeriodMs, uint8_t rows)
    : _term(term),
      _samplePeriod(samplePeriodMs),
      _rows(rows),
      _lastTick(0),
      _sampleNo(0)
{
    _vBuf = new float[_rows];
    _hBuf = new float[_rows];
    _tBuf = new uint32_t[_rows];
}

void AnsiTable::begin() {
    drawFrame();
    for (uint8_t r = 0; r < _rows; r++) clearRow(r);
    _lastTick = millis();
}

void AnsiTable::update(float degV, float degH) {
    uint32_t now = millis();
    if (now - _lastTick < _samplePeriod) return;
    _lastTick += _samplePeriod;

    uint8_t idx = _sampleNo % _rows;

    _tBuf[idx] = _sampleNo * _samplePeriod;
    _vBuf[idx] = degV;
    _hBuf[idx] = degH;

    _sampleNo++;

    for (uint8_t r = 0; r < _rows; r++) {
        int32_t n = (int32_t)_sampleNo - (int32_t)_rows + (int32_t)r;

        if (n < 0) {
            clearRow(r);
            continue;
        }

        uint8_t bi = (uint32_t)n % _rows;
        drawRow(r, _tBuf[bi], _vBuf[bi], _hBuf[bi]);
    }
}

void AnsiTable::drawFrame() {
    _term.reset();
    _term.eraseScreen();
    _term.home();

    // Cabecera
    _term.setForegroundColor(WHITE);
    _term.boldOn();
    _term.xy(1, 1);
    Serial.print("   Tiempo (s) |     V (deg) |     H (deg)");
    _term.boldOff();

    _term.xy(1, 2);
    Serial.print("------------------------------------------");
}

void AnsiTable::clearRow(uint8_t row) {
    uint8_t y = 3 + row;
    _term.xy(1, y);
    _term.eraseLine(); // limpia la línea actual

    // deja una línea “en blanco” (opcional)
    _term.xy(1, y);
    Serial.print(" "); // asegura que algo se imprima
}

void AnsiTable::drawRow(uint8_t row, uint32_t tms, float vDeg, float hDeg) {
    char buf[20];
    uint8_t y = 3 + row;

    _term.xy(1, y);
    _term.eraseLine();

    // Tiempo (cyan)
    _term.setForegroundColor(CYAN);
    snprintf(buf, sizeof(buf), "%10.2f", tms / 1000.0f);
    Serial.print(buf);

    // Separador
    _term.setForegroundColor(WHITE);
    Serial.print(" | ");

    // Vertical (magenta)
    _term.setForegroundColor(MAGENTA);
    snprintf(buf, sizeof(buf), "%10.2f", vDeg);
    Serial.print(buf);

    // Separador
    _term.setForegroundColor(WHITE);
    Serial.print(" | ");

    // Horizontal (yellow)
    _term.setForegroundColor(YELLOW);
    snprintf(buf, sizeof(buf), "%10.2f", hDeg);
    Serial.print(buf);

    _term.reset();
}
