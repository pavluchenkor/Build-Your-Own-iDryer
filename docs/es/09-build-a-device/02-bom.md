---
title: "Composición del sistema de gabinete calefaccionado: componentes y dos versiones de la parte de potencia"
description: "Lista de componentes para un gabinete calefaccionado DIY basado en ESP32: sensor SHT31, termistor, calefactor y ventilador en versión de bajo voltaje (24V) y versión de red (220V)."
---

# Composición del sistema

En esta página encontrará la lista de componentes del dispositivo y dos variantes de la parte de potencia. La parte de bajo voltaje (controlador y sensores) es igual en ambas versiones. Solo difiere la forma en que se conmutan el calefactor y el ventilador.

## Parte de bajo voltaje (común en ambas versiones)

| Nodo | Propósito | Nota |
|------|-----------|------|
| ESP32-C3 o ESP32-S3 | Controlador: lógica, Wi-Fi, portal | DevKit o Super Mini funcionarán |
| Sensor SHT31 | Temperatura y humedad del aire en el gabinete | Interfaz I2C |
| Termistor NTC 100K | Control de temperatura del calefactor | Por ejemplo, Generic 3950 |
| Resistencia de pull-up del termistor | Divisor de voltaje para ADC | Generalmente `4.7 kΩ` |
| Fuente de alimentación | Alimentación del controlador y periféricos de bajo voltaje | Voltaje según la versión elegida |

C3 y S3 son equivalentes para este ejemplo: el código, la conexión y el orden de ensamblaje son los mismos. El S3 es más potente y tiene más memoria — elíjalo si más adelante quiere agregar una pantalla, una tira LED direccionable o su propia lógica pesada.

ESP32 fue elegido porque tiene Wi-Fi, las interfaces necesarias (I2C para SHT31, ADC para el termistor, PWM para el control de carga) y es directamente soportado por `idryer-core`. Más detalles — [Controlador ESP32](../02-controllers/01-esp32-controller.md).

!!! warning "Lógica ESP32 — 3.3V"
    ESP32 funciona a `3.3V`. No aplique `5V` a sus pines. Esto se aplica a sensores, módulos y adaptadores. Más detalles — [Errores de controladores](../08-common-mistakes/04-controller-mistakes.md).

## Sensores

**SHT31** mide la temperatura y la humedad del aire dentro del gabinete. Esta es la retroalimentación principal: le muestra si se mantiene el clima especificado. Se conecta por I2C (dos líneas: `SDA`, `SCL`). Más detalles — [Termistores y sensores climáticos](../03-common-components/04-thermistors.md).

**Termistor** mide la temperatura del calefactor mismo, no del aire. Es necesario para evitar que el calefactor se sobrecaliente: el aire se calienta lentamente, pero el calefactor se calienta rápidamente. El termistor se conecta como divisor de voltaje a un pin ADC. [Verificación del termistor](../06-practical-guides/02-checking-thermistor.md).

!!! note "Por qué dos sensores de temperatura"
    SHT31 dice «cuál es la temperatura en el gabinete», el termistor — «¿se sobrecalentó el calefactor?». El primero establece el objetivo, el segundo protege contra una emergencia.

## Parte de potencia: elija una versión

El calefactor y el ventilador son cargas controladas por el controlador. ESP32 no puede conmutar tales cargas directamente: su pin genera una señal débil de `3.3V`. Entre el controlador y la carga se necesita una llave.

Hay dos versiones fundamentalmente diferentes. Elija una según el calefactor y ventilador que esté utilizando.

### Versión A — bajo voltaje (24V o 12V)

El calefactor y el ventilador se alimentan con `24V` (o `12V`) de corriente continua. Este es un camino más simple y seguro para el ensamblaje DIY.

| Nodo | Componente |
|------|-----------|
| Calefactor | Elemento calefactor `12V` o `24V` (calefactor PTC) |
| Ventilador | Ventilador `24V` o `12V` (2 pines o 4 pines) |
| Llave del calefactor | Módulo MOSFET |
| Llave del ventilador | Módulo MOSFET (o 4 pines PWM directamente) |
| Fuente de alimentación | `24V DC` con margen de potencia |

El controlador controla el módulo MOSFET con una señal del pin ESP32. El módulo conmuta la carga de bajo voltaje. Esta es la misma lógica que en un controlador listo para usar. Más detalles — [Módulo MOSFET](../01-electronics-basics/02-mosfet-module.md).

La potencia de la fuente de alimentación se calcula para la carga total combinada con margen — ver [Cálculo de corriente de carga 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Versión recomendada para el primer dispositivo"
    Si ensambla un dispositivo por primera vez, comience con la versión A. Aquí no hay voltaje de red en la carga, y un error de montaje es menos peligroso.

### Versión B — red (110–230V AC)

El calefactor y el ventilador se alimentan de la red `110–230V`. Esto se hace cuando se necesita un calefactor de red potente, por ejemplo, un calefactor de ventilador listo para usar para un gabinete. Aquí se utilizan módulos de conmutación de CA en lugar de módulos MOSFET.

| Nodo | Componente |
|------|-----------|
| Calefactor | Calefactor de red `110–230V AC` |
| Ventilador | Ventilador de red `110–230V AC` |
| Llave del calefactor | Relé de estado sólido (SSR) para AC |
| Llave del ventilador | SSR o relé ordinario para AC |
| Fuente de alimentación | `24V`/`5V DC` separado para controlador y sensores |
| Protección | Fusible, puesta a tierra de protección del gabinete |

!!! danger "El voltaje de red es peligroso para la vida"
    La versión B funciona con voltaje `110–230V`. Un error de montaje puede causar electrocución o incendio. Antes del ensamblaje, asegúrese de leer los materiales de seguridad: [Triac](../01-electronics-basics/03-triac.md), [Relé de estado sólido (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Errores de calefactores y SSR](../08-common-mistakes/05-heater-ssr-mistakes.md). Si no tiene experiencia con voltaje de red, elija la versión A.

El controlador y los sensores en la versión B aún se alimentan desde una fuente de bajo voltaje separada (`5V`/`24V`). La parte de red y la parte de bajo voltaje deben estar física y eléctricamente separadas.

## Módulos opcionales

Estos nodos no son obligatorios para el gabinete, pero son compatibles con el núcleo y se pueden agregar más adelante:

- iluminación LED direccionable (`hasLed`);
- sensor de peso para medir el consumo de filamento (`hasWeight`);
- etiqueta RFID de carrete (`hasRfid`).

El gabinete básico no los utiliza — comenzamos con lo mínimo.

## Qué sigue

Cuando los componentes estén seleccionados, proceda a [Diagrama de cableado](03-wiring.md): qué pin de ESP32 es responsable de qué y cómo separar las partes de bajo voltaje y potencia.
