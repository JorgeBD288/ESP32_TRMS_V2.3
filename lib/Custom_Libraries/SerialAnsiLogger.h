#ifndef SERIAL_ANSI_LOGGER_H
#define SERIAL_ANSI_LOGGER_H

#include <Arduino.h>
#include <Ansiterm.h>

class SerialAnsiLogger {
public:
    SerialAnsiLogger(Ansiterm& term, uint32_t samplePeriodMs = 50);

    void setPeriod(uint32_t samplePeriodMs);

    // Inicializa el logger
    // withHeader = true → imprime cabecera una vez
    void begin(bool withHeader = true);

    // Imprime UNA FILA: medidas + consignas (scroll infinito)
    void update(float degV, float degH, float refV, float refH);

private:
    void printHeader();

private:
    Ansiterm& _term;
    uint32_t  _period;
    uint32_t  _lastTick;
    uint32_t  _sampleNo;
};

#endif
