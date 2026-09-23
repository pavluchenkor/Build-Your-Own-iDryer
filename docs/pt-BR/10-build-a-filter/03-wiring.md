---
title: "Filtro inteligente: esquema de conexões"
description: "Conexão SGP40 por I2C e ventilador através de chave MOSFET ao ESP32-C3: pinos, alimentação, erros típicos."
---

# Esquema de conexões

O esquema é simples: sensor em I2C, ventilador através de chave, alimentação comum 12 V.

```text
Fonte 12 V ──┬────────────────────────► Ventilador (+)
             │                         Ventilador (−) ◄── MOSFET (dreno)
             │                                            MOSFET (fonte) ─► GND
             │                                            MOSFET (porta) ◄─ GPIO4 ESP32
             │
             └─► buck 12→5 V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Pinos

| Sinal | Pino ESP32-C3 Super Mini |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| Porta MOSFET (ventilador) | GPIO4 |

Você pode escolher outros pinos — então mude os números no código ([capítulo 5](05-sensor-and-telemetry.md)).

## Regras de conexão

1. **Terra comum.** GND da fonte, ESP32, módulo MOSFET e sensor devem estar conectados. Metade do "não funciona" em gambiarra — terra comum esquecida.
2. **Sensor — apenas em 3,3 V.** SGP40 não tolera 5 V de alimentação.
3. **Ventilador — apenas através de chave.** GPIO fornece miliampères; ventilador requer centenas. Conexão direta queimará o pino. Como funciona a chave MOSFET — [Transistores e chaves](../01-electronics-basics/02-mosfet-module.md).
4. **Diodo de proteção externo** para ventilador de computador geralmente não é obrigatório: o ventilador tem sua própria eletrônica de comutação internamente, e externamente parece uma carga eletrônica, não uma indutância pura. Mas ao comutar a linha de alimentação com uma chave (especialmente com PWM), um diodo de proteção paralelo ao ventilador é útil como proteção da chave contra picos indutivos — e se ele já está no módulo da chave, é um bônus.

!!! warning "Verifique a polaridade antes de ligar"
    Invertidas + e − na linha de 12 volts destroem o módulo buck e, frequentemente, a placa. Use o multímetro para sondar antes de ligar pela primeira vez.

## Verificação sem firmware

Após a montagem, antes de carregar o código principal:

1. Aplique 12 V — o ESP32 deve ser reconhecido no sistema como dispositivo USB ao conectar o cabo (ou LED de alimentação acende).
2. Conecte brevemente a porta MOSFET a 3,3 V através de um resistor de 1 kΩ — o ventilador deve ligar.
3. O sensor I2C verificaremos já via firmware com um scanner de barramento no [capítulo 5](05-sensor-and-telemetry.md).
