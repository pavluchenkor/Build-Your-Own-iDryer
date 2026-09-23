---
title: "Esquema de conexão do gabinete aquecido em ESP32"
description: "Mapa de pinos ESP32 para gabinete caseiro: SHT31 em I2C, termistor em ADC, aquecedor e ventilador via chave. Isolamento entre parte de sinal fraco e de potência."
---

# Esquema de conexão

Nesta página está como conectar os componentes ao redor do ESP32. Primeiro um mapa geral de pinos, depois a conexão de cada nó e regras de roteamento da parte de potência.

!!! warning "Primeiro verifique o pinout da sua placa"
    Os números de pino abaixo são um exemplo. Placas diferentes ESP32-C3 e ESP32-S3 têm numeração e localização de pinos diferentes. Antes de montar, verifique com o pinout específico da sua placa de desenvolvimento: no DevKit e no Super Mini ele é publicado pelo fabricante e consta na descrição da placa. Nem todos os pinos podem ser usados livremente: alguns estão ocupados por boot, flash ou USB.

## Mapa de pinos (exemplo)

| Nó | Linha | Pino ESP32 (exemplo) |
|------|-------|----------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Termistor | sinal ADC | GPIO2 |
| Aquecedor (chave) | controle | GPIO4 |
| Ventilador (chave/PWM) | controle | GPIO5 |

Alimentação dos sensores — `3.3V` e `GND` da placa. A parte de potência é alimentada separadamente.

## SHT31 em I2C

SHT31 é conectado com quatro fios:

1. `VCC` do sensor — para `3.3V` da placa.
2. `GND` do sensor — para `GND` da placa.
3. `SDA` do sensor — para o pino `SDA` (exemplo: GPIO8).
4. `SCL` do sensor — para o pino `SCL` (exemplo: GPIO9).

As linhas I2C são curtas. Se o sensor fica longe da placa, mantenha os fios o mais curtos possível e torcidos. A maioria dos módulos SHT31 já têm resistores de pull-up na placa do módulo.

!!! note "Endereço SHT31"
    SHT31 normalmente tem endereço `0x44` (às vezes `0x45`). Se o sensor não responde, verifique o endereço e as linhas `SDA`/`SCL`.

## Termistor em ADC

O termistor é incluído em um divisor de tensão junto com um resistor de pull-up:

1. Um pino do termistor — para `3.3V`.
2. O outro pino do termistor — para o ponto de conexão com resistor `4.7 kΩ` e para pino ADC (exemplo: GPIO2).
3. O outro pino do resistor `4.7 kΩ` — para `GND`.

O controlador mede a tensão no ponto central do divisor e calcula a resistência do termistor, depois a temperatura. O tipo do termistor é definido na firmware (veja [Controle de aquecimento](07-heating-control.md)).

Detalhes sobre verificação e montagem — [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md).

## Aquecedor e ventilador via chave

O ESP32 controla a carga não diretamente, mas através de uma chave. Qual chave — depende da versão de [Composição do sistema](02-bom.md).

### Versão A (24V/12V) — módulo MOSFET

1. Entrada de sinal do módulo (`PWM`/`SIG`) — para o pino de controle ESP32 (exemplo: GPIO4 para aquecedor, GPIO5 para ventilador).
2. `GND` do módulo — para `GND` comum com ESP32.
3. Entrada de alimentação do módulo e carga — para fonte de alimentação `24V`.

!!! warning "Terra comum"
    `GND` do controlador e `GND` da fonte de alimentação de potência devem estar conectados. Sem terra comum, o sinal de controle não tem nível de referência, e a chave funciona de forma imprevisível.

A conexão do ventilador com controle é discutida em detalhes em [Conexão de ventilador](../06-practical-guides/01-connecting-fan.md). Lógica da chave — [Módulo MOSFET](../01-electronics-basics/02-mosfet-module.md).

### Versão B (220V) — SSR/relé

!!! danger "Antes de montar a parte de rede"
    Todas as conexões com a rede devem ser feitas com o dispositivo completamente desligado. O gabinete com a parte de rede deve ter aterramento de proteção e fusível. Use fios de rede com seção apropriada e fixe-os nos terminais com segurança.

O SSR tem dois lados. **Controlado** — entrada de baixa voltagem que o controlador comanda. **Potência** — saídas através das quais a tensão de rede da carga passa. Os lados são isolados um do outro por opto-acoplador dentro do SSR, então você pode controlar a rede com um sinal fraco `3.3V`.

1. A entrada de controle geralmente é marcada como `DC+` e `DC-` (às vezes `+` e `-`) e é projetada para `3–32V` DC. Conecte `DC+` ao pino de controle ESP32 (exemplo: GPIO4) e `DC-` ao `GND` do controlador. A tensão `3.3V` do pino ESP32 é suficiente para abrir o SSR.
2. Os pinos de potência (geralmente marcados como rede/`AC` e carga/`LOAD`) são incluídos na quebra de um dos fios de rede do aquecedor — como um interruptor no fio.
3. O ventilador é comutado por um SSR ou relé separado da mesma forma.

!!! note "Por que SSR precisa de radiador"
    Ao comutar SSR esquenta um pouco, e quanto maior a corrente de carga, mais quente. Portanto, o SSR é parafusado em um radiador (placa de metal para dissipação de calor), e o próprio SSR é escolhido com margem de corrente — notavelmente acima da corrente de carga. Qual margem e radiador você precisa para sua corrente — [Relé de estado sólido (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Roteamento: sinal fraco e parte de potência

- Mantenha fios de sinal (sensores, controle) separados dos fios de potência.
- Não execute fios de termistor e I2C ao longo de fios de potência do aquecedor — esta é uma fonte de interferência.
- Na versão B, separe fisicamente as zonas de rede e de baixa voltagem dentro do gabinete.
- Todas as terras da parte de baixa voltagem devem converger em um ponto.

Interferência do ventilador e terra ruim são causa comum de "leituras flutuantes" e reinicializações. Veja [Erros de fiação](../08-common-mistakes/03-wiring-mistakes.md).

## O que verificar antes de energizar

- Alimentação dos sensores `3.3V`, não `5V`.
- Termistor e resistor divisor montados corretamente, pino ADC no ponto central.
- Terra comum do controlador e fonte de alimentação de potência.
- Na versão B — aterramento do gabinete, fusível, terminais confiáveis, isolamento.
- Sem curtos entre alimentação e terra (teste com multímetro).

Verificação com multímetro — [Multímetro](../05-tools/02-multimeter.md).

## O que vem a seguir

A parte de hardware está montada. Vá para [Início da firmware no núcleo](04-firmware-start.md): criar um projeto e levar o dispositivo ao estado Online no portal.
