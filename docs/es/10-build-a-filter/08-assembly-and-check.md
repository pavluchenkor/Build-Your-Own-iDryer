---
title: "Filtro inteligente: montaje y verificación final"
description: "Montaje del filtro en carcasa, orden de capas de filtración y lista de verificación integral: sensor, telemetría, tarjeta, comandos, automatización."
---

# Montaje y verificación

## Montaje

1. **Carcasa.** Caja con dos ventanas: entrada de aire y salida. La imprimes para tu cartucho filtro o reutilizas una existente.
2. **Capas en el flujo de aire:** entrada → HEPA → carbón → ventilador (en salida) → salida. Sin grietas en las juntas: el aire es perezoso e irá alrededor del filtro si puede.
3. **Sensor** — en el flujo de entrada, antes de los filtros: debe oler el aire sucio de la sala, no el limpio.
4. **Electrónica** — en compartimiento separado o en la pared, lejos del flujo de polvo. La placa — en pernos, no "en montón".
5. Asegura los cables: la vibración del ventilador con el tiempo afloja todo lo que no está asegurado.

## Lista de verificación integral

Verifica en orden — cada punto se basa en los anteriores.

| # | Verificación | Cómo |
|---|---|---|
| 1 | Alimentación | 12 V en la línea del ventilador, 5 V después del buck, 3.3 V en el sensor |
| 2 | Sensor activo | en el registro Serial índice ~100 en aire limpio, crece del aliento |
| 3 | Dispositivo En línea | estado en el portal después de vincularlo en la aplicación |
| 4 | Telemetría | `vocIndex` y `fanStatus` en el flujo del dispositivo |
| 5 | Tarjeta | celdas VOC y Ventilador, lista Mode, campo Threshold |
| 6 | Comando desde portal | Mode → `on`: ventilador se encendió, tarjeta muestra "Encendido" |
| 7 | Automatización | Mode → `auto`, soplar: se encendió en el umbral, se apagó debajo |
| 8 | Reinicio | modo y umbral se guardaron, tarjeta se encendió sola |

## Qué sigue

El filtro está listo. Después — según el gusto:

- **Más entidades**: botón "purgar 5 minutos" (`card().button(...)`), segundo sensor, contador de horas de filtro con aviso de reemplazo;
- **Diseño bonito**: el `layoutRow` de fábrica que ya viste;
- **Tus propios dispositivos**: toda esta sección es una plantilla. Cambia el sensor, el mecanismo y la lógica — y por el mismo esquema construyes humidificador, extractor, controlador de lo que sea. El manifest hará el interface por sí solo.

Si algo no funciona — [Errores típicos](../08-common-mistakes/01-overview.md).
