---
title: "Filtro inteligente: montagem e verificação final"
description: "Montagem do filtro no gabinete, ordem das camadas de filtração e checklist completo: sensor, telemetria, cartão, comandos, automação."
---

# Montagem e verificação

## Montagem

1. **Gabinete.** Caixa com duas aberturas: entrada de ar e saída. Imprima para o seu cartucho de filtro ou adapte uma caixa pronta.
2. **Camadas pelo fluxo de ar:** entrada → HEPA → carvão → ventilador (em exaustão) → saída. Junções — sem frestas: o ar é preguiçoso e vai pelo desvio se puder.
3. **Sensor** — no fluxo de entrada, antes dos filtros: deve cheirar ar sujo da sala, não purificado.
4. **Eletrônica** — em compartimento separado ou na parede, longe do fluxo de pó. Placa — em espaçadores, não "tudo junto".
5. Fixe os fios: a vibração do ventilador com o tempo desaperta tudo que não está preso.

## Checklist completo

Verifique em ordem — cada ponto depende dos anteriores.

| # | Verificação | Como |
|---|---|---|
| 1 | Alimentação | 12 V na linha do ventilador, 5 V após buck, 3,3 V no sensor |
| 2 | Sensor vivo | no log serial índice ~100 em ar limpo, cresce de respiro |
| 3 | Dispositivo Online | status no portal após a vinculação no app |
| 4 | Telemetria | `vocIndex` e `fanStatus` no fluxo do dispositivo |
| 5 | Cartão | células VOC e Ventilador, lista Mode, campo Threshold |
| 6 | Comando do portal | Mode → `on`: ventilador ligou, cartão mostrou "Lig" |
| 7 | Automação | Mode → `auto`, respirar para sensor: ligou no limiar, desligou abaixo |
| 8 | Reinicialização | modo e limiar foram salvos, cartão reviveu sozinho |

## E adiante?

Filtro pronto. Adiante — por gosto:

- **Mais entidades**: botão "soprar 5 minutos" (`card().button(...)`), segundo sensor, contador de horas do filtro com lembrete de troca;
- **Layout bonito**: você já viu o `layoutRow` de fábrica;
- **Seus dispositivos**: toda esta seção — é um template. Mude o sensor, mecanismo de acionamento e lógica — e pelo mesmo esquema monte umidificador, exaustor, controlador de qualquer coisa. O manifesto fará a interface sozinho.

Se algo não funciona — [Erros típicos](../08-common-mistakes/01-overview.md).
