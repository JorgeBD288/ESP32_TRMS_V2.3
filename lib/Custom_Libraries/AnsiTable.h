#ifndef ANSI_TABLE_H
#define ANSI_TABLE_H

#include <Arduino.h>
#include <Ansiterm.h>

class AnsiTable {
public:
    AnsiTable(Ansiterm& term,
              uint32_t samplePeriodMs = 50,
              uint8_t rows = 20);

    void begin();
    void update(float degV, float degH);

private:
    void drawFrame();
    void clearRow(uint8_t row);
    void drawRow(uint8_t row, uint32_t tms, float vDeg, float hDeg);

    Ansiterm& _term;

    uint32_t _samplePeriod;
    uint8_t  _rows;

    uint32_t _lastTick;
    uint32_t _sampleNo;

    float* _vBuf;
    float* _hBuf;
    uint32_t* _tBuf;
};

#endif
