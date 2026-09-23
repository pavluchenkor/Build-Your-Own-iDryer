---
title: "Filtro inteligente: composición del sistema (BOM)"
description: "Lista de componentes del filtro de aire: ESP32-C3, sensor VOC SGP40, ventilador de 120 mm, filtros HEPA y carbón, llave MOSFET, fuente de alimentación."
---

# Composición del sistema

Lista completa de componentes. Los precios son orientativos; todo se compra en cualquier marketplace.

## Electrónica

| Componente | Ejemplo | Precio | Propósito |
|---|---|---|---|
| Placa ESP32-C3 | ESP32-C3 Super Mini o similar | ~$3 | cerebro del dispositivo, Wi-Fi |
| Sensor VOC | SGP40 (módulo, I2C) | ~$4 | índice de calidad del aire |
| Ventilador 120 mm, 12 V | cualquier ventilador de carcasa, preferiblemente con rodamiento hidráulico | ~$5 | circulación de aire a través del filtro |
| Llave MOSFET | módulo en AO3400/IRLZ44N o "módulo MOSFET switch" prefabricado | ~$1 | activación del ventilador desde GPIO de 3.3 voltios |
| Fuente de alimentación 12 V / 1 A | cualquiera de buena calidad | ~$4 | alimentación del ventilador |
| Módulo reductor 12→5 V | mini-360 (buck) | ~$1 | alimentación de ESP32 desde la misma PSU |

Sobre la selección de placas — [Controladores](../02-controllers/00-how-to-choose-controller.md), sobre alimentación y módulos reductores — [Fundamentos de electrónica](../01-electronics-basics/01-load-calculation-24v.md).

## Parte filtrante

| Componente | Ejemplo | Propósito |
|---|---|---|
| Filtro HEPA | cartucho redondo de purificador de aire automotriz/doméstico | atrapa partículas |
| Carbón activado | gránulos en casete o lámina de carbón | absorbe VOC y olores |
| Carcasa | se imprime (diseña STL para tu filtro) o cualquier caja adecuada | mantiene todo junto |

!!! note "Orden de capas"
    El aire debe fluir: entrada → HEPA → carbón → ventilador → salida. El ventilador se monta "en salida", después de los filtros: así el aire sucio no se aspira por las grietas de la carcasa evitando el filtro. En principio esto no es tan importante — lo que funciona es la tasa de renovación del volumen de aire en el tiempo: cuanto mayor sea el caudal del ventilador (CFM), menor es este tiempo.

## Por qué SGP40

- I2C, alimentación 3.3 V — se conecta a ESP32 con dos cables de señal;
- proporciona **índice VOC** 0..500 (100 — "aire normal", más — más sucio), no requiere calibración;
- hay biblioteca lista de Adafruit.

Alternativas:

- **ENS160** — índice VOC + estimación eCO2, también I2C. Buena opción "dos en uno";
- **MH-Z19B/C** — sensor NDIR real de CO2 (ppm), UART, ~$20. Excesivo para un filtro.

## Herramientas

Soldador, flux, estaño, multímetro, termoencogible. Detalles — [Herramientas](../05-tools/02-multimeter.md).
