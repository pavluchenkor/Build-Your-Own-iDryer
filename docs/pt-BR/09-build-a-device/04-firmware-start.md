---
title: "Inicialização de firmware em idryer-core: primeiro lançamento e vinculação ao portal"
description: "Criar um projeto PlatformIO com a biblioteca idryer-core: platformio.ini, Config do dispositivo, primeira gravação do ESP32, configuração do Wi-Fi e vinculação do dispositivo ao portal iDryer no app."
---

# Início da firmware no núcleo

Nesta página, você cria um projeto de firmware, coloca o ESP32 no estado Online no portal e verifica que a parte de rede funciona. Sensores e lógica de aquecimento serão adicionados nos próximos passos.

A abordagem é construída na fachada `iDryer::Link`. Você descreve o dispositivo com uma estrutura `iDryer::Config`, chama `link.begin()` e `link.loop()` — o núcleo cuida de toda a conexão de rede sozinho.

## 1. Prepare as ferramentas

Você vai precisar:

- VS Code com extensão PlatformIO;
- Cabo USB;
- Rede Wi-Fi `2.4 GHz` (ESP32 não funciona com redes apenas `5 GHz`).
- um smartphone com o app iDryer, logado na sua conta do portal iDryer: é por ele que o dispositivo recebe a rede Wi-Fi e é vinculado à conta.

O que é firmware do controlador e como ele entra na placa — [Firmware do controlador](../02-controllers/11-flashing-controller.md).

## 2. Crie um projeto

No PlatformIO, um projeto é uma pasta com estrutura fixa. Crie uma pasta de projeto (por exemplo `my-cabinet`) e abra-a em VS Code. Dentro devem haver esses arquivos:

```text
my-cabinet/
├── platformio.ini        # configurações de build (preencheremos na etapa 4)
├── lib/
│   └── idryer-core/      # biblioteca de núcleo (symlink ou cópia)
└── src/
    └── main.cpp          # código do dispositivo: Config + setup() + loop()
```

Todos os fragmentos de código abaixo vão para esses arquivos — cada etapa indica onde. Crie as pastas `include/`, `lib/` e `src/` manualmente, se não existirem.

Coloque a biblioteca `idryer-core` em `lib/` — PlatformIO encontra bibliotecas lá automaticamente. A forma mais fácil é fazer um symlink para a biblioteca baixada:

```bash
ln -s /caminho/para/idryer-core lib/idryer-core
```

Isso também é necessário para a geração de menu (capítulo 6) — o hook procura o gerador dentro de `lib/idryer-core/`.

## 3. O Wi-Fi e a vinculação não ficam no código

O firmware não tem a senha da rede nem dados da conta. Na primeira inicialização o dispositivo não tem Wi-Fi e espera a configuração: o app iDryer a envia pelo ar (ESPTouch) e depois vincula o dispositivo à sua conta com um token de vinculação de uso único. O core faz tudo isso dentro de `s_link.begin()` e `s_link.loop()`; você só segue os passos no app — seção 9.

## 4. Configure platformio.ini

Preencha `platformio.ini` na raiz do projeto:

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; As bibliotecas do core (MQTT, ArduinoJson, WebSockets, Improv) vêm
; sozinhas de lib/idryer-core/library.json.
; ESPAsyncTCP é o transporte do ESP8266 que vem das dependências do espMqttClient:
; ele não compila no ESP32 e precisa ser excluído.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Substitua `board` pela sua placa (por exemplo, `esp32-s3-devkitc-1`). Você não precisa especificar `idryer-core` em `lib_deps` — ela está em `lib/` (etapa 2).

!!! note "O que estas linhas fazem"
    Você não lista as dependências do core: o PlatformIO pega de `lib/idryer-core/library.json`. `lib_ignore = ESPAsyncTCP` é obrigatório — sem ele o build falha em `ESPAsyncTCP.cpp`. As flags `MQTT_BROKER` e `MQTT_PORT` também são obrigatórias — sem elas o core não compila (`'MQTT_BROKER' was not declared`).

## 5. Descreva o dispositivo em Config

A seguir tudo acontece em um arquivo — `src/main.cpp`. Abra-o e escreva o código desta e das próximas etapas.

`iDryer::Config` é o passaporte do dispositivo. Os sinalizadores `has*` dizem ao portal o que o dispositivo tem e determinam quais campos de telemetria são publicados.

Para o gabinete aquecido no topo de `src/main.cpp`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    // Periféricos:
    .hasHeater         = true,    // aquecedor controlado
    .hasFan            = true,    // ventilador
    .hasAirTemp        = true,    // temperatura do ar (SHT31)
    .hasAirHumidity    = true,    // umidade do ar (SHT31)
    .hasHeaterTemp     = true,    // temperatura do aquecedor (termistor)
    // Períodos de autopublicação:
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identificação no portal:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "Sinalizadores has* — é um contrato com o portal"
    Um campo de telemetria cujo sinalizador correspondente é `false` não é publicado. Por exemplo, sem `hasAirHumidity = true`, a umidade não vai para a nuvem, mesmo que você escreva no código. Inclua apenas o que fisicamente existe no dispositivo.

Lista de componentes e sinalizadores — [Composição do sistema](02-bom.md).

## 6. Função main mínima

No mesmo arquivo após o bloco `Config`, adicione as funções `setup()` e `loop()`. Para o primeiro lançamento é suficiente iniciar o link e rodá-lo em `loop()`:

```cpp
void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Dispositivo desvinculado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

`s_link.begin()` sobe o Wi-Fi, a vinculação e a conexão com o portal. O comando `revoke` chega do portal quando o dispositivo é desvinculado da conta: `handleRevoke()` apaga o segredo do dispositivo, e ele fica esperando uma nova vinculação. Os sensores entram no passo [Sensores](05-sensors.md).

### Completo `src/main.cpp` após este capítulo

Pegue ambos os blocos acima em um arquivo — este é todo o `src/main.cpp` nesta etapa:

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
    // Dispositivo desvinculado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

O capítulo anterior mostra **o que adicionar** e **o `src/main.cpp` completo após as mudanças**, para que você sempre veja o quadro geral, não fragmentos dispersos.

## 7. Flash

```bash
pio run -e cabinet -t upload
```

## 8. Abra o Serial Monitor

```bash
pio device monitor -b 115200
```

Enquanto o dispositivo não tem Wi-Fi, o log fica em silêncio: o core reserva a porta serial para o instalador web (Improv). Os logs ligam assim que o Wi-Fi sobe. Em um dispositivo ainda não vinculado, o log termina assim:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Deixe o monitor aberto e vá para o app.

## 9. Conecte o Wi-Fi e vincule o dispositivo no app

1. Conecte o celular à rede Wi-Fi em que o dispositivo vai funcionar (`2.4 GHz`) e entre no app iDryer com a sua conta do portal.
2. Na tela inicial, toque em **Conectar novo dispositivo** — abre o passo **Wi-Fi**.
3. Confira o nome da rede (o app preenche sozinho se a localização estiver ligada), digite a senha e toque em **Conectar dispositivo**. O app envia a configuração por até 90 segundos; quando o dispositivo entra na rede, aparece **Dispositivo conectado**. Toque em **Avançar**.
4. No passo **Vinculação**, toque em **Vincular**. O app encontra o dispositivo na rede, pega no portal um token de vinculação de uso único, entrega ao dispositivo e espera o portal confirmar que o dispositivo está online.
5. Depois de **Dispositivo vinculado**, ele aparece na lista de dispositivos do portal e do app.

Se o dispositivo já estiver na rede, abra direto o passo **Vinculação** — toque no chip dele no topo da janela.

O log mostra a vinculação:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## Verificação de resultado

Nesta etapa o dispositivo deve estar Online no portal. Ainda não há dados de sensores — isso é esperado. Se algo deu errado:

- o app não viu o dispositivo entrar na rede — confira a senha e se a rede é `2.4 GHz`; com a senha errada o dispositivo volta a esperar a configuração, repita o passo Wi-Fi;
- no passo **Vinculação** o app não encontrou o dispositivo — o celular e o dispositivo precisam estar na mesma rede, e a rede não pode bloquear a descoberta de dispositivos (redes de convidados costumam bloquear);
- o dispositivo reinicia — confira a alimentação do ESP32 (quedas de tensão na partida são uma causa comum de reinícios);
- veja [Erros de alimentação](../08-common-mistakes/02-power-mistakes.md) e [Erros do controlador](../08-common-mistakes/04-controller-mistakes.md).

## O que vem a seguir

A parte de rede funciona. Vá para [Sensores](05-sensors.md): vamos conectar SHT31 e termistor e ver seus dados no portal.
