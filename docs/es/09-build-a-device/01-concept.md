---
title: "Construimos nuestro dispositivo en el núcleo iDryer: concepto"
description: "Ejemplo integral: cómo construir desde cero un armario de almacenamiento calefaccionado de filamento en ESP32 y la biblioteca idryer-core con conexión al portal iDryer."
---

# Construimos nuestro dispositivo: concepto

Esta sección es un ejemplo integral. Las secciones anteriores explicaban bloques separados: alimentación, controladores, sensores, calefactores, seguridad. Aquí construye uno desde estos bloques y lo lleva a un estado funcional con conexión al [portal iDryer](https://portal.idryer.org/).

El ejemplo se basa en la biblioteca `idryer-core`. La biblioteca se encarga de toda la integración de red: conexión a Wi-Fi, vinculación de cuenta, sesión MQTT segura, publicación periódica de telemetría. Usted solo escribe lo específico de su dispositivo: lectura de sensores, control del calefactor y ventilador, lógica de mantenimiento de temperatura.

## Qué exactamente construimos

Construimos **un armario de almacenamiento calefaccionado de filamento**. Es un armario cerrado para 10–40 carretes, donde se mantiene una temperatura de alrededor de `40–45 °C`.

Es importante delinear los límites de la tarea desde el principio.

!!! note "Este no es un secador de alta temperatura"
    No pretendemos secado rápido a alta temperatura. El objetivo del dispositivo es mantener calor suave en el armario que mantiene el filamento seco durante el almacenamiento.

La temperatura de `40–45 °C` es suficiente para almacenar la mayoría de plásticos no exigentes, desde PLA hasta ABS, en estado seco. Para secado activo de materiales exigentes (nylon, policarbonato, PA-CF) se necesitan temperaturas más altas y una construcción diferente: tales secadores se arman por separado, según los principios de las otras secciones.

## Por qué hacerlo usted mismo

El controlador iDryer listo ya hace todo lo que se describe a continuación. Este ejemplo no es un sustituto, sino para mostrar **cómo está estructurado el dispositivo internamente** y proporcionar una base para sus propios módulos.

El ensamblaje independiente tiene sentido cuando:

- necesita un armario de tamaño o forma no estándar;
- quiere entender cómo el controlador controla la calefacción y se comunica con el portal;
- planea hacer su propio módulo de ecosistema y toma este ejemplo como punto de partida.

## Cómo difiere del controlador V2

El controlador de serie iDryer V2 es de dos procesadores: la lógica principal se ejecuta en un microcontrolador separado, y el módulo ESP32 solo funciona como puente a Wi-Fi y el portal. Esto se justifica para un producto de serie con pantalla, básculas, RFID y varias cámaras.

Para un armario casero, esta complejidad no es necesaria. Simplificamos la arquitectura a **un solo ESP32**, que hace todo por sí mismo:

- lee sensores;
- controla el calefactor y ventilador;
- se conecta a Wi-Fi y al portal a través de `idryer-core`.

Funcionalmente repetimos el comportamiento de una cámara del controlador V2 (sensor de clima, calefactor con retroalimentación de termistor, ventilador), pero en una implementación DIY honesta en una placa.

!!! note "Servo no se utiliza"
    En el controlador V2, un servomotor controla una compuerta de aire de la cámara. Para un armario de almacenamiento con calefacción suave uniforme, la compuerta no es necesaria, por lo que este ejemplo no tiene servomotor.

## Lo que da la conexión al núcleo

Cuando el dispositivo se construye en `idryer-core` y se vincula a la cuenta, obtiene sin código adicional:

- control y monitoreo a través del [portal](https://portal.idryer.org/) y aplicación móvil;
- gráfico de temperatura y humedad en el armario;
- inicio y parada remota del modo de mantenimiento de calor;
- configuración de parámetros (temperatura objetivo, histéresis) a través del menú del dispositivo.

!!! note "Sensores y controles propios en la tarjeta"
    La tarjeta del dispositivo no está limitada a las capacidades del vocabulario (`has*`). A través de un card-manifest, el firmware puede declarar cualquier sensor y control personalizados: aparecerán automáticamente en el portal y en la aplicación. Cómo se hace esto se muestra en el ejemplo integral ["Filtro de aire inteligente"](../10-build-a-filter/01-concept.md), especialmente en [el capítulo sobre la tarjeta](../10-build-a-filter/06-card.md). El arranque y la parada del mantenimiento de calor se declaran como acciones de la tarjeta en el [capítulo 7](07-heating-control.md).

## De qué consta esta sección

A continuación va un camino paso a paso desde una placa vacía hasta un armario funcional:

1. [Composición del sistema](02-bom.md) — qué componentes tomar y dos versiones de la parte de potencia (baja tensión y red).
2. [Esquema de conexión](03-wiring.md) — mapa de pines ESP32, aislamiento de partes de baja y alta potencia, seguridad.
3. [Inicio del firmware en el núcleo](04-firmware-start.md) — proyecto PlatformIO, primer inicio, vinculación al portal.
4. [Sensores](05-sensors.md) — conectamos SHT31 y termistor, obtenemos datos de ellos.
5. [Menú de YAML](06-menu.md) — describimos la configuración del dispositivo, entra en NVS y al portal.
6. [Control de calefacción](07-heating-control.md) — lógica de mantenimiento de temperatura, ventilador, arranque y parada desde la tarjeta del dispositivo.
7. [Ensamblaje y verificación](08-assembly-and-check.md) — ensamblaje final, primer calentamiento, lista de verificación de seguridad.

!!! tip "Ejemplo completado"
    Si desea ver el resultado de inmediato, el proyecto terminado está en la carpeta `example/09-cabinet/` del repositorio y se construye con el comando `pio run -e cabinet`. Los capítulos a continuación desglosan este mismo código paso a paso.

## Véase también

- [Por dónde empezar](../00-start-here/01-introduction.md) — orden general de lectura de la sección.
- [Controlador ESP32](../02-controllers/01-esp32-controller.md) — por qué ESP32 es conveniente para un dispositivo con Wi-Fi.
- [Componentes comunes](../03-common-components/01-overview.md) — mapa de componentes del dispositivo.
