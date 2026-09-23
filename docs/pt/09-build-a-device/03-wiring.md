---
title: "Esquema de ligação de armário aquecido em ESP32"
description: "Mapa de pinos de ESP32 para armário caseiro: SHT31 por I2C, termistor em ADC, aquecedor e ventoinha via chave. Isolamento entre partes de baixa e alta potência."
---

# Esquema de ligação

Nesta página está como conectar componentes ao redor de ESP32. Primeiro o mapa geral de pinos, depois a ligação de cada componente e as regras para o encaminhamento da parte de potência.

!!! warning "Primeiro, verifique a pinagem da sua placa"
    Os números de pinos abaixo são um exemplo. Diferentes placas ESP32-C3 e ESP32-S3 têm numeração e disposição de pinos diferentes. Antes da montagem, verifique a pinagem da sua placa de desenvolvimento específica: nas DevKit e Super Mini ela é publicada pelo fabricante e consta na descrição da placa. Nem todos os pinos podem ser usados livremente: alguns estão ocupados por carregamento, flash ou USB.

## Mapa de pinos (exemplo)

| Componente | Linha | Pino de ESP32 (exemplo) |
|------------|-------|------------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Termistor | sinal ADC | GPIO2 |
| Aquecedor (chave) | controlo | GPIO4 |
| Ventoinha (chave/PWM) | controlo | GPIO5 |

Alimentação dos sensores - `3,3V` e `GND` da placa. A parte de potência é alimentada separadamente.

## SHT31 por I2C

SHT31 está ligado com quatro fios:

1. `VCC` do sensor - em `3,3V` da placa.
2. `GND` do sensor - em `GND` da placa.
3. `SDA` do sensor - no pino `SDA` (exemplo: GPIO8).
4. `SCL` do sensor - no pino `SCL` (exemplo: GPIO9).

As linhas I2C são curtas. Se o sensor está longe da placa, mantenha os fios o mais curto possível e torcidos. A maioria dos módulos SHT31 têm resistores de pull-up já na placa do módulo.

!!! note "Endereço SHT31"
    SHT31 normalmente tem endereço `0x44` (às vezes `0x45`). Se o sensor não responde, verifique o endereço e as linhas `SDA`/`SCL`.

## Termistor em ADC

O termistor é incluído num divisor de tensão com o resistor de pull-up:

1. Um pino do termistor - em `3,3V`.
2. O segundo pino do termistor - no ponto de ligação com resistor `4,7 kΩ` e no pino ADC (exemplo: GPIO2).
3. O segundo pino do resistor `4,7 kΩ` - em `GND`.

O controlador mede a tensão no ponto médio do divisor e calcula a resistência do termistor e depois a temperatura. O tipo de termistor é estabelecido no firmware (veja [Controlo de aquecimento](07-heating-control.md)).

Para detalhes sobre verificação e montagem - [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md).

## Aquecedor e ventoinha via chave

ESP32 controla a carga não directamente, mas via chave. Qual chave - depende da versão de [Composição do sistema](02-bom.md).

### Versão A (24V/12V) - Módulo MOSFET

1. Entrada de sinal do módulo (`PWM`/`SIG`) - no pino de controlo de ESP32 (exemplo: GPIO4 para aquecedor, GPIO5 para ventoinha).
2. `GND` do módulo - em `GND` comum com ESP32.
3. Entrada de alimentação do módulo e carga - em fonte de alimentação `24V`.

!!! warning "Terra comum"
    `GND` do controlador e `GND` da fonte de potência devem estar ligados. Sem terra comum, o sinal de controlo não tem nível de referência e a chave funciona de forma imprevisível.

A ligação da ventoinha com controlo é discutida detalhadamente em [Ligação de ventoinha](../06-practical-guides/01-connecting-fan.md). A lógica da chave - [Módulo MOSFET](../01-electronics-basics/02-mosfet-module.md).

### Versão B (220V) - SSR/relé

!!! danger "Antes de montar a parte de rede"
    Todas as ligações com a rede devem ser feitas com o dispositivo completamente desligado. A carcaça com a parte de rede deve ter aterramento de protecção e fusível. Use fios de rede de secção adequada e prenda bem nas clemas.

Um SSR tem dois lados. **Controlo** - entrada de baixa tensão, que o controlador comanda. **Potência** - terminais pelos quais passa a voltagem de rede da carga. Os lados são isolados um do outro por opto-acoplador dentro do SSR, portanto pode controlar a rede com um sinal fraco de `3,3V`.

1. A entrada de controlo é normalmente marcada como `DC+` e `DC-` (às vezes `+` e `-`) e está especificada para `3-32V` DC. Ligue `DC+` ao pino de controlo de ESP32 (exemplo: GPIO4) e `DC-` a `GND` do controlador. A tensão de `3,3V` do pino de ESP32 é suficiente para abrir o SSR.
2. Os terminais de potência (frequentemente marcados como rede/`AC` e carga/`LOAD`) estão incluídos na abertura de um dos fios de rede do aquecedor - como um interruptor no fio.
3. A ventoinha é comutada por um SSR ou relé separado da mesma forma.

!!! note "Por que SSR precisa de dissipador"
    Na comutação, o SSR aquece um pouco, e quanto maior a corrente de carga, mais quente. Portanto, o SSR é aparafusado a um dissipador (placa de metal para dissipação de calor), e o SSR é levado com uma margem de corrente - muito acima da corrente de carga. Qual a margem e o dissipador necessários para a sua corrente - [Relé de estado sólido (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Encaminhamento: baixa tensão e potência

- Mantenha os fios de sinal (sensores, controlo) separados da potência.
- Não coloque fios de termistor e I2C ao longo dos fios de potência do aquecedor - é uma fonte de interferência.
- Na versão B, separe fisicamente as zonas de rede e de baixa tensão dentro da carcaça.
- Ligue todas as terras da secção de baixa tensão num ponto.

Interferência da ventoinha e terra deficiente - uma causa frequente de "leitura flutuante" e reinicializações. Veja [Erros de fiação](../08-common-mistakes/03-wiring-mistakes.md).

## O que verificar antes de fornecer alimentação

- Alimentação de sensores `3,3V`, não `5V`.
- Termistor e resistor divisor montados correctamente, pino ADC no ponto médio.
- Terra comum do controlador e fonte de potência.
- Na versão B - aterramento de carcaça, fusível, clemas seguras, isolamento.
- Sem curtos-circuitos entre alimentação e terra (teste com multímetro).

Teste com multímetro - [Multímetro](../05-tools/02-multimeter.md).

## O que vem a seguir

A parte de hardware está montada. Vá para [Arranque de firmware no núcleo](04-firmware-start.md): criar um projecto e levar o dispositivo ao estado Online no portal.
