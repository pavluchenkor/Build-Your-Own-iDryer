---
title: "Controlo de aquecimento do armário: manutenção de temperatura e ventoinha"
description: "Lógica de armário aquecido em idryer-core: manutenção de temperatura-alvo por histerese, protecção do aquecedor por termistor, ventoinha e comandos do portal."
---

# Controlo de aquecimento

Nesta página você liga sensores, configurações e secção de potência em lógica funcional. O dispositivo mantém a temperatura estabelecida no armário, protege o aquecedor contra sobreaquecimento e responde a comandos do portal.

A lógica é executada em `loop()` junto à manutenção de rede. Todos os temporizadores e limiares são sem bloqueio, sem `delay()`.

## O que deve acontecer

O comportamento do armário é composto por três regras simples:

1. **Manutenção de temperatura.** Se o ar no armário é mais frio que o alvo pela quantidade de histerese - ligar aquecimento. Quando chegou ao alvo - desligar.
2. **Protecção do aquecedor.** O termistor controla o próprio aquecedor. Se sobreaqueceu acima do permitido - o aquecimento é desligado independentemente da temperatura do ar.
3. **Ventoinha.** Ligada para distribuir calor pelo armário, e desligada quando o aquecimento não é necessário.

## Chaves do aquecedor e ventoinha

O aquecedor e ventoinha o controlador liga via chave: módulo MOSFET (versão A) ou SSR (versão B) - veja [Esquema de ligação](03-wiring.md). Do ponto de vista do código é apenas um pino GPIO: `HIGH` - ligado, `LOW` - desligado.

Descreveremos tal chave com uma pequena estrutura e criaremos dois casos - para aquecedor e ventoinha. Adicione isto a `src/main.cpp` (antes de `setup()`):

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — controlo do aquecedor
static GpioOutput myFan{5};      // GPIO5 — controlo da ventoinha
```

Os números de pinos são os mesmos de [Esquema de ligação](03-wiring.md). Em `setup()` ambas as chaves devem ser inicializadas: `myHeater.begin();` e `myFan.begin();`.

!!! warning "Estado seguro no arranque"
    `begin()` imediatamente coloca `LOW` - aquecedor e ventoinha estão desligados até a lógica decidir o contrário. Isto é importante: ao ligar a alimentação o aquecedor não deve acabar acidentalmente ligado.

## Manutenção de temperatura por histerese

Para armário a `40-45 °C` é suficiente histerese simples: aquecimento liga e desliga em torno do alvo. Isto é mais simples que PID completo e para manutenção suave de calor funciona de forma fiável.

A histerese vem do menu (`menu.hysteresis`) — já ligado no [capítulo 6](06-menu.md). A temperatura alvo é definida pelo utilizador ao arrancar o armário a partir do cartão do dispositivo (`s_targetC`; o cartão é ligado mais adiante neste capítulo). Só se aquece em modo Storage. Adicione o estado e a função de decisão:

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // alvo do arranque atual, do cartão

static void controlLoop() {
    // Aquecer só em modo Storage: depois de Parar, o armário arrefece.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // do cartão
    float hyst   = (float)menu.hysteresis;           // do menu

    if (air < target - hyst) {
        s_heating = true;     // arrefecemos — aquecemos
    } else if (air >= target) {
        s_heating = false;    // chegámos ao alvo — parar
    }
}
```

A temperatura alvo chega com o comando de arranque a partir do cartão; os seus limites e o valor por omissão são o item `target_temp` do [menu](06-menu.md).

## Protecção do aquecedor por termistor

O ar aquece lentamente, mas a espiral do aquecedor aquece rapidamente. Sem controlo separado o aquecedor terá tempo para sobrequecer antes que o ar chegue ao alvo. Portanto o termistor do aquecedor estabelece um limite duro.

```cpp
static const float HEATER_MAX_C = 80.0f;   // limite de temperatura do aquecedor

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // termistor

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // reflectir em telemetria
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "O limite do aquecedor é protecção, não configuração climática"
    `HEATER_MAX_C` limita a temperatura do próprio aquecedor, não do ar. O valor depende da construção do aquecedor e materiais da carcaça. Escolha-o com margem abaixo da temperatura em que deformam os detalhes impressos - veja [Materiais à prova de calor](../07-3d-printing/04-heat-resistant-materials.md).

Para aquecimento mais suave em vez de ligar/desligar "tudo ou nada" pode controlar potência através de PWM e o campo `heaterPower01[0]` aceita valores de `0,0` a `1,0`. Para armário com manutenção suave de calor a lógica simples acima normalmente é suficiente.

## Ventoinha

A ventoinha distribui calor pelo armário. A lógica mais simples - ligá-la junto com o aquecimento:

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // girar enquanto aquecemos
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // reflectir em telemetria
}
```

No controlador em série a ventoinha é controlada por temperatura com limiares separados de ligação e desligação (por exemplo, ligação a `55 °C`, desligação a `35 °C`), para não se mexer na borda. Para armário pode-se aplicar a mesma abordagem, ligando os limiares aos parâmetros do menu.

## Montagem em loop()

```cpp
void loop() {
    s_link.loop();          // rede e autopublicação

    // sensores (veja passo «Sensores»):
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // decidimos, aquecer ou não
    applyHeater();   // aplicamos ao aquecedor + protecção
    applyFan();      // aplicamos à ventoinha
}
```

Os campos de telemetria (`heaterPower01`, `fanOn`) são publicados pela fachada - no portal vê-se se o dispositivo está a aquecer neste momento e se a ventoinha funciona.

## Cartão: arranque e paragem

O arranque e a paragem vêm do cartão do dispositivo no portal e na aplicação. O firmware declara-os como **ações** do cartão: o core adiciona-as ao card manifest, e o portal e a aplicação desenham o formulário e os botões. Não é preciso analisar comandos no seu código — o core chama a sua função.

Os limites do campo de temperatura e o valor por omissão vêm do item do menu `target_temp` (30–50 °C, 45) através da ponte `card_menu_bridge.h`. O valor introduzido pelo utilizador segue com o comando de arranque e não é escrito no menu. Adicione o cabeçalho junto aos cabeçalhos do menu do capítulo 6:

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

Em `setup()`, depois dos comandos do menu do capítulo 6, declare as ações. Os valores do menu já estão na cache que o cartão lê: `setup()` chama `menu_sync_state_to_cache()` desde o capítulo 6.

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` e `"IDLE"` — o modo da unidade depois da ação. Enquanto o armário está em repouso, o cartão mostra o formulário de arranque; em modo `STORAGE`, o bloco de sessão e o botão Parar.
- `MENU_TARGET_TEMP` — o id do item `target_temp`; o gerador coloca-o em `menu_ids.h`.
- `s_link.status.mode[0]` e `targetTempC[0]` mostram o estado atual da câmara. Chame `publishStatusNow()` após cada alteração para o cartão mudar de imediato.
- `iDryer::UnitMode::Storage` — modo de manutenção suave do calor. É o modo principal do armário.
- Altere a temperatura de armazenamento no menu do dispositivo no portal — o valor por omissão do campo do cartão acompanha-a: o core deteta sozinho a alteração do menu e volta a publicar o manifesto.

O core adiciona ao card manifest:

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

No portal, o cartão do armário em repouso recebe o campo `Temp.` com 45 °C e o botão `Armazenamento`; depois do arranque, o bloco de sessão com o alvo e o botão `Parar`. Na aplicação, o ecrã inicial mostra as leituras e a sessão em curso; o arranque e a paragem estão na página do dispositivo. Os sensores, campos e a disposição do cartão são tratados no capítulo [Cartão do dispositivo](../10-build-a-filter/06-card.md) da secção do filtro de ar.

!!! warning "Nada de delay() nos callbacks"
    Os callbacks das ações são chamados a partir do tratador de rede. Qualquer bloqueio lá dentro quebra a sessão MQTT. Altere o alvo e o estado; o trabalho real faz-se em `loop()`.

## Completo `src/main.cpp` após este capítulo

Este é o ficheiro final e completo do dispositivo. Novas linhas relativas ao capítulo anterior são marcadas `// ← capítulo 7`. Este mesmo ficheiro está como exemplo pronto na pasta `example/09-cabinet/` do repositório e é compilado com `pio run -e cabinet`.

??? note "O que era — `src/main.cpp` após o capítulo 6"

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
        // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← capítulo 6
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← capítulo 6
    }

    void loop() {
        s_link.loop();

        // ← capítulo 6: publicar o menu ao ficar online e a pedido
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

// ← capítulo 7: chaves do aquecedor e ventoinha
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
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
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

- o botão `Armazenamento` do cartão do dispositivo põe o armário em modo Storage com a temperatura introduzida, o dispositivo começa a aquecer;
- a temperatura do ar aumenta para o alvo e é mantida dentro da histerese;
- o aquecedor não sai acima de `HEATER_MAX_C`;
- ventoinha e potência de aquecimento são vistos em telemetria;
- o botão `Parar` desliga o aquecimento e passa a Idle; até ao próximo arranque o armário não aquece.

## O que vem a seguir

A lógica está pronta. Resta montar o dispositivo na carcaça e verificar sob funcionamento - [Montagem e verificação](08-assembly-and-check.md).
