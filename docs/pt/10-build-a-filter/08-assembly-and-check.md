---
title: "Filtro inteligente: montagem e verificação final"
description: "Montagem do filtro em caixa, ordem das camadas filtrantes e checklist completo: sensor, telemetria, cartão, comandos, automatização."
---

# Montagem e verificação

## Montagem

1. **Caixa.** Caixa com duas aberturas: entrada de ar e saída. Imprima para o seu cartucho de filtro ou adapte uma existente.
2. **Camadas pelo fluxo de ar:** entrada → HEPA → carvão → ventilador (em modo sopro) → saída. Costuras — sem fendas: o ar é preguiçoso e irá contornar o filtro se conseguir.
3. **Sensor** — no fluxo de entrada, antes dos filtros: deve sentir o ar sujo da sala, não o purificado.
4. **Eletrónica** — em compartimento separado ou na parede, longe do fluxo de pó. Placa — em espaçadores, não «em montão».
5. Prenda os cabos: a vibração do ventilador com o tempo afroxa tudo o que não está preso.

## Checklist completo

Verifique por ordem — cada ponto baseia-se nos anteriores.

| # | Verificação | Como |
|---|---|---|
| 1 | Alimentação | 12 V na linha do ventilador, 5 V após buck, 3.3 V no sensor |
| 2 | Sensor vivo | no log Serial índice ~100 em ar limpo, cresce pelo sopro |
| 3 | Dispositivo Online | estado no portal após a vinculação na aplicação |
| 4 | Telemetria | `vocIndex` e `fanStatus` no fluxo do dispositivo |
| 5 | Cartão | células VOC e Ventilador, lista Mode, campo Threshold |
| 6 | Comando do portal | Mode → `on`: ventilador ligou, cartão mostra «Lig» |
| 7 | Automatização | Mode → `auto`, respirar: ligou no limiar, desligou abaixo |
| 8 | Reinicialização | modo e limiar guardaram-se, cartão acordou sozinho |

## Próximos passos

Filtro pronto. Depois — a seu gosto:

- **Mais entidades**: botão «purga 5 minutos» (`card().button(...)`), segundo sensor, contador de horas do filtro com lembrete de troca;
- **Layout bonito**: o `layoutRow` de fábrica que viu já;
- **Os seus dispositivos**: toda esta secção — um template. Substitua sensor, mecanismo de accionamento e lógica — e pelo mesmo esquema construa humidificador, exaustor, controlador de qualquer coisa. O manifesto fará a interface por si.

Se algo não fica operacional — [Erros Comuns](../08-common-mistakes/01-power-mistakes.md).
