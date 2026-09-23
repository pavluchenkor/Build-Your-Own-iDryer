---
title: "Filtro inteligente: esquema de ligações"
description: "Ligação de SGP40 por I2C e ventilador através de chave MOSFET para ESP32-C3: pinos, alimentação, erros típicos."
---

# Esquema de ligações

O esquema é simples: sensor em I2C, ventilador através de chave, alimentação comum 12 V.

```text
Fonte 12 V ──┬────────────────────────► Ventilador (+)
             │                          Ventilador (−) ◄── MOSFET (dreno)
             │                                             MOSFET (fonte) ─► GND
             │                                             MOSFET (gate) ◄─ GPIO4 ESP32
             │
             └─► buck 12→5 V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Pinos

| Sinal | Pino ESP32-C3 Super Mini |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| Gate MOSFET (ventilador) | GPIO4 |

Pode escolher outros pinos — mude os números no código ([capítulo 5](05-sensor-and-telemetry.md)).

## Regras de ligação

1. **Massa comum.** GND da fonte, ESP32, módulo MOSFET e sensor devem estar ligados. Metade dos «não funciona» em projetos caseiros — massa comum esquecida.
2. **Sensor — apenas 3.3 V.** SGP40 não suporta 5 V na alimentação.
3. **Ventilador — apenas através de chave.** GPIO fornece miliamperes; ventilador consome centenas. Ligação direta queima o pino. Como funciona a chave MOSFET — [Transístores e chaves](../01-electronics-basics/02-mosfet-module.md).
4. **Díodo de protecção externa** para ventilador de computador geralmente não é obrigatório: o ventilador tem eletrónica de comutação interna e, externamente, parece uma carga electrónica, não uma indutância pura. Mas ao comutar a linha de alimentação com a chave (especialmente com PWM), um díodo de proteção em paralelo com o ventilador é útil contra picos indutivos na chave — e se já está no módulo da chave, é apenas mais um benefício.

!!! warning "Verifique a polaridade antes de ligar"
    Polaridade invertida na linha 12 V destrói o módulo buck e frequentemente a placa. Use multímetro antes da primeira ligação.

## Verificação sem firmware

Após a montagem, antes de carregar o código principal:

1. Aplique 12 V — ESP32 deve ser reconhecido no sistema como dispositivo USB quando o cabo é ligado (ou acender LED de alimentação).
2. Ligue brevemente o gate MOSFET a 3.3 V através de resistor 1 kΩ — ventilador deve ligar.
3. Sensor I2C verificaremos já na firmware com varredor de barramento no [capítulo 5](05-sensor-and-telemetry.md).
