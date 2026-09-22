---
title: "Filtro de aire inteligente: concepto y qué proporciona el portal"
description: "Ejemplo integral nº 2: filtro de aire para zona de impresión 3D en ESP32 e idryer-core — sensor VOC propio, control propio y tarjeta automática en el portal a través del card manifest."
---

# Filtro de aire inteligente: concepto

Este es el segundo ejemplo integral de la sección "Construye tu propio". En el [primer ejemplo](../09-build-a-device/01-concept.md) construiste un gabinete calefaccionado a partir de los "ladrillos" estándar del ecosistema: temperatura, humedad, calefactor. Aquí damos un paso adelante — construimos un dispositivo **que no existe en el ecosistema iDryer en absoluto**: un filtro de aire con sensor de compuestos orgánicos volátiles (VOC).

## La idea principal: es un concepto, no una receta de un dispositivo único

Lee este párrafo con cuidado — es más importante que el resto del capítulo.

El filtro aquí es solo un ejemplo. El enfoque mostrado funciona para **cualquier dispositivo que se te ocurra**: humidificador, estación de soplado, controlador de extracción, monitor de almacén de filamento, cualquier cosa. Declaras en el firmware qué sensores y órganos de control tiene el dispositivo — una o dos líneas de código por cada uno — y el dispositivo **aparece automáticamente en el portal y en la aplicación móvil** con una tarjeta lista: lecturas en vivo, botones, campos de entrada. Ni una línea de código en el lado del portal, ni acuerdos con el equipo de iDryer, ni pull requests.

Esto funciona gracias al mecanismo de **tarjetas dinámicas** (entity manifest): el dispositivo publica una descripción legible por máquina de "qué mostrar y cómo controlar", y el portal y la aplicación construyen la interfaz según esa descripción. Cómo se ve en el código — [capítulo sobre la tarjeta](06-card.md).

!!! note "Qué significa esto en la práctica"
    Ideaste un dispositivo → lo armaste en ESP32 → describiste los sensores y botones en el firmware → lo vinculaste a tu cuenta en la aplicación. Listo: el dispositivo tiene interfaz en el portal y en la aplicación. Distancia de la idea a "controlar desde el smartphone" — una noche.

## Qué exactamente construimos

**Filtro de aire para zona de impresión 3D**: una caja con ventilador, filtro HEPA y capa de carbón, que:

- mide la calidad del aire con sensor VOC (SGP40);
- enciende el ventilador automáticamente cuando el aire está sucio y lo apaga cuando se limpia;
- muestra el índice VOC y el estado del ventilador en el portal;
- permite elegir el modo desde el portal (`auto` / `on` / `off`) y ajustar el umbral de activación.

ABS y ASA emiten estireno al imprimir, las resinas tienen su propio aroma. Un filtro en la impresora no es lujo, es higiene.

## Por qué es el primer proyecto ideal

Si el gabinete de la sección 09 te pareció complicado — comienza con el filtro:

- **sin calefactor** — significa sin parte de potencia, fusibles térmicos y riesgos;
- número mínimo de componentes: placa, sensor, ventilador, transistor;
- presupuesto alrededor de `$15` sin carcasa;
- en caso de cualquier error en el código, lo peor que sucede es que el ventilador no se encienda.

## Límites de la tarea

Seamos honestos sobre lo que este filtro **no es**:

- no es un extractor: el aire circula en bucle a través del filtro, no se expulsa al exterior;
- no es un dispositivo médico: SGP40 muestra el **índice** relativo de calidad del aire, no la concentración de gas específico en ppm;
- el filtro no reemplaza la ventilación.

!!! note "¿VOC o CO2?"
    Para vapores de impresión, el sensor correcto es VOC: reacciona a compuestos orgánicos (estireno, disolventes). Los sensores de CO2 (por ejemplo, sensor NDIR MH-Z19) miden dióxido de carbono — es un indicador de aire viciado, no de contaminación por impresión. Si quieres ambos, ENS160 proporciona índice VOC y estimación eCO2 simultáneamente; el enfoque de esta sección no cambia — solo una línea más en el manifest de la tarjeta.

## Ruta de la sección

1. [Composición del sistema](02-bom.md) — qué comprar.
2. [Esquema de conexiones](03-wiring.md) — cómo conectar.
3. [Inicio del firmware](04-firmware-start.md) — estructura en `idryer-core`, vinculación al portal.
4. [Sensor y telemetría](05-sensor-and-telemetry.md) — leemos VOC y enviamos a la nube.
5. [Tarjeta del dispositivo](06-card.md) — declaramos sensores y controles, obtenemos interfaz.
6. [Lógica de automatización](07-auto-logic.md) — umbral, histéresis, modo manual desde el portal.
7. [Montaje y verificación](08-assembly-and-check.md) — lista de verificación final.
