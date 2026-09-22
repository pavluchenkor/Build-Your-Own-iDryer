---
title: "Construir o seu próprio dispositivo com núcleo iDryer: conceito"
description: "Exemplo de ponta a ponta: como construir um armário de armazenamento de filamento aquecido do zero em ESP32 e biblioteca idryer-core com ligação ao portal iDryer."
---

# Construir o seu próprio dispositivo: conceito

Esta secção é um exemplo de ponta a ponta. As secções anteriores explicavam os blocos individuais: alimentação, controladores, sensores, aquecedores, segurança. Aqui você monta esses blocos numa única unidade completa e a leva até ao estado funcional com ligação ao [portal iDryer](https://portal.idryer.org/).

O exemplo é construído sobre a biblioteca `idryer-core`. A biblioteca trata de toda a integração de rede: ligação ao Wi-Fi, vinculação de contas, sessão MQTT segura, publicação periódica de telemetria. Você apenas escreve o que é específico do seu dispositivo: leitura de sensores, controlo do aquecedor e ventoinha, lógica de manutenção de temperatura.

## O que exatamente estamos a construir

Estamos a construir um **armário de armazenamento de filamento aquecido**. É um armário fechado com capacidade para 10-40 carretilhas, onde a temperatura é mantida em torno de `40-45 °C`.

É importante estabelecer os limites da tarefa desde o início.

!!! note "Este não é um secador de alta temperatura"
    Não pretendemos fazer secagem rápida em alta temperatura. O objetivo do dispositivo é manter um calor suave no armário que mantenha o filamento seco durante o armazenamento.

A temperatura `40-45 °C` é suficiente para armazenar a maioria dos plásticos não exigentes - de PLA a ABS - num estado seco. Para secagem ativa de materiais exigentes (nylon, policarbonato, PA-CF), são necessárias temperaturas mais elevadas e uma construção diferente - esses secadores são montados separadamente, seguindo os princípios das outras secções.

## Por que fazer isto você mesmo

O controlador iDryer completo já sabe fazer tudo o que está descrito abaixo. Este exemplo não é para substituí-lo, mas para mostrar **como o dispositivo é construído internamente** e para fornecer uma base para os seus próprios módulos.

A montagem independente faz sentido quando:

- precisa de um armário de tamanho ou forma não padrão;
- quer compreender como o controlador gere o aquecimento e comunica com o portal;
- planeia criar o seu próprio módulo do ecossistema e está a usar este exemplo como ponto de partida.

## Como isto é diferente do controlador V2

O controlador iDryer V2 de série é dual-processador: a lógica principal é executada num microcontrolador separado, e o módulo ESP32 funciona apenas como ponte para o Wi-Fi e portal. Isto é justificável para um produto comercial com ecrã, balanças, RFID e várias câmaras.

Para um armário feito em casa, esta complexidade não é necessária. Simplificamos a arquitectura para **um único ESP32**, que faz tudo por si:

- lê sensores;
- controla o aquecedor e a ventoinha;
- liga-se ao Wi-Fi e portal via `idryer-core`.

Funcionalmente, repetimos o comportamento de uma câmara do controlador V2 (sensor climático, aquecedor com feedback de termistor, ventoinha), mas numa verdadeira versão DIY numa única placa.

!!! note "Servo não é utilizado"
    No controlador V2, o servo controla a abas de ar da câmara. Para um armário de armazenamento com aquecimento suave e uniforme, a aba não é necessária, portanto neste exemplo não há servo.

## O que dá a ligação ao núcleo

Quando o dispositivo é construído em `idryer-core` e vinculado à sua conta, obtém sem código adicional:

- gestão e monitorização através do [portal](https://portal.idryer.org/) e aplicação móvel;
- gráfico de temperatura e humidade no armário;
- início e paragem remota do modo de manutenção de calor;
- configuração de parâmetros (temperatura-alvo, histerese) através do menu do dispositivo.

!!! note "Os seus próprios sensores e botões no cartão"
    O cartão do dispositivo não se limita às capacidades declaradas em `has*`. Através do card-manifest, o firmware pode declarar qualquer sensor ou controlo próprio — eles aparecem automaticamente no portal e na aplicação. Como fazê-lo — exemplo completo [«Filtro de ar inteligente»](../10-build-a-filter/01-concept.md), em especial [o capítulo sobre o cartão](../10-build-a-filter/06-card.md). O arranque e a paragem da manutenção de calor declaram-se como ações do cartão no [capítulo 7](07-heating-control.md).

## Que partes tem esta secção

A seguir é o caminho passo a passo desde uma placa vazia até um armário funcional:

1. [Composição do sistema](02-bom.md) - que componentes usar e duas versões da secção de potência (baixa tensão e AC).
2. [Esquema de ligação](03-wiring.md) - mapa dos pinos de ESP32, isolamento entre secções de baixa e alta potência, segurança.
3. [Arranque de firmware no núcleo](04-firmware-start.md) - projecto PlatformIO, primeira execução, vinculação ao portal.
4. [Sensores](05-sensors.md) - ligação de SHT31 e termistor, obtenção de dados.
5. [Menu de YAML](06-menu.md) - descrever as configurações do dispositivo, armazenar em NVS e no portal.
6. [Controlo de aquecimento](07-heating-control.md) - lógica de manutenção de temperatura, ventoinha, arranque e paragem a partir do cartão do dispositivo.
7. [Montagem e verificação](08-assembly-and-check.md) - montagem final, primeiro aquecimento, lista de verificação de segurança.

!!! tip "Exemplo pronto"
    Se quiser ver o resultado imediatamente - o projecto completo está na pasta `example/09-cabinet/` do repositório e é compilado com `pio run -e cabinet`. Os capítulos abaixo analisam este código em passos.

## Ver também

- [Por onde começar](../00-start-here/01-introduction.md) - ordem geral de leitura da secção.
- [Controlador ESP32](../02-controllers/01-esp32-controller.md) - por que ESP32 é conveniente para um dispositivo com Wi-Fi.
- [Componentes comuns](../03-common-components/01-overview.md) - mapa de peças do dispositivo.
