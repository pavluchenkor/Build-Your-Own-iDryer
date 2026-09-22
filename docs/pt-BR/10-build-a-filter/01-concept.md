---
title: "Filtro de Ar Inteligente: conceito e o que oferece ao portal"
description: "Exemplo completo nº 2: filtro de ar para zona de impressão 3D em ESP32 e idryer-core — sensor VOC próprio, lógica própria de controle e cartão automático no portal através do manifesto do cartão."
---

# Filtro de Ar Inteligente: conceito

Este é o segundo exemplo completo da seção "construa você mesmo". No [primeiro exemplo](../09-build-a-device/01-concept.md) você construiu um gabinete aquecido a partir dos "blocos de construção" do ecossistema iDryer: temperatura, umidade, aquecedor. Aqui damos um passo adiante — construímos um dispositivo **que não existe no ecossistema iDryer**: um filtro de ar com sensor de compostos orgânicos voláteis (VOC).

## Ideia principal: este é um conceito, não uma receita de um único dispositivo

Leia este parágrafo com cuidado — é mais importante que todo o resto do capítulo.

O filtro aqui é apenas um exemplo. A abordagem mostrada funciona para **qualquer dispositivo que você imaginar**: umidificador, estação de sopro, controlador de exaustão, monitor de armazenamento de filamento, qualquer coisa. Você declara no firmware quais sensores e controles o dispositivo possui — uma ou duas linhas de código para cada um — e o dispositivo **aparece automaticamente no portal e no aplicativo móvel** com um cartão pronto: leituras ao vivo, botões, campos de entrada. Sem uma única linha de código no lado do portal, sem acordos com a iDryer, sem pull-requests.

Isto funciona graças ao mecanismo de **cartões dinâmicos** (entity manifest): o dispositivo publica uma descrição legível por máquina "o que mostrar e como controlar", e o portal e aplicativo constroem a interface a partir dela. Como isso fica no código — [capítulo sobre o cartão](06-card.md).

!!! note "O que isto significa na prática"
    Você imaginou um dispositivo → construiu em ESP32 → descreveu sensores e botões no firmware → o vinculou à conta pelo app. Pronto: o dispositivo tem uma interface no portal e no aplicativo. A distância da ideia até "controle pelo smartphone" — uma noite de trabalho.

## O que exatamente construiremos

**Filtro de ar para zona de impressão 3D**: uma caixa com ventilador, filtro HEPA e camada de carvão que:

- mede a qualidade do ar com sensor VOC (SGP40);
- liga automaticamente o ventilador quando o ar está sujo e desliga quando está limpo;
- mostra no portal o índice VOC e o estado do ventilador;
- permite selecionar o modo do portal (`auto` / `on` / `off`) e ajustar o limiar de ativação.

ABS e ASA emitem estireno durante a impressão, resinas têm seu próprio aroma. Um filtro na impressora não é luxo, é higiene.

## Por que este é o projeto de estreia ideal

Se o gabinete da seção 09 parecia complexo — comece com o filtro:

- **sem aquecedor** — portanto sem circuito de potência, fusíveis térmicos e riscos;
- número mínimo de componentes: placa, sensor, ventilador, transistor;
- orçamento de cerca de `$15` sem gabinete;
- em caso de erro no código, o pior que pode acontecer é o ventilador não ligar.

## Limites da tarefa

Deixe claro o que este filtro **não é**:

- não é uma exaustão: o ar circula através do filtro, não é expelido para fora;
- não é um dispositivo médico: o SGP40 mostra um **índice** relativo de qualidade do ar, não a concentração de um gás específico em ppm;
- o filtro não substitui a ventilação.

!!! note "VOC ou CO2?"
    Para as emissões de impressão, o sensor correto é VOC: ele responde a compostos orgânicos (estireno, solventes). Sensores de CO2 (por exemplo, o sensor NDIR MH-Z19) medem dióxido de carbono — isto é um indicador de falta de circulação, não de contaminação da impressão. Se você quiser ambos, o ENS160 fornece índice VOC e estimativa de eCO2 simultaneamente; a abordagem desta seção não muda — apenas mais uma linha no manifesto do cartão.

## Roteiro da seção

1. [Composição do sistema](02-bom.md) — o que comprar.
2. [Esquema de conexões](03-wiring.md) — como conectar.
3. [Início do firmware](04-firmware-start.md) — estrutura base em `idryer-core`, vinculação ao portal.
4. [Sensor e telemetria](05-sensor-and-telemetry.md) — lemos VOC e enviamos para a nuvem.
5. [Cartão do dispositivo](06-card.md) — declaramos sensores e controle, obtemos a interface.
6. [Lógica de automação](07-auto-logic.md) — limiar, histerese, modo manual do portal.
7. [Montagem e verificação](08-assembly-and-check.md) — checklist final.
