---
title: "Filtro inteligente: sensor VOC e telemetria"
description: "Leitura de SGP40 por I2C e publicação do campo vocIndex próprio em telemetria iDryer através de callback onTelemetryPublish."
---

# Sensor e telemetria

Neste capítulo o filtro começa a medir o ar e enviar dados para a nuvem. A técnica chave é **campo próprio em telemetria**: o dicionário do ecossistema não conhece VOC, mas o núcleo permite adicionar qualquer campo à telemetria.

## 1. Biblioteca do sensor

Em `platformio.ini` adicione a `lib_deps`:

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. Leitura de SGP40

Em `src/main.cpp` (pinos — do [esquema](03-wiring.md)):

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = sem dados ainda

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() faz compensação interna do sensor.
    // Índice: ~100 = ar normal, superior = mais sujo (máx 500).
    g_vocIndex = s_sgp.measureVocIndex();
}
```

Adicione a chamada de `initVocSensor()` em `setup()` após `s_link.begin()`, e `readVocSensor()` em `loop()` uma vez por segundo (com temporizador millis, não com `delay`!):

```cpp
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();
    }
}
```

!!! warning "Nenhum delay() no loop"
    `s_link.loop()` deve ser chamado continuamente — nele estão Wi-Fi, MQTT e comandos do portal. `delay(1000)` congela tudo. Apenas temporizadores millis.

## 3. Campo próprio em telemetria

A cada `telemetryPeriodMs` o núcleo recolhe automaticamente uma mensagem JSON de telemetria e envia-a para a nuvem. Para nosso dispositivo (um único unit, do dicionário apenas ventilador) o núcleo recolhe a seguinte mensagem:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

Vamos descrever a estrutura:

- `units` — array de **units** (câmaras) do dispositivo. A secadora série iDryer pode ter até quatro câmaras independentes, portanto a telemetria é sempre um array, mesmo com uma câmara;
- `units[0]` — o primeiro (e único) unit: definimos `unitsCount = 1` em `Config`;
- `fanStatus` — campo de dicionário, apareceu porque `hasFan = true`;
- `rssi`, `uptime` — nível de Wi-Fi e tempo de atividade, o núcleo adiciona sempre.

Não há nada sobre VOC nesta mensagem — o núcleo não conhece o nosso sensor. Mas logo antes do envio o núcleo dá ao seu código a oportunidade de adicionar os seus próprios campos à mensagem. Para isso regista um **callback** (chamada de retorno) — uma função que entrega ao núcleo, e o núcleo chama-a por si mesmo em cada publicação, passando a mensagem recolhida (argumento `doc` — é esta).

Em `setup()`:

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc — é a mensagem de telemetria recolhida pelo núcleo (ver JSON acima).
    // Adiciona ao primeiro unit nosso campo vocIndex.
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

A linha `doc["units"][0]["vocIndex"] = g_vocIndex;` lê-se assim: «na mensagem `doc` pegue no array `units`, nele elemento `0` (o nosso único unit) e escreva o campo `vocIndex`». O nome do campo é à sua escolha — no [próximo capítulo](06-card.md) irá referir-se a ele para mostrar o valor no cartão.

!!! note "Se encontrar a palavra hook"
    No código-fonte do núcleo este callback chama-se `PublishHook` — «hook» significa o mesmo: ponto onde a biblioteca permite «pendurar» sua função. Os termos são intercambiáveis; nesta documentação dizemos «callback».

!!! note "Lambda e porque está «vazia»"
    A construção `[](JsonObject doc) { ... }` chama-se **lambda** — é uma função sem nome, escrita directamente no local de uso, para não a extrair e não inventar um nome.

    Os parênteses rectos no início — «lista de captura»: neles enumera-se variáveis locais que a função leva consigo. Regra do núcleo: **parênteses sempre vazios** (`[]`) — a lambda nada captura e não carrega nenhum estado (chama-se *stateless*, «sem estado»).

    Razão técnica: lambdas com captura requerem memória dinâmica, e em ESP32 alocações frequentes fragmentam o heap e, no pior caso, prejudicam o Wi-Fi. Portanto o núcleo aceita apenas funções simples.

    Conclusão prática: tudo que o callback precisa, guarde em variáveis **globais** — como nossa `g_vocIndex`. Esta regra vale para todos os callbacks de `idryer-core`.

O estado do ventilador publica-se pelo caminho de dicionário — basta escrever no campo do núcleo quando liga/desliga (lógica — [capítulo 7](07-auto-logic.md)):

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. Sem sensor à mão? Modo demo

O caminho até ao cartão também se percorre sem o SGP40: o índice é calculado por um modelo do ar. Copie o ficheiro `demo_voc.h` do exemplo:

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

e acrescente ao `platformio.ini` a flag de compilação:

```ini
build_flags =
    -DDEMO_VOC=1
```

Ambos os ramos estão escondidos atrás de uma única função, e o `loop()` não sabe de onde veio o valor:

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

O modelo comporta-se como uma sala onde uma impressora está a imprimir: enquanto o ventilador está parado, o índice sobe a partir do valor de fundo próximo de `100`, e com o ventilador a funcionar desce. O modelo retira o estado do ventilador da sua própria lógica, por isso o limiar e a histerese do [capítulo 7](07-auto-logic.md) veem-se em funcionamento.

Para um dispositivo em serviço a flag não se define: nesse caso compila-se o ramo com o sensor real.

## 5. Verificação

Após carregar no stream MQTT do dispositivo (ou no log Serial de publicações) a telemetria fica assim:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` — seu campo próprio, viajando na nuvem junto com `fanStatus` de dicionário. O portal já o recebe, mas ainda não sabe o que fazer: mostre-lhe isto no próximo capítulo.

!!! note "O campo próprio vive em tempo real"
    O portal distribui a telemetria aos clientes tal como está, por isso o cartão mostra `vocIndex` de imediato. Já no histórico para os gráficos o servidor escreve apenas grandezas do dicionário — temperatura, humidade, aquecimento. O seu campo não aparecerá no gráfico de telemetria; se precisar de histórico, recolha-o do seu lado (por exemplo, Home Assistant ou uma base de dados própria).

Respire sobre o sensor ou aproxime um marcador — o índice deve crescer significativamente em segundos.
