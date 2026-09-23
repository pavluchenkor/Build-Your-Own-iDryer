---
title: "Composição do sistema do gabinete aquecido: componentes e duas versões da parte de potência"
description: "Lista de componentes para um gabinete aquecido caseiro em ESP32: sensor SHT31, termistor, aquecedor e ventilador em versões de baixa voltagem (24V) e rede (220V)."
---

# Composição do sistema

Nesta página está a lista de componentes do dispositivo e duas variantes da parte de potência. A parte de sinal fraco (controlador e sensores) é igual em ambas as versões. Apenas a forma como o aquecedor e ventilador são comutados é diferente.

## Parte de sinal fraco (igual para ambas as versões)

| Nó | Propósito | Observação |
|------|------------|------------|
| ESP32-C3 ou ESP32-S3 | Controlador: lógica, Wi-Fi, portal | DevKit ou Super Mini se adequam |
| Sensor SHT31 | Temperatura e umidade do ar no gabinete | Interface I2C |
| Termistor NTC 100K | Controle de temperatura do aquecedor | Por exemplo, Genérico 3950 |
| Resistor de pull-up do termistor | Divisor de tensão para ADC | Normalmente `4.7 kΩ` |
| Fonte de alimentação | Alimentação do controlador e periféricos de baixa voltagem | Voltagem para a versão escolhida |

C3 e S3 são equivalentes para este exemplo: o código, as conexões e a ordem de montagem são os mesmos. O S3 é mais potente e tem mais memória — pegue ele se mais tarde quiser adicionar uma tela, uma fita endereçável ou sua própria lógica pesada.

O ESP32 foi escolhido porque tem Wi-Fi, as interfaces necessárias (I2C para SHT31, ADC para termistor, PWM para controle de carga) e é suportado diretamente pelo `idryer-core`. Detalhes — [Controlador ESP32](../02-controllers/01-esp32-controller.md).

!!! warning "Lógica ESP32 — 3.3V"
    O ESP32 funciona em `3.3V`. Não aplique `5V` nos seus pinos. Isto se aplica a sensores, módulos e adaptadores. Detalhes — [Erros de controlador](../08-common-mistakes/04-controller-mistakes.md).

## Sensores

**SHT31** mede temperatura e umidade do ar dentro do gabinete. Esse é o principal feedback: por ele você vê se o clima definido é mantido. Conecta-se via I2C (duas linhas: `SDA`, `SCL`). Detalhes — [Termistores e sensores de clima](../03-common-components/04-thermistors.md).

**Termistor** mede a temperatura do próprio aquecedor, não do ar. É necessário para que o aquecedor não superaqueça: o ar aquece lentamente, mas o aquecedor rapidamente. O termistor é conectado como um divisor de tensão para um pino ADC. [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md).

!!! note "Por que dois sensores de calor"
    O SHT31 diz "qual é a temperatura no gabinete", o termistor - "o aquecedor não superaqueceu". O primeiro define o objetivo, o segundo protege contra falha.

## Parte de potência: escolha uma versão

O aquecedor e ventilador são a carga que o controlador gerencia. O ESP32 não pode comutar tal carga diretamente: seu pino fornece um sinal fraco em `3.3V`. Entre o controlador e a carga é necessária uma chave.

Existem duas versões fundamentalmente diferentes. Escolha uma dependendo de qual aquecedor e ventilador você usar.

### Versão A — baixa voltagem (24V ou 12V)

O aquecedor e ventilador são alimentados com `24V` (ou `12V`) de corrente contínua. Este é um caminho mais simples e seguro para montagem independente.

| Nó | Componente |
|------|-----------|
| Aquecedor | Elemento aquecedor `12V` ou `24V` (aquecedor PTC) |
| Ventilador | Ventilador `24V` ou `12V` (2-pino ou 4-pino) |
| Chave do aquecedor | Módulo MOSFET |
| Chave do ventilador | Módulo MOSFET (ou 4-pino PWM direto) |
| Fonte de alimentação | `24V DC` com margem de potência |

O controlador controla o módulo MOSFET com um sinal do pino ESP32. O módulo comuta a carga de baixa voltagem. Esta é a mesma lógica do controlador pronto. Detalhes — [Módulo MOSFET](../01-electronics-basics/02-mosfet-module.md).

A potência da fonte é calculada para a carga total com margem — veja [Cálculo de corrente de carga 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Versão recomendada para o primeiro dispositivo"
    Se você está montando um dispositivo pela primeira vez, comece com a versão A. Aqui não há tensão de rede na carga, e um erro de montagem é menos perigoso.

### Versão B — rede (110–230V AC)

O aquecedor e ventilador são alimentados pela rede `110–230V`. Isto é feito quando você precisa de um poderoso aquecedor de rede — por exemplo, um aquecedor pronto com ventilador para um gabinete. Aqui, em vez de um módulo MOSFET, módulos de comutação AC são usados.

| Nó | Componente |
|------|-----------|
| Aquecedor | Aquecedor de rede `110–230V AC` |
| Ventilador | Ventilador de rede `110–230V AC` |
| Chave do aquecedor | Relé de estado sólido (SSR) para AC |
| Chave do ventilador | SSR ou relé comum para AC |
| Fonte de alimentação | `24V`/`5V DC` separado para controlador e sensores |
| Proteção | Fusível, aterramento de proteção do gabinete |

!!! danger "Tensão de rede é perigosa para a vida"
    A versão B trabalha com tensão `110–230V`. Um erro de montagem pode resultar em choque elétrico ou incêndio. Antes de montar, leia obrigatoriamente o material de segurança: [Triac](../01-electronics-basics/03-triac.md), [Relé de estado sólido (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Erros de aquecedor e SSR](../08-common-mistakes/05-heater-ssr-mistakes.md). Se você não tem experiência com tensão de rede, escolha a versão A.

O controlador e sensores na versão B ainda são alimentados por uma fonte de baixa voltagem separada (`5V`/`24V`). A parte de rede e a parte de sinal fraco devem ser separadas física e eletricamente.

## Módulos opcionais

Esses nós não são obrigatórios para o gabinete, mas são suportados pelo núcleo e podem ser adicionados posteriormente:

- iluminação LED endereçável (`hasLed`);
- sensor de peso para consumo de filamento (`hasWeight`);
- etiqueta RFID de carretilha (`hasRfid`).

O gabinete básico não usa — começamos com o mínimo.

## O que vem a seguir

Quando os componentes são escolhidos, vá para [Esquema de conexão](03-wiring.md): qual pino ESP32 é responsável pelo quê e como isolar a parte de sinal fraco e de potência.
