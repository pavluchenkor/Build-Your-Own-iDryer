---
title: "Esquema de conexión del armario calefactor en ESP32"
description: "Mapa de pines de ESP32 para armario hecho a sí mismo: SHT31 por I2C, termistor en ADC, calentador y ventilador mediante conmutador. Aislamiento de la parte de baja tensión y de potencia."
---

# Esquema de conexión

En esta página se explica cómo conectar los componentes alrededor del ESP32. Primero un mapa general de pines, luego la conexión de cada nodo y las reglas de enrutamiento de la parte de potencia.

!!! warning "Primero verifique la distribución de pines de su placa"
    Los números de pines que se muestran a continuación son un ejemplo. En diferentes placas ESP32-C3 y ESP32-S3, la numeración y distribución de pines son diferentes. Antes del montaje, consulte la distribución de pines de su placa de desarrollo específica: en las DevKit y Super Mini está publicada por el fabricante y figura en la descripción de la placa. No todos los pines se pueden utilizar libremente: algunos están ocupados por el arranque, flash o USB.

## Mapa de pines (ejemplo)

| Nodo | Línea | Pin ESP32 (ejemplo) |
|------|-------|----------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Termistor | señal ADC | GPIO2 |
| Calentador (conmutador) | control | GPIO4 |
| Ventilador (conmutador/PWM) | control | GPIO5 |

La alimentación de los sensores es `3.3V` y `GND` de la placa. La parte de potencia se alimenta por separado.

## SHT31 por I2C

El SHT31 se conecta con cuatro cables:

1. `VCC` del sensor — a `3.3V` de la placa.
2. `GND` del sensor — a `GND` de la placa.
3. `SDA` del sensor — al pin `SDA` (ejemplo: GPIO8).
4. `SCL` del sensor — al pin `SCL` (ejemplo: GPIO9).

Las líneas I2C son cortas. Si el sensor está lejos de la placa, mantenga los cables lo más cortos posible y trenzados. La mayoría de los módulos SHT31 ya tienen resistencias de polarización en la placa del módulo.

!!! note "Dirección SHT31"
    El SHT31 generalmente tiene la dirección `0x44` (a veces `0x45`). Si el sensor no responde, verifique la dirección y las líneas `SDA`/`SCL`.

## Termistor en ADC

El termistor se incluye en un divisor de tensión junto con una resistencia de polarización:

1. Un terminal del termistor — a `3.3V`.
2. El segundo terminal del termistor — en el punto de conexión con la resistencia `4.7 kΩ` y en el pin ADC (ejemplo: GPIO2).
3. El segundo terminal de la resistencia `4.7 kΩ` — a `GND`.

El controlador mide el voltaje en el punto medio del divisor y calcula la resistencia del termistor, y luego la temperatura. El tipo de termistor se especifica en el firmware (consulte [Control de calentamiento](07-heating-control.md)).

Para obtener detalles sobre verificación e instalación, consulte [Verificación del termistor](../06-practical-guides/02-checking-thermistor.md).

## Calentador y ventilador mediante conmutador

El ESP32 no controla la carga directamente, sino a través de un conmutador. Qué conmutador usar depende de la versión de [Composición del sistema](02-bom.md).

### Versión A (24V/12V) — módulo MOSFET

1. La entrada de señal del módulo (`PWM`/`SIG`) — en el pin de control de ESP32 (ejemplo: GPIO4 para el calentador, GPIO5 para el ventilador).
2. `GND` del módulo — a `GND` común con ESP32.
3. La entrada de alimentación del módulo y la carga — a la fuente de alimentación `24V`.

!!! warning "Tierra común"
    El `GND` del controlador y el `GND` de la fuente de alimentación de potencia deben estar conectados. Sin tierra común, la señal de control no tiene nivel de referencia y el conmutador funciona de manera impredecible.

La conexión del ventilador con control se analiza en detalle en [Conexión del ventilador](../06-practical-guides/01-connecting-fan.md). La lógica del conmutador se encuentra en [Módulo MOSFET](../01-electronics-basics/02-mosfet-module.md).

### Versión B (220V) — SSR/relé

!!! danger "Antes de instalar la parte de red"
    Realice todas las conexiones con la red con el dispositivo completamente desconectado de la alimentación. El gabinete con la parte de red debe tener protección a tierra y un fusible. Utilice cables de red de sección suficiente y asegúrelos firmemente en los terminales.

El SSR tiene dos lados. El lado **de control** es una entrada de baja tensión controlada por el controlador. El lado **de potencia** tiene salidas a través de las cuales pasa el voltaje de red de la carga. Los lados están aislados entre sí por un optoacoplador dentro del SSR, por lo que puede controlar la red con una señal débil de `3.3V`.

1. La entrada de control generalmente está marcada como `DC+` y `DC-` (a veces `+` y `-`) y está diseñada para `3-32V` de corriente continua. Conecte `DC+` al pin de control de ESP32 (ejemplo: GPIO4) y `DC-` a `GND` del controlador. El voltaje de `3.3V` del pin ESP32 es suficiente para abrir el SSR.
2. Los terminales de potencia (a menudo marcados como red/`AC` y carga/`LOAD`) se conectan en la ruptura de uno de los cables de red del calentador — de la misma manera que un interruptor en el cable.
3. El ventilador se conmuta con un SSR o relé separado de la misma manera.

!!! note "Por qué SSR necesita disipador"
    Cuando se conmuta el SSR, se calienta ligeramente, y cuanto mayor sea la corriente de carga, mayor será el calentamiento. Por lo tanto, el SSR se atornilla a un disipador de calor (una placa metálica para disipar el calor), y el SSR se toma con un margen de corriente — notablemente más alto que la corriente de carga. Qué margen y disipador se necesitan para su corriente — [Relé de estado sólido (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Enrutamiento: baja tensión y parte de potencia

- Mantenga los cables de señal (sensores, control) separados de los cables de potencia.
- No haga pasar los cables del termistor e I2C a lo largo de los cables de potencia del calentador — esta es una fuente de interferencia.
- En la versión B, separe físicamente las zonas de red y baja tensión dentro del gabinete.
- Conecte todas las tierras de la parte de baja tensión en un solo punto.

Las interferencias del ventilador y la mala tierra son una causa frecuente de lecturas "flotantes" y reinicios. Consulte [Errores de cableado](../08-common-mistakes/03-wiring-mistakes.md).

## Qué verificar antes de aplicar energía

- La alimentación de los sensores es `3.3V`, no `5V`.
- El termistor y la resistencia del divisor están montados correctamente, el pin ADC está en el punto medio.
- Tierra común del controlador y fuente de alimentación de potencia.
- En la versión B — puesta a tierra del gabinete, fusible, terminales confiables, aislamiento.
- No hay cortocircuitos entre la alimentación y tierra (verificar con multímetro).

La verificación con multímetro se encuentra en [Multímetro](../05-tools/02-multimeter.md).

## Qué viene después

La parte de hardware está completa. Pase a [Inicio del firmware en el núcleo](04-firmware-start.md): creamos un proyecto y llevamos el dispositivo al estado en línea en el portal.
