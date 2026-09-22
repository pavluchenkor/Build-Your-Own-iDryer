---
title: "Menu do dispositivo de YAML: configurações em NVS e no portal"
description: "Como descrever o menu do dispositivo em idryer-core em menu.yaml: temperatura-alvo e histerese são armazenadas em NVS e exibidas no menu do dispositivo no portal iDryer."
---

# Menu de YAML

Menu é um conjunto de configurações do dispositivo: temperatura-alvo, histerese, limiares de ventoinha. Em `idryer-core` o menu é descrito num único ficheiro `menu.yaml`, e tudo o resto - estruturas C++, armazenamento em memória não-volátil (NVS) e publicação no portal - é gerado automaticamente.

Este é um dos blocos-chave do núcleo. Não escreve código de armazenamento de configurações nem inventa formato para o portal - apenas lista parâmetros em YAML.

## Por que menu

Após os passos anteriores o dispositivo lê sensores, mas todos os limiares estão "cosidos" no código. O menu resolve três tarefas simultaneamente:

- **armazenamento**: valores sobrevivem a reinicializações (NVS);
- **gestão do portal**: o portal mostra cada item do menu pelo seu tipo (número, interruptor);
- **fonte única de verdade**: um ficheiro descreve memória e interface.

## Como funciona

Um ficheiro `menu.yaml` passa através de um gerador durante a construção:

```text
menu.yaml → (compilação pio run) → ficheiros C++ em src/menu/ + NVS + JSON para portal
```

O portal desenha cada item do menu pelo seu tipo. `role:` dá ao item um rótulo traduzido do contrato do núcleo; um item sem `role:` aparece com o seu `title`.

!!! warning "Não edite ficheiros gerados"
    Ficheiros `menu_state.*`, `menu_bindings.*`, `menu_ids.h` e outros são criados pelo gerador. Edite apenas `menu.yaml` e recompile - caso contrário as suas alterações serão sobrescritas.

## Passo 1. Copie o modelo

Há um modelo de menu na biblioteca. Copie para o projecto:

```bash
mkdir -p src/menu
cp caminho/para/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## Passo 2. Conecte a geração durante a compilação

Copie um exemplo de hook do projecto `iDryer-Storage` (pode ser levado como está, não precisa configurar):

```bash
mkdir -p extra_scripts
cp caminho/para/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

Depois em `platformio.ini` adicione à secção `[env:cabinet]` a linha `-Isrc/menu` (para o código ver `#include <menu_state.h>`) e conecte o hook através de `extra_scripts`:

```ini
[env:cabinet]
; ... platform / board / lib_deps do capítulo 4 — sem mudanças ...

build_flags =
    -Isrc/menu                      ; ← adicionado: caminho para menu gerado
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← adicionado
    pre:extra_scripts/pre_gen_menu.py
```

O hook encontrará automaticamente o gerador no caminho `lib/idryer-core/menu/menu_gen.py`, portanto a biblioteca deve estar conectada através de `lib/` (symlink ou cópia), como descrito no capítulo 4.

## Passo 3. Descreva os parâmetros do armário

Abra `src/menu/menu.yaml`. O modelo já tem um item raiz `root` com matriz `children` e exemplos de parâmetros. Remova os exemplos (`my_param`, `my_flag`, `my_mode_group`) e adicione os seus dentro de `children`. Os últimos dois itens - `units_count` e `language` - deixe no lugar: este é um contrato fixa com o portal.

Para um armário básico basta alguns parâmetros.

Temperatura-alvo de armazenamento:

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # rótulo do contrato do núcleo
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # chave NVS (≤ 15 caracteres)
  persist: true
  scope: global
  default: 45
```

Histerese (quanto graus a temperatura pode descer abaixo do alvo antes de o aquecimento retomar):

```yaml
- id: hysteresis
  type: value
  title: { ru: "ГИСТЕРЕЗИС", en: "HYSTERESIS" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint8
  min: 1
  max: 5
  step: 1
  bind: hysteresis
  persist: true
  scope: global
  default: 2
```

!!! note "role: - é uma lista fechada"
    O valor `role:` não pode ser inventado arbitrariamente - deve ser da lista `canonical_roles` do contrato do núcleo. Se não há papel apropriado, a compilação parará e mostrará a lista permitida. Para armazém de armazenamento as funções adequadas são da família `storage.*`: `storage.target_temperature`, `storage.target_humidity`, `storage.start`, `storage.stop`. A lista completa está na cabeça `menu.template.yaml`. `role:` é opcional: um parâmetro sem ela (como a histerese acima) é guardado e publicado da mesma forma, só o rótulo vem de `title`.

Restrições que não podem ser violadas:

- `bind` - não mais de 15 caracteres (limite de chave NVS);
- não adicione o campo `widget:` ao `menu.yaml` — nem o portal nem a aplicação o leem: um item do menu é desenhado pelo seu tipo.

!!! warning "Verifique o item ignore_external_cmd do modelo"
    No modelo há um item `ignore_external_cmd`, e seu `bind` tem 19 caracteres, o que excede o limite de 15. Se deixar assim, a geração falhará: `bind 'ignore_external_cmd' ... tem 19 caracteres, limite 15`. Ou remova este item, ou encurte `bind` para `ign_ext_cmd` (como em produtos reais). Para um armário básico pode ser simplesmente removido.

## Passo 4. Compile o projecto e verifique geração

```bash
pio run -e cabinet
```

Durante a compilação o hook pré-compilado instala dependências (uma vez) e gera ficheiros C++ do menu. Se `menu.yaml` não mudou - a geração é pulada (`up-to-date`).

Verifique que a geração passou. No log de compilação aparece uma linha sobre geração de menu, e na pasta `src/menu/` - ficheiros gerados:

```text
src/menu/
├── menu.yaml          # seu ficheiro (fonte)
├── menu_state.h/.cpp  # objecto menu com todos os parâmetros
├── menu_bindings.*    # acesso por bind + escrita em NVS
├── menu_ids.h
└── menu_meta.h        # e outros
```

Se a compilação caiu com mensagem sobre `role:` desconhecida - significa que o papel não está da lista `canonical_roles`. Corrija-o e recompile. Ficheiros com marca autogen não edite à mão.

## Passo 5. Carregue o menu no arranque

Ligue o menu gerado em `src/main.cpp` e carregue-o em `setup()` — **antes** de `s_link.begin()`:

```cpp
#include <menu_state.h>      // objecto menu com todos os parâmetros
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // estabelecer valores padrão do YAML
menu.loadFromNVS();          // valores guardados; no primeiro arranque os valores por omissão são guardados
menu_sync_state_to_cache();  // valores para a cache a partir da qual o menu publicado é construído
```

Depois disto, os parâmetros ficam acessíveis através do objeto global `menu`:

```cpp
uint16_t target = menu.target_temp;   // acesso directo ao valor
```

## Passo 6. O menu no portal: publicar e aceitar alterações

O portal não lê o menu do dispositivo por si: o firmware publica-o e aplica as alterações que voltam. Três partes:

- **publicar** — `menu_buildFullJson()` do core constrói o JSON do menu a partir de `menu.yaml` e dos valores atuais; `devicePublisher()->publishConfigRaw()` envia-o para o portal (topic MQTT `config`) e para a aplicação pela rede local;
- **quando** — quando o dispositivo fica online e com o comando `get_config`: o portal envia-o quando abre o menu do dispositivo (a roda dentada no cartão);
- **alterar** — o portal envia `set` com o `id` do item e o novo valor `val`. `menu_apply_by_bind()` escreve o valor em `menu`, na NVS e na cache; depois o menu é publicado de novo e o portal mostra o valor confirmado.

Adicione depois dos includes:

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // publicar o menu a partir de loop()

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
        if (v < m.min_val) v = m.min_val;              // limites de menu.yaml
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + cache
        s_menuPending = true;                          // mostrar o novo valor no portal
        return;
    }
}
```

Em `setup()`, depois de `s_link.begin()`:

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

Em `loop()`, depois de `s_link.loop()`:

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // acabou de ficar online
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "Porque é que o menu é publicado a partir de loop()"
    Os callbacks dos comandos são chamados bem dentro do tratador de rede. Construir aí o JSON do menu gasta muita pilha, por isso o callback só levanta uma flag e `loop()` publica.

`applySet()` limita o valor ao `min`/`max` do item de `menu.yaml`: o dispositivo não confia cegamente num número que chega.

## Completo `src/main.cpp` após este capítulo

Em relação ao capítulo anterior, foram adicionadas as linhas marcadas com `// ← capítulo 6`: carregar o menu, publicá-lo e aceitar alterações.

??? note "O que era — `src/main.cpp` após o capítulo 5"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"

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

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
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
        s_link.telemetry.heaterTempC[0] = readHeaterTempC();
    }
    ```

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

## Verificação de resultado

Após o firmware:

- a roda dentada no cartão do dispositivo abre a página do dispositivo com o menu: a temperatura alvo (o portal rotula-a pela sua role — «Storage temperature») e **HYSTERESIS**;
- altere aí um valor — o dispositivo aceita-o, guarda-o na NVS e publica o menu de novo, e o portal mostra o valor confirmado;
- depois de reiniciar, o dispositivo publica os valores guardados;
- os parâmetros internos (histerese) estão acessíveis no código através de `menu`.

## O que vem a seguir

Configurações são descritas e armazenadas. Agora as ligamos ao hardware em [Controlo de aquecimento](07-heating-control.md): o aquecedor mantém a temperatura-alvo, a ventoinha liga-se pelo limiar.
