---
title: "Filtro de ar inteligente: conceito e benefícios no portal"
description: "Exemplo completo nº 2: filtro de ar para área de impressão 3D em ESP32 e idryer-core — sensor VOC próprio, controlo próprio e cartão automático no portal através do manifesto de cartão."
---

# Filtro de ar inteligente: conceito

Este é o segundo exemplo completo da secção «construa você mesmo». No [primeiro exemplo](../09-build-a-device/01-concept.md) você construiu um armário aquecido a partir dos «blocos de construção» do ecossistema: temperatura, humidade, aquecedor. Aqui damos um passo em frente — construímos um dispositivo **que não existe no ecossistema iDryer**: um filtro de ar com sensor de compostos orgânicos voláteis (VOC).

## Ideia principal: este é um conceito, não uma receita para um único dispositivo

Leia este parágrafo com atenção — é mais importante do que todo o resto do capítulo.

O filtro é apenas um exemplo. A abordagem apresentada funciona para **qualquer dispositivo que você imaginar**: humidificador, estação de sopro, controlador de exaustão, monitor de armazém de filamento, o que quiser. Você declara na firmware quais sensores e órgãos de controlo o dispositivo possui — com uma ou duas linhas de código para cada um — e o dispositivo **aparece automaticamente no portal e na aplicação móvel** com um cartão pronto: leituras ao vivo, botões, campos de entrada. Nem uma linha de código no portal, nem acordos com o pessoal do iDryer, nem pull-requests.

Isto funciona graças ao mecanismo de **cartões dinâmicos** (manifesto de entidade): o dispositivo publica uma descrição legível por máquina «o que mostrar e com o que controlar», e o portal e a aplicação constroem a interface a partir dessa descrição. Como isto fica no código — [capítulo sobre o cartão](06-card.md).

!!! note "O que isto significa na prática"
    Pensou num dispositivo → construiu em ESP32 → descreveu sensores e botões na firmware → associou à conta na aplicação. Pronto: o dispositivo tem interface no portal e na aplicação. A distância da ideia até «controlo do smartphone» — uma noite.

## O que exatamente estamos a construir

**Filtro de ar para área de impressão 3D**: uma caixa com ventilador, filtro HEPA e camada de carvão ativado, que:

- mede a qualidade do ar com sensor VOC (SGP40);
- liga o ventilador automaticamente quando o ar está sujo e desliga quando se limpa;
- mostra no portal o índice VOC e o estado do ventilador;
- permite escolher o modo a partir do portal (`auto` / `on` / `off`) e configurar o limiar de activação.

ABS e ASA emitem estireno durante a impressão, resinas seu próprio aroma. Um filtro na impressora não é luxo, é higiene.

## Porque é este o projeto inicial perfeito

Se o armário da secção 09 lhe pareceu complexo — comece pelo filtro:

- **sem aquecedor** — portanto sem parte de potência, fusíveis térmicos e riscos;
- componentes mínimos: placa, sensor, ventilador, transístor;
- orçamento cerca de `$15` sem caixa;
- com qualquer erro no código, o pior que pode acontecer é o ventilador não ligar.

## Limites do projecto

Vamos definir honestamente o que este filtro **não é**:

- não é um sistema de exaustão: o ar é reciclado através do filtro, não expelido para a rua;
- não é um aparelho médico: SGP40 mostra um **índice** relativo de qualidade do ar, não a concentração de um gás específico em ppm;
- o filtro não substitui a ventilação.

!!! note "VOC ou CO2?"
    Para vapores de impressão, o sensor correcto é VOC: reage a compostos orgânicos (estireno, solventes). Sensores de CO2 (como o sensor NDIR MH-Z19) medem dióxido de carbono — este é um indicador de ar viciado, não de contaminação da impressão. Se quiser ambos, ENS160 fornece índice VOC e estimativa de eCO2 simultaneamente; a abordagem nesta secção não muda — apenas mais uma linha no manifesto de cartão.

## Roteiro da secção

1. [Composição do sistema](02-bom.md) — o que comprar.
2. [Esquema de ligações](03-wiring.md) — como ligar.
3. [Início da firmware](04-firmware-start.md) — estrutura em `idryer-core`, ligação ao portal.
4. [Sensor e telemetria](05-sensor-and-telemetry.md) — lemos VOC e enviamos para a nuvem.
5. [Cartão do dispositivo](06-card.md) — declaramos sensores e controles, obtemos interface.
6. [Lógica de automatização](07-auto-logic.md) — limiar, histerese, modo manual a partir do portal.
7. [Montagem e verificação](08-assembly-and-check.md) — checklist final.
