---
title: "Construindo seu próprio dispositivo no núcleo iDryer: conceito"
description: "Exemplo completo: como montar do zero um gabinete aquecido para armazenamento de filamento em ESP32 e na biblioteca idryer-core com conexão ao portal iDryer."
---

# Construindo seu próprio dispositivo: conceito

Esta seção é um exemplo completo. As seções anteriores explicavam componentes individuais: fonte de energia, controladores, sensores, aquecedores, segurança. Aqui você monta esses componentes em um único dispositivo funcional e o coloca em funcionamento com conexão ao [portal iDryer](https://portal.idryer.org/).

O exemplo é baseado na biblioteca `idryer-core`. A biblioteca cuida de toda a integração de rede: conexão Wi-Fi, associação de conta, sessão MQTT segura, publicação periódica de telemetria. Você escreve apenas o que é específico do seu dispositivo: leitura de sensores, controle de aquecedor e ventilador, lógica de manutenção de temperatura.

## O que exatamente estamos construindo

Estamos construindo um **gabinete aquecido para armazenamento de filamento**. É um gabinete fechado para 10-40 carretilhas, onde a temperatura é mantida em torno de `40-45 °C`.

É importante definir claramente os limites do projeto desde o início.

!!! note "Isto não é uma secadora de alta temperatura"
    Não pretendemos fazer secagem rápida em alta temperatura. O objetivo do dispositivo é manter calor suave no gabinete, o que mantém o filamento seco durante o armazenamento.

A temperatura de `40-45 °C` é suficiente para manter a maioria dos plásticos não exigentes - do PLA ao ABS - em estado seco. Para secagem ativa de materiais exigentes (nylon, policarbonato, PA-CF), são necessárias temperaturas mais altas e uma construção diferente - essas secadoras são montadas separadamente, de acordo com os princípios das outras seções.

## Por que fazer isso sozinho

O controlador iDryer pronto já sabe fazer tudo o que está descrito abaixo. Este exemplo é necessário não como substituto, mas para mostrar **como o dispositivo funciona internamente** e fornecer base para seus próprios módulos.

A montagem independente faz sentido quando:

- você precisa de um gabinete de tamanho ou forma não padrão;
- você quer entender como o controlador gerencia aquecimento e se comunica com o portal;
- você planeja fazer seu próprio módulo de ecossistema e está usando este exemplo como ponto de partida.

## Como isso difere do controlador V2

O controlador de série iDryer V2 é de dois processadores: a lógica principal é executada em um microcontrolador separado, e o módulo ESP32 atua apenas como um bridge para Wi-Fi e portal. Isto é justificado para um produto pronto com tela, balança, RFID e várias câmeras.

Para um gabinete caseiro, essa complexidade não é necessária. Simplificamos a arquitetura para um **único ESP32**, que faz tudo sozinho:

- lê os sensores;
- controla o aquecedor e ventilador;
- conecta-se ao Wi-Fi e portal via `idryer-core`.

Funcionalmente, repetimos o comportamento de uma câmera do controlador V2 (sensor de clima, aquecedor com feedback de termistor, ventilador), mas em uma implementação DIY honesta em uma única placa.

!!! note "Servo não é usado"
    No controlador V2, o servo controla a damper de ar da câmera. Para um gabinete de armazenamento com aquecimento suave e uniforme, a damper não é necessária, então este exemplo não inclui servo.

## O que oferece a conexão ao núcleo

Quando o dispositivo é montado em `idryer-core` e vinculado à conta, você obtém sem código adicional:

- gerenciamento e monitoramento via [portal](https://portal.idryer.org/) e aplicativo móvel;
- gráfico de temperatura e umidade no gabinete;
- iniciar e parar o modo de manutenção de calor remotamente;
- configurar parâmetros (temperatura-alvo, histerese) por meio do menu do dispositivo.

!!! note "Os seus próprios sensores e botões no cartão"
    O cartão do dispositivo não se limita às capacidades declaradas em `has*`. Através do card-manifest, o firmware pode declarar qualquer sensor ou controle próprio — eles aparecem automaticamente no portal e no aplicativo. Como fazer isso — exemplo completo [«Filtro de ar inteligente»](../10-build-a-filter/01-concept.md), em especial [o capítulo sobre o cartão](../10-build-a-filter/06-card.md). A partida e a parada da manutenção de calor são declaradas como ações do cartão no [capítulo 7](07-heating-control.md).

## Do que consiste esta seção

A seguir está o caminho passo a passo de uma placa vazia para um gabinete funcional:

1. [Composição do sistema](02-bom.md) — quais componentes tomar e duas versões da parte de potência (baixa voltagem e rede).
2. [Esquema de conexão](03-wiring.md) — mapa de pinos ESP32, isolamento entre a parte de sinal fraco e de potência, segurança.
3. [Início da firmware no núcleo](04-firmware-start.md) — projeto PlatformIO, primeiro lançamento, vinculação ao portal.
4. [Sensores](05-sensors.md) — conectamos SHT31 e termistor, obtemos dados deles.
5. [Menu em YAML](06-menu.md) — descrevemos configurações do dispositivo, elas vão para NVS e portal.
6. [Controle de aquecimento](07-heating-control.md) — lógica de manutenção de temperatura, ventilador, partida e parada pelo cartão do dispositivo.
7. [Montagem e verificação](08-assembly-and-check.md) — montagem final, primeiro aquecimento, lista de verificação de segurança.

!!! tip "Exemplo pronto"
    Se você quiser ver o resultado imediatamente — o projeto completo está na pasta `example/09-cabinet/` do repositório e é montado com o comando `pio run -e cabinet`. Os capítulos abaixo decompõem esse mesmo código passo a passo.

## Veja também

- [Por onde começar](../00-start-here/01-introduction.md) — ordem geral de leitura da seção.
- [Controlador ESP32](../02-controllers/01-esp32-controller.md) — por que ESP32 é conveniente para um dispositivo com Wi-Fi.
- [Componentes comuns](../03-common-components/01-overview.md) — mapa de peças do dispositivo.
