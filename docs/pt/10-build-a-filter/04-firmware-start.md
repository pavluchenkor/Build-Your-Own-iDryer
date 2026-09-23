---
title: "Filtro inteligente: início da firmware e ligação ao portal"
description: "Estrutura da firmware do filtro em idryer-core: Config para tipo de dispositivo não-standard, primeira execução, vinculação à conta na aplicação."
---

# Início da firmware

A estrutura do projeto repete inteiramente [o capítulo do exemplo com o armário](../09-build-a-device/04-firmware-start.md): PlatformIO, `idryer-core` em `lib/`, mesmo `platformio.ini` (substitua apenas o nome do ambiente para `filter`). Aqui — apenas o que é diferente.

O projeto pronto deste capítulo — [example/10-filter](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/10-filter) no repositório do manual: é de lá que vêm o `platformio.ini` e todo o código analisado a seguir por partes. A biblioteca do núcleo — [idryer-core](https://github.com/pavluchenkor/idryer-core).

!!! note "Log na porta: duas flags de compilação"
    No ESP32-C3 a saída `Serial` vai por omissão para os pinos UART0 e não para a porta USB da placa — o Serial Monitor fica vazio. Para ver o log, são necessárias duas linhas em `build_flags`:

    ```ini
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    ```

    As mensagens do próprio ESP-IDF (erros de mDNS e semelhantes) chegam ao USB mesmo sem elas, por isso «alguma coisa é impressa, mas as minhas linhas não» é sinal precisamente destas flags em falta.

## Config: dispositivo de tipo não-standard

O filtro não tem nem aquecedor nem sensor de clima do dicionário do ecossistema. Das capacidades do dicionário tem apenas ventilador. Em `src/main.cpp`:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // dispositivo não-standard
    .unitsCount        = 1,
    // Periféricos: do dicionário do ecossistema temos apenas ventilador.
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
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

!!! note "DeviceType::Unknown — isto é normal"
    O tipo `Unknown` significa «o portal não conhece este produto». Anteriormente era um problema: o portal não tinha cartão para tipo desconhecido. Agora é o caminho padrão: a interface do dispositivo será completamente descrita pelo manifesto de cartão ([capítulo 6](06-card.md)), e o portal construirá o cartão a partir dele. O tipo é necessário apenas para produtos «próprios» do iDryer que têm cartões comerciais.

A flag `hasFan = true` nos dá gratuitamente: campo `fanStatus` na telemetria, célula «Ventilador» no cartão e entidade no manifesto — tudo do dicionário do ecossistema.

## Sensor VOC não existe em Config — e não deveria existir

Note: em `Config` não há flag «hasVoc». O dicionário `has*` descreve periféricos conhecidos pelo ecossistema. O seu sensor próprio adiciona-o não através do dicionário, mas por dois outros mecanismos: acrescenta os seus dados à telemetria no seu próprio campo e declara-o no manifesto de cartão — são os próximos dois capítulos. Nisto reside a essência da abordagem: o dicionário não precisa de ser expandido para cada novo dispositivo.

## Primeira execução e ligação

O procedimento é o mesmo que para o armário:

1. Grave a placa e abra o Serial Monitor: enquanto o dispositivo não tem Wi-Fi, o log está em silêncio.
2. Na aplicação iDryer: **Ligar novo dispositivo** → passo **Wi-Fi** (rede e palavra-passe, **Ligar dispositivo**) → passo **Vinculação** → **Emparelhar**.
3. Depois de **Dispositivo emparelhado**, o dispositivo fica associado à sua conta e passa a `Online` no portal; o log mostra `MQTT: Connected!`.

Detalhes, erros possíveis e nova associação — no [capítulo do exemplo do armário](../09-build-a-device/04-firmware-start.md).

![Página do dispositivo no portal logo após a vinculação](../../img/10-filter/04-portal-device.png)
*Dispositivo no portal: nome, estado Idle, ícone de ligação. O gráfico está vazio e o menu não chegou — o dispositivo não declarou nada sobre eles.*

No portal o dispositivo é já visível, mas o cartão está quase vazio — ainda não há dados. Vamos ligar o sensor.
