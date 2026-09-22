---
title: "Filtro inteligente: lógica de automatización"
description: "Umbral e histéresis por índice VOC, modos auto/on/off desde el portal, almacenamiento de configuración en NVS y publicación del estado del ventilador."
---

# Lógica de automatización

Conectamos todo: el sensor decide, el ventilador gira, el portal controla.

## 1. Estado y configuración

Al principio de `src/main.cpp`:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // índice VOC para activación
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) — memoria no volátil de ESP32: el modo y el umbral sobreviven a un reinicio.

## 2. Control del ventilador

Todo encendido y apagado lo concentramos en una función `setFan`. Toma un argumento `on` — el estado deseado: `true` = encender, `false` = apagar. En el resto del código siempre llamamos a `setFan(true)` / `setFan(false)`, y ella hace toda la rutina: tira del pin, recuerda el estado e informa al portal.

```cpp
static void setFan(bool on) {      // on — argumento: true = encender, false = apagar
    if (g_fanOn == on) return;     // ya en el estado deseado — no hacemos nada
    g_fanOn = on;                  // guardamos el nuevo estado en la variable global
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // físicamente encendemos/apagamos la llave del ventilador

    // Informamos el estado al núcleo: fanOn[0] — campo de telemetría del diccionario
    // (apareció porque hasFan = true; [0] — nuestra única unidad, como en el capítulo 5).
    // De aquí se irá a la nube y a la celda "Ventilador" de la tarjeta.
    s_link.telemetry.fanOn[0] = on;

    // Cambio de estado — razón para enviar telemetría de inmediato, sin esperar el período.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` hace el respuesta instantánea: presionaste en el portal — en un segundo la tarjeta muestra el estado confirmado. Exactamente confirmado: el portal de iDryer nunca "adivina" el estado, muestra lo que el dispositivo realmente envió.

## 3. Automatización con histéresis

Si enciendes el ventilador exactamente en el umbral, alrededor del umbral va a vibrar encendido/apagado. Se cura con brecha:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: encendemos en el umbral, apagamos 20 puntos debajo.
    if (g_vocIndex < 0) return;                      // sensor aún en silencio
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

Llamamos a `tickAutoLogic()` en el mismo lugar donde leemos el sensor — en `loop()` cada segundo con temporizador. Este es ese `loop()` del capítulo 5, se agrega una línea. Ahora completo se ve así:

```cpp
void loop() {
    s_link.loop();                        // red, telemetría, comandos — siempre primero

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // una vez por segundo:
        s_lastReadMs = now;
        readVocSensor();                  //   leemos VOC (capítulo 5)
        tickAutoLogic();                  //   e inmediatamente tomamos decisión sobre el ventilador
    }
}
```

El orden dentro del bloque de segundo no es accidental: primero lectura fresca del sensor, luego decisión por ella.

## 4. Callbacks desde el portal

Esas mismas funciones que prometimos en el [capítulo 6](06-card.md):

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // aplicamos de inmediato, no esperamos el siguiente tik
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

Estas funciones **reemplazan** las versiones de prueba del capítulo 6 — elimina las versiones vacías.

Observa lo que **no hay** en este código: análisis de MQTT, tópicos, comandos JSON. El usuario eligió `on` en la lista del portal → el núcleo recibió el comando, lo verificó y llamó a `onModeSelected("on")`. Toda la mecánica de transporte — responsabilidad del núcleo.

## 5. setup() final

Queda agregar dos cosas al `setup()`: carga de configuración guardada desde NVS (al principio, para que la lógica trabaje con ella de inmediato) y configuración del pin del ventilador. El `setup()` completo después de este capítulo se ve así:

```cpp
void setup() {
    Serial.begin(115200);

    // Configuración desde NVS: lo que el usuario elegía en ocasiones previas.
    s_prefs.begin("filter");   // abrir el espacio de nombres "filter" en NVS
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Los segundos argumentos getUChar/getInt — valores por defecto: se devolverán
    // en el primer inicio, cuando nada se ha guardado en NVS aún.

    pinMode(FAN_PIN, OUTPUT);  // pin de la llave del ventilador — en salida

    s_link.begin();
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetría: campo vocIndex propio (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Tarjeta: sensor + órganos de control (capítulo 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Diseño (capítulo 6, opcional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Verificación de escenarios

| Acción | Expectativa |
|---|---|
| Modo `auto`, soplar en el sensor | VOC crece, en el umbral se enciende el ventilador, la tarjeta muestra "Encendido" |
| El aire se limpió | por debajo de umbral−20 se apaga el ventilador automáticamente |
| Modo `on` desde el portal | el ventilador gira independientemente del VOC |
| Modo `off` desde el portal | el ventilador está apagado, VOC sigue mostrándo |
| Reinicio de la placa | el modo y umbral se guardaron |

## 7. Código completo: src/main.cpp entero

Todo el código de los capítulos 4–7, reunido en un archivo. Si algo no coincide con el tuyo — compara con este listado.

```cpp
// ============================================================
// Filtro de aire inteligente en idryer-core.
// SGP40 (VOC) + ventilador a través de MOSFET, modo auto/manual,
// control y tarjeta en el portal a través de card manifest.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>

// ── Pines ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // compuerta MOSFET del ventilador
// SDA=8, SCL=9 — se especifican en Wire.begin() abajo

// ── Pasaporte del dispositivo (capítulo 4) ────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // dispositivo no estándar
    .unitsCount        = 1,
    .hasFan            = true,        // única habilidad del diccionario
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── Estado (capítulo 7) ─────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // índice VOC para activación
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: la configuración sobrevive el reinicio

// ── Sensor VOC (capítulo 5) ────────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = sin datos aún

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // Índice: ~100 = aire normal, más alto = más sucio (máx 500).
    g_vocIndex = s_sgp.measureVocIndex();
}

// ── Ventilador (capítulo 7) ────────────────────────────────────
static void setFan(bool on) {         // on: true = encender, false = apagar
    if (g_fanOn == on) return;        // ya en estado deseado
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // campo del diccionario → nube → tarjeta
    s_link.publishTelemetryNow();     // cambio de estado — publicamos de inmediato
}

// ── Automatización con histéresis (capítulo 7) ─────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: encendemos en el umbral, apagamos 20 puntos debajo.
    if (g_vocIndex < 0) return;       // sensor aún en silencio
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Callbacks de comandos del portal (capítulos 6–7) ────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // aplicamos de inmediato
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: configuración, red, sensor, tarjeta ────────────────
void setup() {
    Serial.begin(115200);

    // Configuración desde NVS (segundos argumentos — valores por defecto).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, vinculación — todo adentro
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetría: agregamos campo vocIndex propio (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Tarjeta: sensor + órganos de control (capítulo 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Diseño de la tarjeta de fábrica (capítulo 6, opcional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: la red siempre, sensor y lógica una vez por segundo ─
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // red, telemetría, comandos — siempre primero

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // lectura fresca…
        tickAutoLogic();              // …e inmediatamente decisión por ella
    }
}
```
