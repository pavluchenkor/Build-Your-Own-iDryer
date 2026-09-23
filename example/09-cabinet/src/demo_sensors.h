// Демо-датчики: показания шкафа без железа (сборка с -DDEMO_SENSORS=1).
//
// Простая модель: шкаф стоит в комнате, температура которой медленно
// колеблется около 24 °C. Включённый нагреватель быстро разогревается и
// греет воздух, выключенный — остывает, и шкаф возвращается к комнатной.
// Влажность падает, когда воздух теплеет. Модель читает мощность нагрева из
// телеметрии, поэтому логика главы 7 видит отклик, как на настоящем шкафу.
#pragma once

#include <Arduino.h>
#include <math.h>
#include <iDryer.h>

inline void demoSensors(iDryer::Telemetry& t) {
    static float    airC    = 24.0f;   // воздух в шкафу
    static float    heaterC = 24.0f;   // корпус нагревателя
    static uint32_t lastMs  = 0;

    const uint32_t now = millis();
    if (lastMs != 0 && now - lastMs < 1000) return;   // шаг модели — раз в секунду
    const float dt = lastMs ? (now - lastMs) / 1000.0f : 1.0f;
    lastMs = now;

    const float room = 24.0f + sinf(now / 600000.0f * 2.0f * PI);   // 23..25 °C, период 10 мин
    const bool  on   = t.heaterPower01[0] > 0.0f;

    heaterC += ((on ? 70.0f : airC) - heaterC) * dt / 20.0f;           // нагреватель — за секунды
    airC    += (heaterC - airC) * dt / 300.0f + (room - airC) * dt / 900.0f;  // воздух — за минуты

    t.airTempC[0]       = airC;
    t.airHumidityPct[0] = 50.0f - (airC - 24.0f) * 0.8f + 2.0f * sinf(now / 420000.0f * 2.0f * PI);
    t.heaterTempC[0]    = heaterC;
}
