---
title: "Ligação de sensores SHT31 e termistor a idryer-core"
description: "Leitura de sensor climático SHT31 e termistor do aquecedor em ESP32: preenchimento de telemetria idryer-core e saída de dados no portal iDryer."
---

# Sensores

Nesta página você liga dois sensores e fornece os seus dados ao portal. Primeiro SHT31 (clima do armário), depois termistor (temperatura do aquecedor). Este é o passo "obtive dados" antes de adicionar lógica de controlo.

O princípio de trabalho com o núcleo é simples: o seu código em `loop()` escreve leituras frescas em campos `s_link.telemetry.*`, e a fachada publica-os na nuvem a cada `telemetryPeriodMs` da `Config`. Não é necessário publicar manualmente.

## Campos de telemetria

Para o nosso armário usamos três campos (índice `[0]` - primeira e única câmara):

| Campo | O que armazena | Flag em Config |
|-------|----------------|----------------|
| `s_link.telemetry.airTempC[0]` | temperatura do ar, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | humidade do ar, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | temperatura do aquecedor, °C | `hasHeaterTemp` |

As três flags já activámos em [Config no passo anterior](04-firmware-start.md).

## Regra: o código do sensor não deve bloquear loop()

A fachada `idryer-core` serve Wi-Fi e MQTT no mesmo `loop()`. Portanto, ao ler sensores não pode chamar `delay()` - uma pausa quebra a sessão de rede. O sensor é pesquisado por um temporizador, e o valor pronto é apenas lido. Os drivers prontos do ecossistema já são construídos assim.

## Passo 1. SHT31: clima do armário

Não é necessário escrever driver SHT31 do zero - uma classe pronta `Sht31ClimateSensor` existe no exemplo `iDryer-Storage`. Usa a biblioteca `robtillaart/SHT31` e lê o sensor sem bloqueio.

1. Adicione a biblioteca SHT31 a `lib_deps` do seu `platformio.ini`:

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Copie para a sua pasta `src/` quatro ficheiros de `iDryer-Storage/src/storage/sensors/`: `Sht31ClimateSensor.h`, `Sht31ClimateSensor.cpp`, `IClimateSensor.h` e `sensor_reading.h`.

3. Ligue o sensor por I2C (veja [Esquema de ligação](03-wiring.md)) e leia-o em `src/main.cpp`:

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — pinos da sua placa
    s_climateOk = s_climate.begin();  // encontra automaticamente 0x44 ou 0x45
    s_link.begin();
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
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

A estrutura `SensorReading` (campos `ok`, `temperature`, `humidity`) é declarada em `sensor_reading.h`. Após o firmware no portal aparecerão temperatura e humidade do armário - essa é a primeira retroacção do dispositivo.

## Passo 2. Termistor: temperatura do aquecedor

Não tenho uma classe de termistor pronta para ESP32, portanto escrevemos directamente em `src/main.cpp`. O termistor está ligado a um pino ADC através de um divisor de tensão (veja [Esquema de ligação](03-wiring.md)): o controlador mede a tensão no ponto médio, calcula a resistência do termistor e depois a temperatura.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // pino ADC
static const float SERIES_R   = 4700.0f;   // resistor divisor, Ω
static const float NOMINAL_R  = 100000.0f; // resistência do termistor a 25 °C, Ω
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // coeficiente B da folha técnica do termistor

// Retorna temperatura do aquecedor em °C.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 em ESP32
    float v   = (float)raw / 4095.0f;           // fracção da escala completa
    float r   = SERIES_R * (1.0f - v) / v;      // resistência do termistor, Ω
    // Equação Steinhart-Hart (parâmetro B):
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

Em `loop()` escreva o resultado na telemetria junto da leitura SHT31:

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "Esta é leitura simplificada - ajuste os parâmetros para o seu termistor"
    As constantes `NOMINAL_R` e `BETA` dependem do termistor específico - pegue-as na sua folha técnica (termistor caseiro comum - Generic 3950, `100 kΩ`). A fórmula divisor corresponde ao esquema de [Esquema de ligação](03-wiring.md): termistor para `3,3V`, resistor para `GND`. Com outro encaminhamento a fórmula muda. ADC em ESP32 é não-linear, portanto para medições precisas as leituras são calibradas - nos controladores iDryer em série para isso usa-se tabela de termistor (biblioteca `Thermistor`).

Verificação de termistor com multímetro - [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md).

## Completo `src/main.cpp` após este capítulo

Abaixo está o ficheiro inteiro. Novas linhas em relação ao capítulo anterior são marcadas `// ← capítulo 5`; o resto não mudou.

??? note "O que era — `src/main.cpp` após o capítulo 4"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Dryer,
        .unitsCount        = 1,
        .hasHeater         = true,
        .hasFan            = true,
        .hasAirTemp        = true,
        .hasAirHumidity    = true,
        .hasHeaterTemp     = true,
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
        // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
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

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    .hasHeater         = true,
    .hasFan            = true,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = true,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← capítulo 5: sensor de clima SHT31
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← capítulo 5: termistor do aquecedor
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

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // ← capítulo 5  (SDA, SCL — pinos da sua placa)
    s_climateOk = s_climate.begin();  // ← capítulo 5
    s_link.begin();
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    if (s_climateOk) {                                       // ← capítulo 5
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();     // ← capítulo 5
}
```

## Verificação de resultado

Após este passo três valores devem ser exibidos no portal:

- temperatura do ar no armário;
- humidade no armário;
- temperatura do aquecedor.

Se as leituras "flutuam" ou são claramente falsas:

- verifique terra comum e encaminhamento (interferência de fios de potência) - [Erros de fiação](../08-common-mistakes/03-wiring-mistakes.md);
- verifique valor nominal do resistor divisor e tipo de termistor;
- certifique-se de que SHT31 responde por I2C (endereço correcto e linhas).

Diagnóstico "o sensor mostra disparates" - [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md) e [Erros típicos](../08-common-mistakes/01-overview.md).

## O que vem a seguir

Tem dados dos sensores. Agora descreveremos as configurações do dispositivo (temperatura-alvo, histerese) em [Menu de YAML](06-menu.md), para que possam ser alteradas do portal e armazenadas em memória.
