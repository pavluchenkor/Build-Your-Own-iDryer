---
title: "Composição do sistema de armário aquecido: componentes e duas versões da secção de potência"
description: "Lista de componentes para armário aquecido caseiro em ESP32: sensor SHT31, termistor, aquecedor e ventoinha em versões de baixa tensão (24V) e AC (220V)."
---

# Composição do sistema

Nesta página está a lista de componentes do dispositivo e duas variantes da secção de potência. A secção de baixa tensão (controlador e sensores) é a mesma em ambas as versões. Apenas o modo de comutação do aquecedor e ventoinha é diferente.

## Secção de baixa tensão (comum a ambas as versões)

| Componente | Função | Notas |
|------------|--------|-------|
| ESP32-C3 ou ESP32-S3 | Controlador: lógica, Wi-Fi, portal | Funciona DevKit ou Super Mini |
| Sensor SHT31 | Temperatura e humidade do ar no armário | Interface I2C |
| Termistor NTC 100K | Controlo de temperatura do aquecedor | Por exemplo, Generic 3950 |
| Resistor de pull-up do termistor | Divisor de tensão para ADC | Normalmente `4,7 kΩ` |
| Fonte de alimentação | Alimentação do controlador e periféricos de baixa tensão | Tensão para a versão escolhida |

C3 e S3 são equivalentes para este exemplo: o código, a ligação e a ordem de montagem são os mesmos. O S3 é mais potente e tem mais memória - escolha-o se mais tarde quiser acrescentar um ecrã, uma fita LED endereçável ou lógica pesada própria.

ESP32 foi escolhido porque tem Wi-Fi, as interfaces necessárias (I2C para SHT31, ADC para termistor, PWM para controlo de carga) e é suportado directamente por `idryer-core`. Para mais detalhes - [Controlador ESP32](../02-controllers/01-esp32-controller.md).

!!! warning "Lógica ESP32 - 3,3V"
    ESP32 funciona em `3,3V`. Não coloque `5V` nos seus pinos. Isto aplica-se a sensores, módulos e adaptadores. Para mais detalhes - [Erros de controladores](../08-common-mistakes/04-controller-mistakes.md).

## Sensores

**SHT31** mede temperatura e humidade do ar dentro do armário. É a retroacção principal: por ela você vê se o clima solicitado é mantido. Ligado via I2C (duas linhas: `SDA`, `SCL`). Para mais detalhes - [Termistores e sensores climáticos](../03-common-components/04-thermistors.md).

**Termistor** mede a temperatura do próprio aquecedor, não do ar. É necessário para evitar que o aquecedor sobrequeça: o ar aquece lentamente, mas o aquecedor aquece rapidamente. O termistor está ligado como um divisor de tensão na entrada ADC. [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md).

!!! note "Por que dois sensores de calor"
    SHT31 diz "qual é a temperatura no armário", termistor - "o aquecedor não sobreaqueceu?". O primeiro estabelece o objetivo, o segundo protege contra falhas.

## Secção de potência: escolha uma versão

O aquecedor e ventoinha são a carga que o controlador controla. ESP32 não pode comutar tal carga directamente: o seu pino fornece um sinal fraco `3,3V`. Há uma chave entre o controlador e a carga.

Existem duas versões fundamentalmente diferentes. Escolha uma dependendo de qual aquecedor e ventoinha está a usar.

### Versão A - Baixa tensão (24V ou 12V)

O aquecedor e ventoinha são alimentados a partir de `24V` (ou `12V`) corrente contínua. Este é o caminho mais simples e seguro para montagem independente.

| Componente | Tipo |
|------------|------|
| Aquecedor | Elemento de aquecimento `12V` ou `24V` (aquecedor PTC) |
| Ventoinha | Ventoinha `24V` ou `12V` (2-pinos ou 4-pinos) |
| Chave do aquecedor | Módulo MOSFET |
| Chave da ventoinha | Módulo MOSFET (ou PWM 4-pinos directamente) |
| Fonte de alimentação | `24V DC` com margem de potência |

O controlador controla o módulo MOSFET com um sinal do pino de ESP32. O módulo comuta a carga de baixa tensão. É a mesma lógica que no controlador completo. Para mais detalhes - [Módulo MOSFET](../01-electronics-basics/02-mosfet-module.md).

A potência da fonte de alimentação é calculada para a carga total com margem - veja [Cálculo de corrente de carga 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Versão recomendada para o primeiro dispositivo"
    Se está a montar o dispositivo pela primeira vez, comece com a versão A. Não há voltagem de rede na carga e um erro de montagem é menos perigoso.

### Versão B - AC (110-230V AC)

O aquecedor e ventoinha são alimentados a partir da rede `110-230V`. Isto é feito quando é necessário um aquecedor de rede poderoso - por exemplo, um aquecedor pronto com ventoinha para o armário. Aqui, em vez de módulo MOSFET, são usados módulos de comutação AC.

| Componente | Tipo |
|------------|------|
| Aquecedor | Aquecedor de rede `110-230V AC` |
| Ventoinha | Ventoinha de rede `110-230V AC` |
| Chave do aquecedor | Relé de estado sólido (SSR) para AC |
| Chave da ventoinha | SSR ou relé comum para AC |
| Fonte de alimentação | Separado `24V`/`5V DC` para controlador e sensores |
| Protecção | Fusível, aterramento de protecção da carcaça |

!!! danger "Voltagem de rede é perigosa"
    Versão B funciona com voltagem `110-230V`. Um erro de montagem pode causar choque ou incêndio. Antes de montar, leia os materiais de segurança: [Triac](../01-electronics-basics/03-triac.md), [Relé de estado sólido (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Erros de aquecedores e SSR](../08-common-mistakes/05-heater-ssr-mistakes.md). Se não tem experiência com voltagem de rede, escolha a versão A.

O controlador e sensores na versão B ainda são alimentados por uma fonte de baixa tensão separada (`5V`/`24V`). As partes AC e de baixa tensão devem ser fisicamente e electricamente separadas.

## Módulos opcionais

Estes componentes não são obrigatórios para o armário, mas são suportados pelo núcleo e podem ser adicionados mais tarde:

- iluminação LED endereçável (`hasLed`);
- sensor de peso de filamento (`hasWeight`);
- marcação RFID do carretel (`hasRfid`).

O armário básico não os usa - começamos com o mínimo.

## O que vem a seguir

Quando os componentes são escolhidos, vá para [Esquema de ligação](03-wiring.md): qual pino de ESP32 para quê e como separar as partes de baixa e alta potência.
