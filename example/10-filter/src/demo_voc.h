// Демо-датчик: индекс VOC без железа (сборка с -DDEMO_VOC=1).
//
// Простая модель комнаты, в которой печатает принтер. Пока фильтр стоит,
// испарения копятся и индекс ползёт вверх от фонового значения; когда
// вентилятор гонит воздух через фильтр, индекс падает. Модель смотрит на
// состояние вентилятора, поэтому автоматика главы 7 видит отклик и порог с
// гистерезисом работает, как на настоящем фильтре.
#pragma once

#include <Arduino.h>
#include <math.h>

inline int32_t demoVocIndex(bool fanOn) {
    static float    voc    = 100.0f;   // индекс: ~100 — обычный воздух
    static uint32_t lastMs = 0;

    const uint32_t now = millis();
    if (lastMs != 0 && now - lastMs < 1000) return (int32_t)voc;  // шаг модели — раз в секунду
    const float dt = lastMs ? (now - lastMs) / 1000.0f : 1.0f;
    lastMs = now;

    const float background = 100.0f + 3.0f * sinf(now / 300000.0f * 2.0f * PI);  // фон, период 5 мин
    voc += (fanOn ? -2.0f : 0.8f) * dt;                 // принтер пачкает, фильтр чистит
    voc += (background - voc) * dt / 120.0f;            // тяга к фоновому уровню
    voc += 0.5f * (float)random(-100, 101) / 100.0f;    // дрожь показаний датчика

    if (voc < 80.0f)  voc = 80.0f;
    if (voc > 400.0f) voc = 400.0f;
    return (int32_t)voc;
}
