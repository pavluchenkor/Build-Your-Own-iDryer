---
title: "Montaje del armario climatizado y verificación previa al inicio"
description: "Montaje final del armario DIY en ESP32: instalación en la carcasa, primer calentamiento, calibración de temperatura y lista de verificación de seguridad antes del funcionamiento permanente."
---

# Montaje y verificación

En esta página ensambla el dispositivo en la carcasa, realiza el primer calentamiento controlado y verifica que el armario funcione de forma segura. Realice las verificaciones en orden y no deje el dispositivo sin supervisión durante el primer encendido.

## Orden de montaje

1. Asegure el ESP32 y la parte de potencia en la carcasa de modo que las zonas de bajo voltaje y potencia estén separadas.
2. Coloque el sensor SHT31 en el armario lejos del flujo directo del calentador; de lo contrario, mostrará la temperatura de la corriente, no del aire en el volumen.
3. Asegure el termistor en contacto térmico con el calentador.
4. Verifique que los cables no toquen el calentador ni entren en contacto con el ventilador.
5. En la versión B (`220V`), asegúrese de que los cables de red estén asegurados en los terminales, el aislamiento esté intacto y la carcasa esté conectada a tierra.

Requisitos para la carcasa y la colocación de componentes — [Diseño de carcasa](../07-3d-printing/05-enclosure-design.md).

!!! warning "Piezas impresas cerca del calor"
    El PLA se ablanda a una temperatura que fácilmente se alcanza cerca del calentador. Las piezas cerca del calor deben imprimirse con materiales resistentes al calor: ABS, ASA o PA (nailon). Consulte [Materiales resistentes al calor](../07-3d-printing/04-heat-resistant-materials.md) y [Por qué PLA es arriesgado](../07-3d-printing/06-why-pla-is-risky.md).

## Verificación antes de aplicar energía

Verifique con un multímetro antes del primer encendido:

- no hay cortocircuito entre alimentación y tierra;
- la alimentación de los sensores es de `3.3V`, no `5V`;
- tierra común del controlador y la fuente de potencia;
- termistor y resistencia divisora están conectados correctamente;
- en la versión B — tierra de la carcasa y fusible en su lugar.

Cómo usar un multímetro — [Multímetro](../05-tools/02-multimeter.md).

## Primer encendido

1. Suministre energía solo al controlador y los sensores. Si la fuente de alimentación es única para todo, alimente el controlador por separado: desde USB o mediante un convertidor reductor, y deje sin conectar por ahora el cable del calentador.
2. Verifique que el dispositivo esté en línea en el portal y muestre temperatura y humedad.
3. Conecte el calentador y el ventilador.
4. Inicie el modo de mantenimiento de calor desde el portal y observe.

!!! danger "No deje el primer calentamiento sin supervisión"
    Al encender por primera vez, observe el dispositivo. Verifique que el calentador se apague al alcanzar el objetivo y por protección del termistor, no que caliente continuamente.

Lo que debe observar en los primeros minutos:

- la temperatura del aire sube y se estabiliza alrededor del objetivo;
- la temperatura del calentador no excede el límite establecido;
- el calentamiento se apaga al alcanzar el objetivo y se vuelve a encender después del enfriamiento por la cantidad de histéresis;
- el ventilador funciona y no toca los cables;
- el controlador no se reinicia al encender la carga.

## Calibración

Después del primer calentamiento, compare las lecturas con un termómetro separado en el armario:

- si la temperatura del aire en el armario difiere del objetivo — verifique la colocación del SHT31 (no debe estar en la corriente o contra la pared);
- si la temperatura del calentador parece poco realista — verifique el tipo de termistor y el valor de la resistencia divisora;
- si es necesario, corrija la temperatura objetivo y la histéresis en el [menú](06-menu.md).

## Si algo no funciona

| Síntoma | Dónde revisar |
|---------|---------------|
| El controlador se reinicia bajo carga | [Errores de alimentación](../08-common-mistakes/02-power-mistakes.md) |
| El sensor muestra valores sin sentido | [Errores de cableado](../08-common-mistakes/03-wiring-mistakes.md), [Verificación del termistor](../06-practical-guides/02-checking-thermistor.md) |
| El dispositivo no se conecta a Wi-Fi | [Errores del controlador](../08-common-mistakes/04-controller-mistakes.md) |
| El calentador/SSR se calienta mucho | [Errores de calentador y SSR](../08-common-mistakes/05-heater-ssr-mistakes.md) |

La secuencia de diagnóstico general — [Lista de verificación de diagnóstico](../08-common-mistakes/06-diagnostic-checklist.md).

## Lista de verificación antes del funcionamiento permanente

- [ ] El dispositivo mantiene la temperatura objetivo y no calienta continuamente.
- [ ] La protección del calentador por termistor funciona.
- [ ] Los cables no tocan el calentador ni el ventilador.
- [ ] Las piezas impresas cerca del calor son resistentes al calor.
- [ ] En la versión B: carcasa conectada a tierra, fusible instalado, aislamiento intacto.
- [ ] Los datos en el portal coinciden con la temperatura real en el armario.

## Conclusión

Usted ha montado un armario de almacenamiento climatizado en ESP32 e `idryer-core`: el dispositivo lee el clima y la temperatura del calentador, mantiene la temperatura establecida, protege el calentador del sobrecalentamiento y se controla desde el portal. Esta es una base completa sobre la cual puede construir sus propios módulos del ecosistema.

Los componentes posteriores — iluminación, básculas, RFID — también son compatibles con el núcleo; se pueden agregar siguiendo el mismo esquema: sensor o periférico → telemetría o comando → visualización en el portal.
