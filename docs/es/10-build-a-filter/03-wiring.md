---
title: "Filtro inteligente: esquema de conexiones"
description: "Conexión SGP40 por I2C y ventilador mediante llave MOSFET a ESP32-C3: pines, alimentación, errores típicos."
---

# Esquema de conexiones

El esquema es simple: sensor en I2C, ventilador a través de una llave, alimentación común de 12 V.

```text
PSU 12 V ──┬────────────────────────► Ventilador (+)
           │                         Ventilador (−) ◄── MOSFET (drenador)
           │                                            MOSFET (fuente) ─► GND
           │                                            MOSFET (compuerta) ◄─ GPIO4 ESP32
           │
           └─► buck 12→5 V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Pines

| Señal | Pin ESP32-C3 Super Mini |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| Compuerta MOSFET (ventilador) | GPIO4 |

Puedes elegir otros pines — luego cambia los números en el código ([capítulo 5](05-sensor-and-telemetry.md)).

## Reglas de conexión

1. **Tierra común.** GND de la fuente, ESP32, módulo MOSFET y sensor deben estar conectados. La mitad de los "no funciona" en proyectos caseros — tierra común olvidada.
2. **Sensor — solo a 3.3 V.** SGP40 no tolera 5 V en la alimentación.
3. **Ventilador — solo a través de la llave.** GPIO proporciona miliamperios; el ventilador consume cientos. La conexión directa quemará el pin. Cómo funciona una llave MOSFET — [Transistores y llaves](../01-electronics-basics/02-mosfet-module.md).
4. **Diodo de protección externo** para un ventilador de computadora generalmente no es obligatorio: el ventilador tiene su propia electrónica de conmutación adentro, y externamente parece una carga electrónica, no una inductancia pura. Pero al conmutar la línea de alimentación con la llave (especialmente con PWM), un diodo de derivación paralelo al ventilador es útil como protección de la llave contra sobretensión inductiva — y si ya está en el módulo de la llave, es solo una ventaja.

!!! warning "Verifica la polaridad antes de encender"
    La polaridad invertida en la línea de 12 voltios daña el módulo buck y, a menudo, la placa. Comprueba con un multímetro antes de la primera conexión de energía.

## Verificación sin firmware

Después de montar, antes de cargar el código principal:

1. Suministra 12 V — ESP32 debe detectarse en el sistema como dispositivo USB cuando conectes el cable (o debe brillar un LED de alimentación).
2. Cierra brevemente la compuerta MOSFET a 3.3 V a través de una resistencia de 1 kOhm — el ventilador debe encenderse.
3. Verificaremos el sensor I2C desde el firmware con un escáner de bus en el [capítulo 5](05-sensor-and-telemetry.md).
