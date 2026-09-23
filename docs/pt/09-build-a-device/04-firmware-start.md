---
title: "Arranque de firmware em idryer-core: primeira execução e vinculação ao portal"
description: "Criar um projeto PlatformIO com a biblioteca idryer-core: platformio.ini, Config do dispositivo, primeira gravação do ESP32, configuração do Wi-Fi e associação do dispositivo ao portal iDryer na aplicação."
---

# Arranque de firmware no núcleo

Nesta página você cria um projecto de firmware, leva o ESP32 ao estado Online no portal e verifica que a parte de rede funciona. Sensores e lógica de aquecimento adicionamos nos passos seguintes.

A abordagem é construída na fachada `iDryer::Link`. Você descreve o dispositivo com uma estrutura `iDryer::Config`, chama `link.begin()` e `link.loop()` - o núcleo faz toda a ligação de rede.

## 1. Prepare as ferramentas

Você vai precisar de:

- VS Code com extensão PlatformIO;
- cabo USB;
- rede Wi-Fi `2,4 GHz` (ESP32 não funciona com redes só `5 GHz`);
- um smartphone com a aplicação iDryer ([App Store](https://apps.apple.com/app/idryer/id6760609044), [Google Play](https://play.google.com/store/apps/details?id=org.idryer.mobile)), com sessão iniciada na sua conta do portal iDryer: é através dela que o dispositivo recebe a rede Wi-Fi e fica associado à conta;
- a biblioteca do núcleo [idryer-core](https://github.com/pavluchenkor/idryer-core);
- o projecto pronto deste capítulo - [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet) no repositório do manual: é de lá que vêm o driver do sensor e outros ficheiros que mais à frente se propõe copiar.

O que é firmware do controlador e como entra na placa - [Firmware do controlador](../02-controllers/11-flashing-controller.md).

## 2. Crie um projecto

Em PlatformIO um projecto é uma pasta com estrutura fixa. Crie uma pasta de projecto (por exemplo `my-cabinet`) e abra-a em VS Code. Dentro devem estar estes ficheiros:

```text
my-cabinet/
├── platformio.ini        # configurações de construção (preenchidas no passo 4)
├── lib/
│   └── idryer-core/      # biblioteca do núcleo (symlink ou cópia)
└── src/
    └── main.cpp          # código do dispositivo: Config + setup() + loop()
```

Todos os fragmentos de código abaixo vão para estes ficheiros - cada passo especifica qual. Crie as pastas `include/`, `lib/` e `src/` manualmente se não existirem.

Coloque a biblioteca `idryer-core` em `lib/` - PlatformIO encontra bibliotecas lá automaticamente. A maneira mais fácil é fazer um symlink para a biblioteca transferida:

```bash
git clone https://github.com/pavluchenkor/idryer-core.git ~/idryer-core
ln -s ~/idryer-core lib/idryer-core
```

Em vez do symlink pode simplesmente copiar a pasta da biblioteca para `lib/idryer-core` - funciona da mesma maneira.

Isto também é necessário para gerar menu (capítulo 6) - o hook procura o gerador dentro de `lib/idryer-core/`.

## 3. O Wi-Fi e a associação não estão no código

O firmware não contém a palavra-passe da rede nem dados da conta. No primeiro arranque o dispositivo não tem Wi-Fi e espera pela configuração: a aplicação iDryer envia-a pelo ar (ESPTouch) e depois associa o dispositivo à sua conta com um token de associação de uso único. O core faz tudo isto dentro de `s_link.begin()` e `s_link.loop()`; a si só lhe resta seguir os passos na aplicação — secção 9.

**Como a rede chega ao dispositivo.** Uma placa sem rede guardada escuta o éter, como um receptor que ainda não foi sintonizado numa estação. O telemóvel, entretanto, «bate» o nome da rede e a palavra-passe no ar — mais ou menos como em código Morse, só que com pacotes Wi-Fi. A placa apanha essa transmissão, liga-se à rede e depois entra nela sozinha a cada arranque. Não são precisos pinos nem fios à parte para isto: funciona a antena própria da placa, arranca sozinho enquanto não houver rede e dura até 90 segundos.

Se pelo ar não resultar, há um caminho por cabo: o instalador web [install.idryer.org](https://install.idryer.org) entrega à placa a rede e o token de associação por USB — o mesmo que faz a aplicação, só que por cabo. Também ajuda um simples reinício da placa: depois dele volta a esperar pela configuração.

## 4. Configure platformio.ini

Preencha `platformio.ini` na raiz do projecto:

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; As bibliotecas do core (MQTT, ArduinoJson, WebSockets, Improv) chegam
; sozinhas a partir de lib/idryer-core/library.json.
; ESPAsyncTCP é o transporte ESP8266 das dependências do espMqttClient:
; não compila no ESP32 e tem de ser excluído.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Substitua `board` pela sua placa (por exemplo, `esp32-s3-devkitc-1`). Não precisa de especificar `idryer-core` em `lib_deps` - ela está em `lib/` (passo 2).

!!! note "O que fazem estas linhas"
    Não precisa de listar as dependências do core: o PlatformIO vai buscá-las a `lib/idryer-core/library.json`. `lib_ignore = ESPAsyncTCP` é obrigatório — sem ele a compilação falha em `ESPAsyncTCP.cpp`. As flags `MQTT_BROKER` e `MQTT_PORT` também são obrigatórias — sem elas o core não compila (`'MQTT_BROKER' was not declared`).

## 5. Descreva o dispositivo em Config

A seguir tudo acontece num ficheiro - `src/main.cpp`. Abra-o e escreva o código deste e dos passos seguintes.

`iDryer::Config` é o passaporte do dispositivo. As flags `has*` dizem ao portal o que o dispositivo tem e determinam quais campos de telemetria são publicados.

Para o armário aquecido no início de `src/main.cpp`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // dispositivo próprio: o cartão é construído pelo manifesto
    .unitsCount        = 1,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "Flags has* - isto é um contrato com o portal"
    Um campo de telemetria cuja flag correspondente é `false` não é publicado. Por exemplo, sem `hasAirHumidity = true` a humidade não vai para a nuvem, mesmo que a escreva no código. Incluir apenas o que fisicamente existe no dispositivo.

A lista de componentes e flags - [Composição do sistema](02-bom.md).

## 6. Programa principal mínimo

No mesmo ficheiro após o bloco `Config` adicione as funções `setup()` e `loop()`. Para primeira execução é suficiente iniciar a ligação e executá-la em `loop()`:

```cpp
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

`s_link.begin()` ativa o Wi-Fi, a associação e a ligação ao portal. O comando `revoke` chega do portal quando o dispositivo é desassociado da conta: `handleRevoke()` apaga o segredo do dispositivo e este fica à espera de uma nova associação. Os sensores entram no passo [Sensores](05-sensors.md).

### Completo `src/main.cpp` após este capítulo

Pegue nos dois blocos acima num ficheiro - este é todo o `src/main.cpp` neste passo:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // dispositivo próprio: o cartão é construído pelo manifesto
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
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

O capítulo anterior mostra o que **adicionar** e o **completo `src/main.cpp` após as mudanças**, para que sempre veja a imagem inteira, não fragmentos dispersos.

## 7. Grave o firmware

```bash
pio run -e cabinet -t upload
```

## 8. Abra o Serial Monitor

```bash
pio device monitor -b 115200
```

Enquanto o dispositivo não tem Wi-Fi, o log está em silêncio: o core reserva a porta série para o instalador web (Improv). Os logs ligam-se assim que o Wi-Fi fica ativo. Num dispositivo ainda não associado, o log termina assim:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

A última linha é o que se pretende neste passo: há rede, não há segredo de associação, o dispositivo espera o token da aplicação. Deixe o monitor aberto e passe para a aplicação.

## 9. Ligue o Wi-Fi e associe o dispositivo na aplicação

1. Ligue o telemóvel à rede Wi-Fi em que o dispositivo vai funcionar (`2.4 GHz`) e inicie sessão na aplicação iDryer com a sua conta do portal.
2. No ecrã inicial, toque em **Ligar novo dispositivo** — abre-se o passo **Wi-Fi**.
3. Confirme o nome da rede (a aplicação preenche-o sozinha se a localização estiver ativa), escreva a palavra-passe e toque em **Ligar dispositivo**. A aplicação envia a configuração durante até 90 segundos; quando o dispositivo entra na rede, aparece **Dispositivo ligado**. Toque em **Seguinte**.
4. No passo **Vinculação**, toque em **Emparelhar**. A aplicação encontra o dispositivo na rede, obtém do portal um token de associação de uso único, entrega-o ao dispositivo e espera que o portal confirme que o dispositivo está online.
5. Depois de **Dispositivo emparelhado**, o dispositivo aparece na lista de dispositivos do portal e da aplicação.

Se o dispositivo já estiver na rede, abra logo o passo **Vinculação** — toque no respetivo chip no topo da janela.

![Passo Wi-Fi na aplicação: nome da rede e palavra-passe](../../img/09-cabinet/04-app-wifi.png)
*Passo **Wi-Fi**: a aplicação entrega a rede ao dispositivo pelo ar.*

![Passo de vinculação: a aplicação encontrou o dispositivo na rede](../../img/09-cabinet/04-app-pairing.png)
*Passo **Vinculação**: a aplicação encontrou o dispositivo na rede pelo seu número de série. Os dispositivos alheios estão marcados como ocupados.*

![Mensagem «dispositivo emparelhado»](../../img/09-cabinet/04-app-paired.png)
*Pronto: o dispositivo está associado à conta e vai já aparecer na lista.*

O log mostra a associação:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## Verificação de resultado

Nesta fase o dispositivo deve estar Online no portal. Ainda não há dados de sensores — é o esperado: o `Config` ainda não declarou nada sobre eles e o cartão não tem o que mostrar.

![Cartão do dispositivo no portal logo após a vinculação](../../img/09-cabinet/04-portal-card.png)
*O dispositivo no portal: nome, estado Idle, ícone de ligação. Não há leituras — aparecem no capítulo seguinte.*

O nome `Device DEVICE_…` é de fábrica. Mude o nome do dispositivo com o lápis ao lado do nome: mais à frente nos exemplos chama-se «Storage cabinet».

Se algo correu mal:

- a aplicação não viu o dispositivo entrar na rede — verifique a palavra-passe e se a rede é de `2.4 GHz`; com a palavra-passe errada o dispositivo volta a esperar pela configuração, reinicie a placa e repita o passo Wi-Fi;
- a rede continua a não passar pelo ar — faça o mesmo por USB através do instalador web [install.idryer.org](https://install.idryer.org);
- no passo **Vinculação** a aplicação não encontrou o dispositivo — o telemóvel e o dispositivo têm de estar na mesma rede, e a rede não pode bloquear a descoberta de dispositivos (as redes de convidados costumam bloquear);
- o dispositivo reinicia — verifique a alimentação do ESP32 (quedas de tensão no arranque são uma causa frequente de reinícios);
- a compilação falha com erro — pergunte na comunidade: [Telegram](https://t.me/iDryer), [Discord](https://discord.gg/jGce5eeHHz);
- ver [Erros de alimentação](../08-common-mistakes/02-power-mistakes.md) e [Erros do controlador](../08-common-mistakes/04-controller-mistakes.md).

## O que vem a seguir

A parte de rede funciona. Vá para [Sensores](05-sensors.md): ligaremos SHT31 e termistor e veremos os dados no portal.
