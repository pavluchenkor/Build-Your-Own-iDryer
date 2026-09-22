---
title: "Filtro inteligente: cartão no portal (manifesto de cartão)"
description: "Cartão dinâmico do dispositivo: declaramos sensor VOC, modo, limiar e layout através de link.card() — portal e aplicação constroem a interface automaticamente."
---

# Cartão do dispositivo

Este é o capítulo principal da secção. Aqui o dispositivo obtém a interface no portal e na aplicação móvel — **sem uma única linha de código neles**.

## Como funciona

O dispositivo publica um **manifesto de cartão** — descrição legível por máquina «o que mostrar e com o que controlar». Portal e aplicação leem o manifesto e constroem o cartão: sensores tornam-se células com valores ao vivo, controlos — botões, campos de entrada e listas. O layout também pode ser definido na firmware.

Você não precisa publicar nada manualmente: você declara entidades através de `link.card()`, e o núcleo recolhe o manifesto e envia-o automaticamente na ligação.

## 1. Declaramos entidades

Todas as declarações são feitas em `setup()`, após `s_link.begin()`. Nosso filtro tem três entidades: leitura VOC, lista de modos e campo de limiar. Vamos analisar cada uma separadamente, e no final reunir o bloco inteiro.

### Princípio geral: id e label

Cada entidade tem dois nomes, não confunda:

- **id** — interno, nome máquina (`"voc"`, `"mode"`). Caracteres latinos, dígitos, underscore, sem espaços. Pelo id a entidade é reconhecida no layout, comandos e portal entre si. Escolhido uma vez — não muda;
- **label** — inscrição para humanos (`"VOC index"`, `"Mode"`). O que escrever é o que o utilizador vê no cartão. Pode mudar livremente.

### Sensor: leitura VOC

```cpp
s_link.card().sensor(
    "voc",              // id: nome interno da entidade
    "VOC index",        // label: inscrição no cartão
    "",                 // unit: unidade de medida à direita do número ("°C", "%", "g");
                        //   o índice VOC não tem unidades — string vazia
    "units[0].vocIndex" // path: onde obter o valor — caminho dentro do JSON de telemetria.
                        //   Este é o CAMPO que adicionámos no capítulo 5:
                        //   doc["units"][0]["vocIndex"]. Os nomes devem coincidir
                        //   letra a letra, senão no cartão será um traço.
);
```

Sensor — é uma célula «apenas leitura»: o portal pega o valor de telemetria por `path` e mostra. Sensor não tem comando.

### Lista de escolha: modo de funcionamento

```cpp
// Variantes da lista. O utilizador vê-los-á no menu dropdown como estão.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: nome interno da entidade
    "Mode",                  // label: inscrição no cartão
    kModes,                  // options: array de variantes (declarado acima)
    3,                       // quantidade de variantes no array — auto, on, off = três.
                             //   C++ não conhece o comprimento sozinho, nós informamos
    [](const char* opt) {    // callback: função que o núcleo chamará quando
                             //   o utilizador seleccionar uma variante no portal.
                             //   opt — string seleccionada, por exemplo "on"
        onModeSelected(opt); //   passamos à nossa lógica (escreveremos no capítulo 7)
    }
);
```

Aqui aparece a segunda metade do mecanismo: **controlo**. Quando o utilizador escolhe uma variante no portal, o dispositivo recebe um comando, o núcleo recebe-o automaticamente, verifica (strings estranhas que não estão em `options` não passam) e chama o seu callback com o valor escolhido. Não precisa de desempacotar mensagens MQTT manualmente — a sua zona de responsabilidade começa dentro de `onModeSelected`.

### Campo numérico: limiar de activação

```cpp
s_link.card().number(
    "threshold",       // id: nome interno da entidade
    "VOC threshold",   // label: inscrição no cartão
    100,               // min: o portal não deixará escrever menos
    400,               // max: nem mais; o núcleo além disso
                       //   corta o valor dentro destes limites no seu lado
    10,                // step: incremento de mudança com setas do campo
    "",                // unit: unidade de medida; o índice não tem
    [](float v) {              // callback: chamado quando o utilizador envia
                               //   novo valor; v — número dentro de min..max
        onThresholdChanged(v); //   passamos à nossa lógica (capítulo 7)
    }
);
```

### Reunimos tudo junto

O aspecto final do bloco em `setup()` — é o que deve ficar no seu código. As funções `onModeSelected` e `onThresholdChanged` escreveremos no capítulo 7; para o código compilar já agora, declare-as como stubs **acima** de `setup()`:

```cpp
// Stubs: corpos verdadeiros escreveremos no capítulo 7 (lógica de automatização).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetria: campo próprio vocIndex (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Cartão: sensor + dois órgãos de controlo.
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

E o ventilador? Não precisa declarar: a flag `hasFan = true` em `Config` já adicionou a célula «Ventilador» ao manifesto automaticamente — é uma capacidade de dicionário, o núcleo conhece tudo sobre ela.

!!! note "Parênteses rectos em callbacks — sempre vazios"
    `[](const char* opt) { ... }` — é uma lambda, função sem nome; analisámos em detalhe na [nota do capítulo 5](05-sensor-and-telemetry.md). Lembrete da regra do núcleo: parênteses sempre vazios (`[]`), não levamos nada «consigo» para a lambda, guardamos o necessário em variáveis globais — como `g_mode` e `g_threshold` do próximo capítulo.

## 2. Layout automático do cartão

Você pode não definir layout. O portal construirá o cartão a partir das entidades declaradas — e construirá com cuidado: células de leitura agrupam-se em linhas (até três por linha, depois quebra), órgãos de controlo ficam abaixo, cada um em sua linha, tudo em estilo do portal. Para a maioria dos dispositivos isto é suficiente — a interface fica organizada sem uma única preocupação com layout.

A ordem das entidades no cartão — a ordem de sua declaração em `setup()`.

## 3. Layout próprio do cartão (opcional)

Primeiro — como o cartão é organizado. O cartão é uma pilha vertical de **linhas**. Uma linha de células de sensores divide a largura em partes iguais: uma célula ocupa toda a largura, duas — metade cada, três — um terço cada. Os controlos (lista, campo, botão) de uma linha ficam uns por baixo dos outros, a toda a largura.

O layout automático da secção anterior distribui entidades por estas linhas sozinho. Se quiser decidir você o que fica junto com o quê — defina linhas manualmente com chamadas de `layoutRow`. Uma chamada = uma linha, ordem de chamadas = ordem de linhas de cima para baixo:

```cpp
// Linha 1: duas células — índice VOC e ventilador, cada metade da largura.
s_link.card().layoutRow("voc", "fan");

// Linha 2: dois controlos — modo e limiar, um por baixo do outro.
s_link.card().layoutRow("mode", "threshold");
```

Para `layoutRow` passam-se **id** das entidades — aqueles nomes internos que deu na declaração (foi para isto que id era necessário). `"fan"` — id da entidade de dicionário do ventilador, criada pela flag `hasFan`.

No cartão isto dará a seguinte composição:

```text
┌─ DIY Air Filter ────────────────┐
│  VOC index        │  Ventilador │   ← linha 1: voc, fan
│  103              │  Deslig     │
├───────────────────┴─────────────┤
│  Mode                           │   ← linha 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [150                         ] │
└─────────────────────────────────┘
```

Entidades não mencionadas em nenhuma linha não desaparecem — o portal desenha-as abaixo automaticamente. Assim pode definir apenas o «principal», deixando o resto para a automatização.

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

Não é necessário entender este JSON — o núcleo gera-o a partir das suas chamadas. Mas é útil saber: se escrever firmware **não** em `idryer-core` (Rust, MicroPython, o que quer que seja), basta publicar este JSON por si próprio — o portal é omnívoro, desde que o formato coincida.

## 5. Verificação

Grave o firmware e abra o dashboard do portal:

- célula **VOC index** mostra o índice ao vivo (respire sobre o sensor — o número cresce na próxima atualização);
- célula **Ventilador** — Lig/Deslig;
- **Mode** — lista pendente, **VOC threshold** — campo numérico: o valor é enviado cerca de 0,6 s depois da alteração, sem botão de confirmação. A lista e o campo começam na primeira opção e no mínimo — enviam comandos e não mostram a definição atual do dispositivo; o estado é mostrado pelas células (por exemplo, a ventoinha).

A escolha de modo e limiar ainda não faz nada — callbacks stubs. Vamos animá-los no [próximo capítulo](07-auto-logic.md).

!!! note "Este é o conceito em si"
    Veja o que aconteceu: descreveu a interface em cinco linhas na firmware — e ela apareceu no portal e na aplicação. A mesma técnica funciona para qualquer dispositivo seu: mudam apenas id, inscrições e callbacks.

!!! note "Operações com arranque e paragem"
    A lista, o campo e o botão enviam um valor de imediato. Operações com arranque e paragem — secagem, aquecimento, iluminação — declaram-se como **ações** do cartão com um modo e parâmetros de arranque: o cartão mostra um formulário de arranque e, enquanto a operação decorre, um bloco de sessão com o botão de paragem. Exemplo — o [capítulo 7](../09-build-a-device/07-heating-control.md) da secção do armário.
