---
title: "Conexión de sensores SHT31 y termistor a idryer-core"
description: "Lectura del sensor climático SHT31 y termistor calefactor en ESP32: rellenado de telemetría idryer-core y envío de datos al portal iDryer."
---

# Sensores

En esta página se conectan dos sensores y se envían sus datos al portal. Primero el SHT31 (clima del armario), luego el termistor (temperatura del calefactor). Este es el paso de "obtención de datos" antes de añadir lógica de control.

El principio de trabajo con el núcleo es simple: su código en `loop()` escribe lecturas frescas en los campos `s_link.telemetry.*`, y la fachada publica automáticamente al cloud cada `telemetryPeriodMs` del `Config`. No es necesario llamar a la publicación manualmente.

## Campos de telemetría

Para nuestro armario se utilizan tres campos (índice `[0]` — primera y única cámara):

| Campo | Qué almacena | Bandera en Config |
|-------|--------------|-------------------|
| `s_link.telemetry.airTempC[0]` | temperatura del aire, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | humedad del aire, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | temperatura del calefactor, °C | `hasHeaterTemp` |

Estas tres banderas se habilitan aquí mismo, en `Config` (véase el listado completo al final del capítulo). La bandera le indica al portal y a la aplicación que el dispositivo tiene ese sensor: sin ella la celda no aparece en la tarjeta.

## Regla: el código del sensor no debe bloquear loop()

La fachada `idryer-core` atiende Wi-Fi y MQTT en el mismo `loop()`. Por lo tanto, al leer sensores no se puede llamar a `delay()` — la pausa corta la sesión de red. El sensor se consulta por temporizador y el valor listo simplemente se lee. Los controladores listos del ecosistema ya están organizados así.

## Paso 1. SHT31: clima del armario

No es necesario escribir el controlador SHT31 desde cero — la clase lista `Sht31ClimateSensor` está en el ejemplo de este capítulo, [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet). Utiliza la biblioteca `robtillaart/SHT31` y lee el sensor sin bloqueo.

1. Agregue la biblioteca SHT31 a `lib_deps` de su `platformio.ini`:

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Copie en su carpeta `src/` los cuatro archivos del controlador:

    ```bash
    git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
    cp ~/byo-idryer/example/09-cabinet/src/{Sht31ClimateSensor.h,Sht31ClimateSensor.cpp,IClimateSensor.h,sensor_reading.h} src/
    ```

3. Conecte el sensor por I2C (véase [Esquema de conexión](03-wiring.md)) y léalo en `src/main.cpp`:

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — pines de su placa
    s_climateOk = s_climate.begin();  // encuentra automáticamente la dirección 0x44 o 0x45
    s_link.begin();
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
}
```

El controlador devuelve una sola instantánea de lecturas mediante la estructura `SensorReading` de `sensor_reading.h`:

```cpp
struct SensorReading {
    float    temperature = NAN;   // °C, NAN si no hay valor
    float    humidity    = NAN;   // % RH, NAN si no hay valor
    float    pressure    = NAN;   // hPa, para futuros sensores
    uint32_t ts_ms       = 0;     // millis() en el momento de la lectura
    bool     ok          = false; // true si la temperatura y la humedad son válidas
    int      err         = 0;     // código de error, 0 — sin error
};
```

Después de programar el firmware, en el portal aparecerán la temperatura y humedad del armario — esta es la primera retroalimentación del dispositivo.

## Paso 2. Termistor: temperatura del calefactor

No tengo una clase lista de termistor para ESP32, así que lo escribimos directamente en `src/main.cpp`. El termistor está conectado al pin de ADC a través de un convertidor de voltaje (véase [Esquema de conexión](03-wiring.md)): el controlador mide el voltaje en el punto intermedio, se calcula la resistencia del termistor y luego — la temperatura.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // pin de ADC
static const float SERIES_R   = 4700.0f;   // resistencia del divisor, Ω
static const float NOMINAL_R  = 100000.0f; // resistencia del termistor a 25 °C, Ω
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // coeficiente B del termistor en su hoja de datos

// Devuelve la temperatura del calefactor en °C.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 en ESP32
    float v   = (float)raw / 4095.0f;           // fracción de la escala completa
    float r   = SERIES_R * (1.0f - v) / v;      // resistencia del termistor, Ω
    // Ecuación de Steinhart–Hart en la forma del parámetro B — véase Wikipedia:
    // https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

En `loop()` escriba el resultado en telemetría junto con la lectura de SHT31:

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "Esta es lectura simplificada — ajuste los parámetros a su termistor"
    Las constantes `NOMINAL_R` y `BETA` dependen del termistor específico — extráigalas de su hoja de datos (termistor doméstico común — Generic 3950, `100 kΩ`). La fórmula del divisor corresponde al esquema en [Esquema de conexión](03-wiring.md): termistor a `3.3V`, resistencia a `GND`. Con otra disposición, la fórmula cambia. El ADC en ESP32 es no lineal, por lo que para mediciones precisas se calibran las lecturas — en los controladores de serie iDryer se utiliza una tabla de termistor para esto (biblioteca `Thermistor`).

Diagnóstico del termistor con multímetro — [Verificación del termistor](../06-practical-guides/02-checking-thermistor.md).

## Paso 3. ¿Sin sensores a mano? Modo demo

Se puede recorrer todo el camino hasta la tarjeta sin hardware: las lecturas las calcula un modelo del armario. Copie el archivo `demo_sensors.h` del mismo ejemplo:

```bash
cp ~/byo-idryer/example/09-cabinet/src/demo_sensors.h src/
```

y agregue a `platformio.ini` la bandera de compilación:

```ini
build_flags =
    -DDEMO_SENSORS=1
```

Ambas ramas están ocultas tras una única función, y `loop()` no sabe de dónde vinieron los valores:

```cpp
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    // lectura del SHT31 y del termistor — como arriba
#endif
}
```

El modelo se comporta como un armario real: la habitación oscila lentamente alrededor de `24 °C`, el calefactor encendido calienta el aire, apagado lo deja enfriarse, y durante el calentamiento la humedad baja. El modelo lee la potencia de calefacción de la telemetría, así que la lógica del capítulo [Control de calefacción](07-heating-control.md) ve la respuesta y la histéresis funciona. Las capturas de pantalla de esta sección se hicieron justo así.

Para un dispositivo real la bandera no se establece: entonces se compila la rama con los sensores reales.

## Archivo `src/main.cpp` completo después de este capítulo

A continuación — el archivo completo. Las nuevas líneas con respecto al capítulo anterior se marcan con `// ← capítulo 5`; el resto no cambió.

??? note "Antes — `src/main.cpp` después del capítulo 4"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // dispositivo propio: la tarjeta la arma el manifiesto
        .unitsCount        = 1,
        .telemetryPeriodMs = 5000,
        .statusPeriodMs    = 10000,
        .hardwareVersion   = "1.0",
        .firmwareVersion   = "0.1.0",
        .model             = "DIY Storage Cabinet",
    };
    static iDryer::Link s_link(CFG);

    void setup() {
        Serial.begin(115200);
        s_link.begin();
        // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← capítulo 5
#include <math.h>                  // ← capítulo 5
#include "Sht31ClimateSensor.h"    // ← capítulo 5
#include "demo_sensors.h"    // ← capítulo 5: lecturas sin sensores (-DDEMO_SENSORS=1)

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // dispositivo propio: la tarjeta la arma el manifiesto
    .unitsCount        = 1,
    .hasAirTemp        = true,     // ← capítulo 5
    .hasAirHumidity    = true,     // ← capítulo 5
    .hasHeaterTemp     = true,  // ← capítulo 5
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← capítulo 5: sensor climático SHT31
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← capítulo 5: termistor del calefactor
static const int   THERM_PIN  = 2;
static const float SERIES_R   = 4700.0f;
static const float NOMINAL_R  = 100000.0f;
static const float NOMINAL_T  = 25.0f;
static const float BETA       = 3950.0f;

static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);
    float v   = (float)raw / 4095.0f;
    float r   = SERIES_R * (1.0f - v) / v;
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}

// ← capítulo 5: sensores o, con -DDEMO_SENSORS=1, el modelo del armario
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();
#endif
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // ← capítulo 5  (SDA, SCL — pines de su placa)
    s_climateOk = s_climate.begin();  // ← capítulo 5
    s_link.begin();
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    readSensors();   // ← capítulo 5
}
```

## Verificación del resultado

![Página del dispositivo en el portal: lecturas y gráfico](../../img/09-cabinet/05-portal-device.png)
*Lecturas en la tarjeta y gráfico de telemetría. La temperatura del calefactor va como línea aparte en el gráfico. El menú al pie de la página todavía está vacío — de él se ocupa el capítulo siguiente.*

Después de este paso, el portal debe mostrar tres valores:

- temperatura del aire en el armario;
- humedad en el armario;
- temperatura del calefactor.

Si las lecturas "se desplazan" o son claramente incorrectas:

- compruebe la masa común y la disposición (ruido de cables de potencia) — [Errores de cableado](../08-common-mistakes/03-wiring-mistakes.md);
- compruebe la clasificación de la resistencia del divisor y el tipo de termistor;
- asegúrese de que el SHT31 responda por I2C (dirección correcta y líneas).

Diagnóstico "el sensor muestra tonterías" — [Verificación del termistor](../06-practical-guides/02-checking-thermistor.md) y [Errores típicos](../08-common-mistakes/01-overview.md).

## Qué sigue

Hay datos del sensor. Ahora describiremos la configuración del dispositivo (temperatura objetivo, histéresis) en [Menú de YAML](06-menu.md) para que se puedan cambiar desde el portal y almacenar en memoria.
