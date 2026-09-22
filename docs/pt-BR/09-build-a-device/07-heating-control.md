---
title: "Controle de aquecimento do gabinete: manutenção de temperatura e ventilador"
description: "Lógica do gabinete aquecido em idryer-core: manutenção de temperatura-alvo por histerese, proteção do aquecedor por termistor, ventilador e comandos do portal."
---

# Controle de aquecimento

Nesta página você conecta sensores, configurações e a parte de potência em uma lógica funcional. O dispositivo mantém a temperatura especificada no gabinete, protege o aquecedor de superaquecimento e responde aos comandos do portal.

A lógica é executada em `loop()` perto do atendimento de rede. Todos os temporizadores e limites são não-bloqueantes, sem `delay()`.

## O que deve acontecer

O comportamento do gabinete consiste em três regras simples:

1. **Manutenção de temperatura.** Se o ar no gabinete está mais frio que a meta na quantidade de histerese — ligar aquecimento. Quando atingiu a meta — desligar.
2. **Proteção do aquecedor.** O termistor controla o próprio aquecedor. Se ele superaqueceu acima do permitido — o aquecimento desliga independentemente da temperatura do ar.
3. **Ventilador.** Liga para dispersar o calor pelo gabinete e desliga quando o aquecimento não é necessário.

## Chaves de aquecedor e ventilador

O controlador liga o aquecedor e ventilador através de uma chave: módulo MOSFET (versão A) ou SSR (versão B) — veja [Esquema de conexão](03-wiring.md). Do ponto de vista do código, é apenas um pino GPIO: `HIGH` — ligado, `LOW` — desligado.

Descrever tal chave com uma pequena estrutura e ter duas instâncias — para o aquecedor e ventilador. Adicione isto a `src/main.cpp` (antes de `setup()`):

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — controle do aquecedor
static GpioOutput myFan{5};      // GPIO5 — controle do ventilador
```

Os números de pino são os mesmos de [Esquema de conexão](03-wiring.md). Em `setup()` ambas as chaves devem ser inicializadas: `myHeater.begin();` e `myFan.begin();`.

!!! warning "Estado seguro no início"
    `begin()` imediatamente define `LOW` — aquecedor e ventilador desligados, até a lógica decidir o contrário. Isto é importante: ao ligar a energia, o aquecedor não deve ficar aceso acidentalmente.

## Manutenção de temperatura por histerese

Para um gabinete em `40–45 °C`, histerese simples é suficiente: o aquecimento liga e desliga em torno da meta. É mais simples que PID completo e para manutenção suave de calor funciona de forma confiável.

A histerese vem do menu (`menu.hysteresis`) — já conectado no [capítulo 6](06-menu.md). A temperatura alvo é definida pelo usuário ao iniciar o gabinete pelo cartão do dispositivo (`s_targetC`; o cartão é conectado mais adiante neste capítulo). Só aquece no modo Storage. Adicione o estado e a função de decisão:

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // alvo da execução atual, do cartão

static void controlLoop() {
    // Aquecer só no modo Storage: depois de Parar, o gabinete esfria.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // do cartão
    float hyst   = (float)menu.hysteresis;           // do menu

    if (air < target - hyst) {
        s_heating = true;     // esfriou — aqueça
    } else if (air >= target) {
        s_heating = false;    // atingiu meta — parar
    }
}
```

A temperatura alvo chega com o comando de partida vindo do cartão; seus limites e o valor padrão são o item `target_temp` do [menu](06-menu.md).

## Proteção do aquecedor por termistor

O ar aquece lentamente, mas a espiral do aquecedor — rapidamente. Sem controle separado, o aquecedor terá tempo de superaquecer antes de o ar atingir a meta. Portanto, o termistor do aquecedor define um limite rígido.

```cpp
static const float HEATER_MAX_C = 80.0f;   // limite de temperatura do aquecedor

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // termistor

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // refletir na telemetria
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "Limite do aquecedor — é proteção, não configuração de clima"
    `HEATER_MAX_C` limita a temperatura do próprio aquecedor, não do ar. O valor depende do design do aquecedor e dos materiais do gabinete. Escolha com margem abaixo da temperatura em que peças plásticas deformam — veja [Materiais termoresistentes](../07-3d-printing/04-heat-resistant-materials.md).

Para aquecimento mais suave em vez de ligar/desligar "tudo ou nada", você pode controlar potência via PWM, e o campo `heaterPower01[0]` aceita valores de `0.0` para `1.0`. Para um gabinete com manutenção suave de calor, a lógica simples acima geralmente é suficiente.

## Ventilador

O ventilador dispersa o calor pelo gabinete. Lógica mais simples — ligá-lo junto com o aquecimento:

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // girar enquanto aquece
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // refletir na telemetria
}
```

No controlador em série, o ventilador é controlado pela temperatura com limites separados de ligação e desligação (por exemplo, liga a `55 °C`, desliga a `35 °C`), para que ele não oscile no limite. Para um gabinete você pode aplicar a mesma abordagem, vinculando os limites a parâmetros do menu.

## Montando em loop()

```cpp
void loop() {
    s_link.loop();          // rede e autopublicação

    // sensores (veja passo "Sensores"):
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // decidir se aquece ou não
    applyHeater();   // aplicar ao aquecedor + proteção
    applyFan();      // aplicar ao ventilador
}
```

Os campos de telemetria (`heaterPower01`, `fanOn`) a fachada publica automaticamente — no portal você vê se o dispositivo está aquecendo agora e se o ventilador está funcionando.

## Cartão: partida e parada

A partida e a parada vêm do cartão do dispositivo no portal e no app. O firmware as declara como **ações** do cartão: o core as adiciona ao card manifest, e o portal e o app desenham o formulário e os botões. Não é preciso interpretar comandos no seu código — o core chama a sua função.

Os limites do campo de temperatura e o valor padrão vêm do item de menu `target_temp` (30–50 °C, 45) pela ponte `card_menu_bridge.h`. O valor digitado pelo usuário vai junto com o comando de partida e não é gravado no menu. Adicione o cabeçalho ao lado dos cabeçalhos do menu do capítulo 6:

```cpp
#include <card/card_menu_bridge.h>
```

Callbacks das ações — antes de `setup()`:

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // já dentro de 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}
```

Em `setup()`, depois dos comandos do menu do capítulo 6, declare as ações. Os valores do menu já estão no cache que o cartão lê: `setup()` chama `menu_sync_state_to_cache()` desde o capítulo 6.

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` e `"IDLE"` — o modo da unidade depois da ação. Enquanto o gabinete está ocioso, o cartão mostra o formulário de partida; no modo `STORAGE`, o bloco de sessão e o botão Parar.
- `MENU_TARGET_TEMP` — o id do item `target_temp`; o gerador o coloca em `menu_ids.h`.
- `s_link.status.mode[0]` e `targetTempC[0]` mostram o estado atual da câmara. Chame `publishStatusNow()` depois de cada mudança para o cartão trocar na hora.
- `iDryer::UnitMode::Storage` — modo de manutenção suave do calor. É o modo principal do gabinete.
- Mude a temperatura de armazenamento no menu do dispositivo no portal — o valor padrão do campo do cartão acompanha: o core percebe sozinho a mudança do menu e publica o manifesto de novo.

O core adiciona ao card manifest:

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

No portal, o cartão do gabinete ocioso ganha o campo `Temp.` com 45 °C e o botão `Armazenamento`; depois da partida, o bloco de sessão com o alvo e o botão `Parar`. No app, a tela inicial mostra as leituras e a sessão em andamento; partida e parada ficam na página do dispositivo. Sensores, campos e layout do cartão são tratados no capítulo [Cartão do dispositivo](../10-build-a-filter/06-card.md) da seção do filtro de ar.

!!! warning "Nada de delay() nos callbacks"
    Os callbacks das ações são chamados pelo handler de rede. Qualquer bloqueio dentro deles derruba a sessão MQTT. Mude o alvo e o status; o trabalho de verdade fica no `loop()`.

## Completo `src/main.cpp` após este capítulo

Este é o arquivo final e completo do dispositivo. Novas linhas em relação ao capítulo anterior são marcadas `// ← capítulo 7`. Este mesmo arquivo está como exemplo pronto na pasta `example/09-cabinet/` do repositório e é compilado com `pio run -e cabinet`.

??? note "O que foi — `src/main.cpp` após o capítulo 6"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include <menu_state.h>                      // ← capítulo 6: parâmetros (menu.target_temp …)
    #include <menu_bindings.h>                   // ← capítulo 6: menu_apply_by_bind
    #include <menu_commands.h>                   // ← capítulo 6: menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← capítulo 6: publishConfigRaw

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

    static Sht31ClimateSensor s_climate(&Wire);
    static bool               s_climateOk = false;

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

    // ← capítulo 6: menu no portal
    static bool s_menuPending = false;

    static void publishMenu() {
        static char buf[MENU_FULL_JSON_BUF_SIZE];
        const size_t len = menu_buildFullJson(buf, sizeof(buf));
        if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
    }

    static void applySet(JsonObjectConst data) {
        const int id = data["id"] | -1;
        float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                         : data["val"].as<float>();
        for (uint16_t i = 0; i < g_bindings_count; i++) {
            if ((int)g_bindings[i].id != id) continue;
            const MenuMeta& m = g_menu_meta[id];
            if (v < m.min_val) v = m.min_val;
            if (v > m.max_val) v = m.max_val;
            menu_apply_by_bind(g_bindings[i].bind, v);
            s_menuPending = true;
            return;
        }
    }

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        menu.initDefaults();                     // ← capítulo 6
        menu.loadFromNVS();                      // ← capítulo 6
        menu_sync_state_to_cache();              // ← capítulo 6
        s_link.begin();
        // Dispositivo desvinculado no portal: apagar o segredo e aguardar nova vinculação.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← capítulo 6
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← capítulo 6
    }

    void loop() {
        s_link.loop();

        // ← capítulo 6: publicar o menu ao ficar online e sob pedido
        static bool s_wasOnline = false;
        const bool online = s_link.isOnline();
        if (online && !s_wasOnline) s_menuPending = true;
        s_wasOnline = online;
        if (s_menuPending) {
            s_menuPending = false;
            publishMenu();
        }

        if (s_climateOk) {
            s_climate.tick(millis());
            SensorReading r = s_climate.get();
            if (r.ok) {
                s_link.telemetry.airTempC[0]       = r.temperature;
                s_link.telemetry.airHumidityPct[0] = r.humidity;
            }
        }
        s_link.telemetry.heaterTempC[0] = readHeaterTempC();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>
#include <math.h>
#include "Sht31ClimateSensor.h"
#include <menu_state.h>
#include <menu_bindings.h>
#include <menu_commands.h>
#include <local_access/device_publisher.h>
#include <card/card_menu_bridge.h>        // ← capítulo 7

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

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

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

static bool s_menuPending = false;

static void publishMenu() {
    static char buf[MENU_FULL_JSON_BUF_SIZE];
    const size_t len = menu_buildFullJson(buf, sizeof(buf));
    if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
}

static void applySet(JsonObjectConst data) {
    const int id = data["id"] | -1;
    float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                     : data["val"].as<float>();
    for (uint16_t i = 0; i < g_bindings_count; i++) {
        if ((int)g_bindings[i].id != id) continue;
        const MenuMeta& m = g_menu_meta[id];
        if (v < m.min_val) v = m.min_val;
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);
        s_menuPending = true;
        return;
    }
}

// ← capítulo 7: chaves do aquecedor e ventilador
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← capítulo 7: lógica de manutenção de temperatura
static bool        s_heating    = false;
static float       s_targetC    = 0.0f;
static const float HEATER_MAX_C = 80.0f;

static void controlLoop() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) { s_heating = false; return; }
    float air    = s_link.telemetry.airTempC[0];
    float target = s_targetC;
    float hyst   = (float)menu.hysteresis;
    if (air < target - hyst)  s_heating = true;
    else if (air >= target)   s_heating = false;
}

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];
    bool  allow = s_heating && heaterTemp < HEATER_MAX_C;
    if (allow) myHeater.on(); else myHeater.off();
    s_link.telemetry.heaterPower01[0] = allow ? 1.0f : 0.0f;
}

static void applyFan() {
    if (s_heating) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = s_heating;
}

// ← capítulo 7: ações do cartão
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);
    s_climateOk = s_climate.begin();
    myHeater.begin();              // ← capítulo 7
    myFan.begin();                 // ← capítulo 7
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // Dispositivo desvinculado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← capítulo 7
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}

void loop() {
    s_link.loop();

    static bool s_wasOnline = false;
    const bool online = s_link.isOnline();
    if (online && !s_wasOnline) s_menuPending = true;
    s_wasOnline = online;
    if (s_menuPending) {
        s_menuPending = false;
        publishMenu();
    }

    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // ← capítulo 7
    applyHeater();   // ← capítulo 7
    applyFan();      // ← capítulo 7
}
```

## Verificação de resultado

Após este passo:

- o botão `Armazenamento` no cartão do dispositivo coloca o gabinete no modo Storage com a temperatura digitada, e o dispositivo começa a aquecer;
- a temperatura do ar sobe até a meta e é mantida dentro da histerese;
- o aquecedor não ultrapassa `HEATER_MAX_C`;
- ventilador e potência de aquecimento são visíveis na telemetria;
- o botão `Parar` desliga o aquecimento e passa para Idle; até a próxima partida o gabinete não aquece.

## O que vem a seguir

A lógica está pronta. Falta montar o dispositivo no gabinete e verificar sob energia — [Montagem e verificação](08-assembly-and-check.md).
