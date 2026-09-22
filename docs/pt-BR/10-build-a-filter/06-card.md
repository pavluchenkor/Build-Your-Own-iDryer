---
title: "Filtro inteligente: cartão no portal (manifesto do cartão)"
description: "Cartão dinâmico do dispositivo: declaramos sensor VOC, modo, limiar e layout através de link.card() — portal e aplicativo constroem a interface automaticamente."
---

# Cartão do dispositivo

Este é o capítulo-chave da seção. Aqui o dispositivo recebe uma interface no portal e no aplicativo móvel — **sem uma única linha de código de suas partes**.

## Como funciona

O dispositivo publica um **manifesto do cartão** — uma descrição legível por máquina "o que mostrar e como controlar". O portal e aplicativo leem o manifesto e constroem o cartão: sensores viram células com valores ao vivo, controles viram botões, campos de entrada e listas. Você também pode especificar o layout do manifesto.

Você não precisa publicar nada manualmente: você declara entidades através de `link.card()`, e o núcleo coleta automaticamente o manifesto e o envia ao conectar.

## 1. Declaramos as entidades

Todas as declarações são feitas em `setup()`, após `s_link.begin()`. Nosso filtro tem três entidades: leitura VOC, lista de modos e campo de limiar. Vamos analisar cada um separadamente e depois montar o bloco inteiro.

### Princípio geral: id e label

Cada entidade tem dois nomes, não os confunda:

- **id** — interno, nome máquina (`"voc"`, `"mode"`). Letras latinas, dígitos, sublinhado, sem espaços. Pelo id a entidade é reconhecida pelo layout, comandos e portal mutuamente. Invente uma vez — não mude;
- **label** — legenda para humanos (`"VOC index"`, `"Mode"`). O que você escrever, o usuário verá no cartão. Pode mudar livremente.

### Sensor: leitura VOC

```cpp
s_link.card().sensor(
    "voc",              // id: nome interno da entidade
    "VOC index",        // label: legenda no cartão
    "",                 // unit: unidade à direita do número ("°C", "%", "g");
                        //   para índice VOC não há unidades — string vazia
    "units[0].vocIndex" // path: de onde pegar o valor — caminho dentro do JSON de telemetria.
                        //   Este é O MESMO campo que adicionamos no capítulo 5:
                        //   doc["units"][0]["vocIndex"]. Os nomes devem coincidir
                        //   letra por letra, senão será traço no cartão.
);
```

Sensor — é uma célula "somente leitura": o portal pega o valor de telemetria pelo `path` e mostra. Sensor não tem comando.

### Lista de escolha: modo de operação

```cpp
// Variantes da lista. O usuário as verá no menu suspenso como estão.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: nome interno da entidade
    "Mode",                  // label: legenda no cartão
    kModes,                  // options: vetor de variantes (declarado acima)
    3,                       // número de variantes no vetor — auto, on, off = três.
                             //   C++ não sabe o tamanho do vetor por si, o informamos nós
    [](const char* opt) {    // callback: função que o núcleo chamará quando
                             //   o usuário selecionar uma variante no portal.
                             //   opt — a string escolhida, por exemplo "on"
        onModeSelected(opt); //   passamos para nossa lógica (escreveremos no capítulo 7)
    }
);
```

Aqui aparece a segunda metade do mecanismo: **controle**. Quando o usuário seleciona uma variante no portal, chega um comando ao dispositivo. O núcleo o recebe sozinho, verifica (strings estranhas que não estão em `options` não chegam a você) e chama seu callback com o valor escolhido. Não é necessário analisar mensagens MQTT manualmente — sua zona de responsabilidade começa dentro de `onModeSelected`.

### Campo numérico: limiar de ativação

```cpp
s_link.card().number(
    "threshold",       // id: nome interno da entidade
    "VOC threshold",   // label: legenda no cartão
    100,               // min: o portal não deixa digitar menos que isto
    400,               // max: mais que isto também não; o núcleo adicionalmente
                       //   cortará o valor por estas bordas em sua parte
    10,                // step: passo de mudança pelo valor das setas
    "",                // unit: unidade de medida; para índice não há
    [](float v) {              // callback: chamado quando o usuário enviou
                               //   um novo valor; v — número nos limites min..max
        onThresholdChanged(v); //   passamos para nossa lógica (escreveremos no capítulo 7)
    }
);
```

### Montamos tudo junto

Vista final do bloco em `setup()` — o que deve ficar em seu código. As funções `onModeSelected` e `onThresholdChanged` escreveremos no capítulo 7; para o código compilar agora, declare-as como esboços **acima** de `setup()`:

```cpp
// Esboços: os corpos reais escreveremos no capítulo 7 (lógica de automação).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Dispositivo desvinculado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetria: seu campo vocIndex (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Cartão: sensor + dois órgãos de controle.
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });
}
```

E o ventilador? Você **não** precisa declará-lo: a flag `hasFan = true` em `Config` já adicionou a célula "Ventilador" ao manifesto automaticamente — é uma habilidade do dicionário, o núcleo sabe tudo sobre ele.

!!! note "Colchetes em callbacks — sempre vazios"
    `[](const char* opt) { ... }` — é uma lambda, função sem nome; analisamos isto em detalhes na [nota do capítulo 5](05-sensor-and-telemetry.md). Lembremos da regra do núcleo: os colchetes de captura são sempre vazios (`[]`), não levamos nada "junto" para a lambda, tudo necessário armazenamos em variáveis globais — como `g_mode` e `g_threshold` do próximo capítulo.

## 2. Layout automático do cartão

Você pode não especificar layout nenhum. O portal coleta o cartão a partir das entidades declaradas — e coleta com esmero: células de leitura agrupam em linhas (até três em uma linha, depois quebra), os controles ficam abaixo, cada um em sua linha, tudo em design oficial do portal. Para maioria dos dispositivos isto basta — a interface fica impecável sem uma única reflexão sobre diagramação.

A ordem das entidades no cartão — ordem de sua declaração em `setup()`.

## 3. Layout customizado do cartão (opcional)

Primeiro — como o cartão é organizado. O cartão é uma pilha vertical de **linhas**. Uma linha de células de sensores divide a largura igualmente: uma célula ocupa a largura toda, duas — metade cada, três — um terço cada. Os controles (lista, campo, botão) de uma linha ficam um embaixo do outro, na largura toda.

O layout automático da seção anterior distribui entidades por estas linhas sozinho. Se você quiser decidir sozinho o que fica junto — especifique as linhas manualmente por chamadas `layoutRow`. Uma chamada = uma linha, ordem de chamadas = ordem das linhas de cima para baixo:

```cpp
// Linha 1: duas células — índice VOC e ventilador, cada uma metade da largura.
s_link.card().layoutRow("voc", "fan");

// Linha 2: dois controles — modo e limite, um embaixo do outro.
s_link.card().layoutRow("mode", "threshold");
```

Em `layoutRow` você passa os **ids** das entidades — aqueles nomes internos que deu a elas ao declarar (é para isto que id era necessário). `"fan"` — id da entidade do ventilador do dicionário, criada pela flag `hasFan`.

No cartão isto resultará em tal composição:

```text
┌─ DIY Air Filter ────────────────┐
│  VOC index        │  Ventilador │   ← linha 1: voc, fan
│  103              │  Deslig      │
├───────────────────┴─────────────┤
│  Mode                           │   ← linha 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [150                         ] │
└─────────────────────────────────┘
```

Entidades que você não mencionar em linha nenhuma não desaparecem — o portal as desenha abaixo automaticamente. Assim você pode delinear apenas o "principal", e deixar o resto para automação.

## 4. O que vai para o ar

O núcleo publica no tópico `idryer/{serial}/card` (retained):

```json
{
  "v": 1,
  "entities": [
    { "id": "fan",  "type": "binary_sensor", "device_class": "fan",
      "source": "telemetry", "path": "units[0].fanStatus" },
    { "id": "voc",  "type": "sensor", "label": "VOC index",
      "source": "telemetry", "path": "units[0].vocIndex" },
    { "id": "mode", "type": "select", "label": "Mode",
      "options": ["auto", "on", "off"], "action": "card.mode", "arg": "value" },
    { "id": "threshold", "type": "number", "label": "VOC threshold",
      "min": 100, "max": 400, "step": 10, "action": "card.threshold", "arg": "value" }
  ],
  "layout": [ ["voc", "fan"], ["mode", "threshold"] ]
}
```

Não é necessário entender este JSON — o núcleo o gera de suas chamadas. Mas é útil saber dele: se você escrever firmware **não** em `idryer-core` (Rust, MicroPython, qualquer coisa), é suficiente publicar tal JSON você mesmo — o portal é onívoro, basta que o formato coincida.

## 5. Verificação

Grave o firmware e abra o dashboard do portal:

- célula **VOC index** mostra índice ao vivo (sopre no sensor — o número cresce na próxima atualização);
- célula **Ventilador** — Lig/Deslig;
- **Mode** — lista suspensa, **VOC threshold** — campo numérico: o valor é enviado cerca de 0,6 s depois da mudança, sem botão de confirmação. A lista e o campo começam na primeira opção e no mínimo — eles enviam comandos e não mostram a configuração atual do dispositivo; o estado aparece nas células (por exemplo, o ventilador).

Seleção de modo e limiar ainda não fazem nada — callbacks são esboços. Vamos trazê-los à vida no [próximo capítulo](07-auto-logic.md).

!!! note "Isto é o conceito na prática"
    Veja o que aconteceu: você descreveu a interface com cinco linhas no firmware — e ela apareceu no portal e aplicativo. O mesmo truque funciona para qualquer dispositivo seu: mudam apenas ids, legendas e callbacks.

!!! note "Operações com partida e parada"
    A lista, o campo e o botão enviam o valor na hora. Operações com partida e parada — secagem, aquecimento, iluminação — são declaradas como **ações** do cartão com um modo e parâmetros de partida: o cartão mostra um formulário de partida e, enquanto a operação roda, um bloco de sessão com o botão de parada. Exemplo — o [capítulo 7](../09-build-a-device/07-heating-control.md) da seção do gabinete.
