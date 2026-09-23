---
title: "Filtro inteligente: composição do sistema (BOM)"
description: "Lista de componentes do filtro de ar: ESP32-C3, sensor VOC SGP40, ventilador 120 mm, filtros HEPA e de carvão, chave MOSFET, alimentação."
---

# Composição do sistema

Lista completa de componentes. Os preços são aproximados, tudo é comprado em qualquer marketplace.

## Eletrônica

| Componente | Exemplo | Preço | Para quê |
|---|---|---|---|
| Placa ESP32-C3 | ESP32-C3 Super Mini ou similar | ~$3 | cérebro do dispositivo, Wi-Fi |
| Sensor VOC | SGP40 (módulo, I2C) | ~$4 | índice de qualidade do ar |
| Ventilador 120 mm, 12 V | qualquer ventilador de gabinete, melhor com mancal hidráulico | ~$5 | circula ar através do filtro |
| Chave MOSFET | módulo em AO3400/IRLZ44N ou "módulo de chave MOSFET" pronto | ~$1 | liga o ventilador do GPIO de 3,3 volts |
| Fonte de alimentação 12 V / 1 A | qualquer uma de boa qualidade | ~$4 | alimenta o ventilador |
| Módulo abaixador 12→5 V | mini-360 (buck) | ~$1 | alimenta o ESP32 da mesma fonte |

Sobre a escolha de placas — [Controladores](../02-controllers/00-how-to-choose-controller.md), sobre alimentação e módulos abaixadores — [Noções básicas de eletrônica](../01-electronics-basics/01-load-calculation-24v.md).

## Parte filtrante

| Componente | Exemplo | Para quê |
|---|---|---|
| Filtro HEPA | cartucho redondo de purificador automotivo/doméstico | captura partículas |
| Carvão ativado | grânulos em cartucho ou tapete de carvão | absorve VOC e odores |
| Gabinete | impresso (você projeta o STL para seu filtro) ou qualquer caixa adequada | mantém tudo junto |

!!! note "Ordem das camadas"
    O ar deve seguir: entrada → HEPA → carvão → ventilador → saída. O ventilador é colocado em exaustão, depois dos filtros: assim o ar sujo não é puxado através de fendas do gabinete contornando o filtro. Isto não é tão crítico — o que funciona é a renovação do volume de ar em determinado tempo: quanto maior a vazão do ventilador (CFM), mais curto é esse tempo.

## Por que SGP40

- I2C, alimentação 3,3 V — conecta-se ao ESP32 com dois fios de sinal;
- fornece **índice VOC** 0..500 (100 — "ar normal", mais alto — mais sujo), não requer calibração;
- há biblioteca pronta da Adafruit.

Alternativas:

- **ENS160** — índice VOC + estimativa de eCO2, também I2C. Boa opção "dois em um";
- **MH-Z19B/C** — sensor NDIR verdadeiro de CO2 (ppm), UART, ~$20. Excessivo para o filtro.

## Ferramentas

Ferro de solda, fluxo, solda, multímetro, tubo termorretrátil. Detalhes — [Ferramentas](../05-tools/02-multimeter.md).
