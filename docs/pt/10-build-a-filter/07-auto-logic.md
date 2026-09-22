---
title: "Filtro inteligente: lógica de automatização"
description: "Limiar e histerese por índice VOC, modos auto/on/off a partir do portal, guardar configurações em NVS e publicar estado do ventilador."
---

# Lógica de automatização

Conectamos tudo: sensor decide, ventilador gira, portal controla.

## 1. Estado e configurações

No início de `src/main.cpp`:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // índice VOC de activação
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) — memória não-volátil do ESP32: modo e limiar sobrevivem a reinicializações.

## 2. Controlo do ventilador

Reunimos toda ligação e desligação numa função `setFan`. Ela toma um argumento `on` — estado desejado: `true` = ligar, `false` = desligar. Adiante no código sempre chamamos `setFan(true)` / `setFan(false)`, e ela faz toda a rotina: muda o pino, memoriza o estado e informa o portal.

```cpp
static void setFan(bool on) {      // on — argumento: true = ligar, false = desligar
    if (g_fanOn == on) return;     // já no estado desejado — nada fazer
    g_fanOn = on;                  // memorizamos o novo estado na variável global
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // fisicamente ligamos/desligamos a chave do ventilador

    // Informamos o estado ao núcleo: fanOn[0] — campo de telemetria de dicionário
    // (apareceu porque hasFan = true; [0] — nosso único unit, como no capítulo 5).
    // Daqui vai para a nuvem e para a célula «Ventilador» do cartão.
    s_link.telemetry.fanOn[0] = on;

    // Mudança de estado — razão para enviar telemetria já, não esperar pelo período.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` torna a resposta instantânea: clicou no portal — em um segundo o cartão mostra o estado confirmado. Precisamente confirmado: o portal iDryer nunca «adivinha» o estado, mostra o que o dispositivo realmente enviou.

## 3. Automatização com histerese

Se ligarmos o ventilador exactamente no limiar, à volta do limiar ele vai oscilar entre lig/deslig. Corrige-se com um intervalo:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: ligamo-nos no limiar, desligamo-nos 20 pontos abaixo.
    if (g_vocIndex < 0) return;                      // sensor ainda está silencioso
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

Chamamos `tickAutoLogic()` onde lemos o sensor — em `loop()` com temporizador de segundo. Este é o `loop()` do capítulo 5, acrescenta-se uma linha. Completo fica agora assim:

```cpp
void loop() {
    s_link.loop();                        // rede, telemetria, comandos — sempre primeiro

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // uma vez por segundo:
        s_lastReadMs = now;
        readVocSensor();                  //   lemos VOC (capítulo 5)
        tickAutoLogic();                  //   e já decidimos sobre o ventilador
    }
}
```

A ordem dentro do bloco de segundo não é acidental: primeira leitura fresca do sensor, depois decisão sobre ela.

## 4. Callbacks do portal

Aquelas funções que prometemos no [capítulo 6](06-card.md):

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // aplicamos já, não esperamos pelo próximo tick
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

Estas funções **substituem** os stubs do capítulo 6 — delete as versões vazias.

Note o que **não há** neste código: desempacotamento de MQTT, tópicos, comandos JSON. O utilizador escolheu `on` na lista no portal → o núcleo recebeu o comando, verificou e chamou `onModeSelected("on")`. Toda a mecânica de transporte — responsabilidade do núcleo.

## 5. setup() final

Falta adicionar em `setup()` duas coisas: carregar as configurações guardadas em NVS (no início, para a lógica trabalhar logo com elas) e configurar o pino do ventilador. O `setup()` completo após este capítulo fica assim:

```cpp
void setup() {
    Serial.begin(115200);

    // Configurações de NVS: o que o utilizador escolheu outras vezes.
    s_prefs.begin("filter");   // abrir espaço de nomes "filter" em NVS
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Segundos argumentos de getUChar/getInt — valores por defeito: retornam
    // na primeira execução, quando NVS ainda está vazio.

    pinMode(FAN_PIN, OUTPUT);  // pino da chave do ventilador — para saída

    s_link.begin();
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetria: campo próprio vocIndex (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Cartão: sensor + órgãos de controlo (capítulo 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Layout (capítulo 6, opcional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Verificação de cenários

| Ação | Expectativa |
|---|---|
| Modo `auto`, respirar sobre o sensor | VOC cresce, no limiar ventilador liga, cartão mostra «Lig» |
| Ar se limpa | abaixo de limiar−20 ventilador desliga sozinho |
| Modo `on` a partir do portal | ventilador gira independentemente de VOC |
| Modo `off` a partir do portal | ventilador parado, VOC continua mostrando-se |
| Reinicializar a placa | modo e limiar guardaram-se |

## 7. Código final: src/main.cpp inteiro

Todo o código dos capítulos 4–7, reunido num ficheiro. Se algo não corresponder ao seu — compare com esta listagem.

```cpp
// ============================================================
// Filtro de ar inteligente em idryer-core.
// SGP40 (VOC) + ventilador através de MOSFET, modo auto/manual,
// controlo e cartão no portal através do manifesto de cartão.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>

// ── Pinos ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // gate MOSFET do ventilador
// SDA=8, SCL=9 — definem-se em Wire.begin() abaixo

// ── Passaporte do dispositivo (capítulo 4) ──────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // dispositivo não-standard
    .unitsCount        = 1,
    .hasFan            = true,        // única capacidade de dicionário
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── Estado (capítulo 7) ──────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // índice VOC de activação
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: configurações sobrevivem reinicialização

// ── Sensor VOC (capítulo 5) ──────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = sem dados ainda

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // Índice: ~100 = ar normal, superior = mais sujo (máx 500).
    g_vocIndex = s_sgp.measureVocIndex();
}

// ── Ventilador (capítulo 7) ──────────────────────────────────
static void setFan(bool on) {         // on: true = ligar, false = desligar
    if (g_fanOn == on) return;        // já no estado desejado
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // campo de dicionário → nuvem → cartão
    s_link.publishTelemetryNow();     // mudança de estado — publicamos já
}

// ── Automatização com histerese (capítulo 7) ──────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: ligamo-nos no limiar, desligamo-nos 20 pontos abaixo.
    if (g_vocIndex < 0) return;       // sensor ainda está silencioso
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Callbacks de comandos do portal (capítulos 6–7) ──────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // aplicamos já
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: configurações, rede, sensor, cartão ───────────────
void setup() {
    Serial.begin(115200);

    // Configurações de NVS (segundos argumentos — defeitos primeira execução).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, ligação — tudo dentro
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetria: adicionamos o campo próprio vocIndex (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Cartão: sensor + órgãos de controlo (capítulo 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Layout do cartão de fábrica (capítulo 6, opcional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: rede sempre, sensor e lógica uma vez por segundo ────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // rede, telemetria, comandos — sempre primeiro

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // leitura fresca…
        tickAutoLogic();              // …e já decisão sobre ela
    }
}
```
