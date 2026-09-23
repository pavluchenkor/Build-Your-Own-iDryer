---
title: "Filtro inteligente: início do firmware e vinculação ao portal"
description: "Estrutura base do firmware do filtro em idryer-core: configuração de tipo de dispositivo não-padrão, primeiro lançamento, vinculação à conta no app."
---

# Início do firmware

A estrutura do projeto repete completamente o [capítulo do exemplo do gabinete](../09-build-a-device/04-firmware-start.md): PlatformIO, `idryer-core` em `lib/`, o mesmo `platformio.ini` (apenas mude o nome do ambiente para `filter`). Aqui — apenas o que é diferente.

O projeto pronto deste capítulo — [example/10-filter](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/10-filter) no repositório do tutorial: é de lá que vêm o `platformio.ini` e todo o código que adiante é analisado por partes. A biblioteca do núcleo — [idryer-core](https://github.com/pavluchenkor/idryer-core).

!!! note "Log na porta: duas flags de build"
    No ESP32-C3, a saída `Serial` por padrão vai para os pinos do UART0, e não para a porta USB da placa — o monitor de porta fica vazio. Para ver o log, o `build_flags` precisa de duas linhas:

    ```ini
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    ```

    As mensagens do próprio ESP-IDF (erros de mDNS e semelhantes) vão para o USB mesmo sem elas, por isso «algo é impresso, mas as minhas linhas não» é sinal justamente dessas flags faltando.

## Config: dispositivo de tipo não-padrão

O filtro não tem nem aquecedor, nem sensor de clima do dicionário do ecossistema. Do vocabulário do ecossistema, ele só tem ventilador. Em `src/main.cpp`:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // dispositivo não-padrão
    .unitsCount        = 1,
    // Periféricos: do vocabulário do ecossistema, temos apenas ventilador.
    .hasFan            = true,
    // Períodos de publicação automática:
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identificação no portal:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
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

!!! note "DeviceType::Unknown — é normal"
    O tipo `Unknown` significa "o portal não conhece este produto". Antes era um problema: o portal não tinha um cartão para um tipo desconhecido. Agora é o caminho padrão: a interface do dispositivo será totalmente descrita pelo manifesto do cartão ([capítulo 6](06-card.md)), e o portal construirá o cartão a partir dele. O tipo é necessário apenas para os próprios produtos iDryer, que têm cartões de marca.

A flag `hasFan = true` nos dá gratuitamente: campo `fanStatus` na telemetria, célula "Ventilador" no cartão e entidade no manifesto — tudo do vocabulário do ecossistema.

## VOC-sensor não está na Config — e não deveria estar

Observe: não há flag "hasVoc" na `Config`. O dicionário `has*` descreve periféricos conhecidos do ecossistema. Seu sensor customizado é adicionado não através do dicionário, mas através de dois outros mecanismos: você adiciona suas leituras à telemetria em seu próprio campo e o declara no manifesto do cartão — estes são os dois próximos capítulos. Esta é a essência da abordagem: o dicionário não precisa ser expandido para cada novo dispositivo.

## Primeiro lançamento e vinculação

O procedimento é o mesmo do gabinete:

1. Grave a placa e abra o Serial Monitor: enquanto o dispositivo não tem Wi-Fi, o log fica em silêncio.
2. No app iDryer: **Conectar novo dispositivo** → passo **Wi-Fi** (rede e senha, **Conectar dispositivo**) → passo **Vinculação** → **Vincular**.
3. Depois de **Dispositivo vinculado**, o dispositivo está vinculado à sua conta e fica `Online` no portal; o log mostra `MQTT: Connected!`.

Detalhes, erros possíveis e nova vinculação — no [capítulo do exemplo do gabinete](../09-build-a-device/04-firmware-start.md).

![Página do dispositivo no portal logo após a vinculação](../../img/10-filter/04-portal-device.png)
*Dispositivo no portal: nome, estado Idle, ícone de conexão. O gráfico está vazio e o menu não chegou — o dispositivo não declarou nada sobre eles.*

O dispositivo já é visível no portal, mas o cartão está quase vazio — ainda não há dados. Vamos conectar o sensor.
