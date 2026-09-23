---
title: "Chytrý filtr: karta v portálu (card manifest)"
description: "Dynamická karta zařízení: deklarujeme VOC senzor, režim, práh a rozvržení skrz link.card() — portál a aplikace si automaticky staví rozhraní."
---

# Karta zařízení

Toto je klíčová kapitola oddílu. Zde zařízení získá rozhraní v portálu a mobilní aplikaci — **bez jediného řádku kódu na jejich straně**.

## Jak to funguje

Zařízení zveřejní **manifest karty** — strojově čitelný popis „co zobrazit a čím ovládat". Portál a aplikace si podle manifestu postaví kartu: senzory se stanou buňkami s živými hodnotami, ovládací prvky — tlačítky, vstupními poli a seznamy. Rozvržení si taky můžete zadat z programu.

Nemusíte nic publikovat ručně: deklarujete entity přes `link.card()` a jádro samo sestaví manifest a odešle jej při připojení.

## 1. Deklarujeme entity

Všechny deklarace se provádějí v `setup()`, za `s_link.begin()`. Náš filtr má tři entity: čtení VOC, seznam režimů a pole prahu. Rozebereme každou zvlášť a na konci celek složíme dohromady.

### Obecný princip: id a label

Každá entita má dvě jména, nepleťte si je:

- **id** — vnitřní, strojové jméno (`"voc"`, `"mode"`). Latina, číslice, podtržítko, bez mezer. Podle id se entita pozná v rozvržení, příkazech a portálu navzájem. Vymysleli jednou — neměníte;
- **label** — nápis pro člověka (`"VOC index"`, `"Mode"`). Co napíšete, to si uživatel na kartě přečte. Měnit se dá libovolně.

### Senzor: čtení VOC

```cpp
s_link.card().sensor(
    "voc",              // id: vnitřní jméno entity
    "VOC index",        // label: nápis na kartě
    "",                 // unit: jednotka měření vpravo od čísla ("°C", "%", "g");
                        //   VOC index nemá jednotky — prázdný řetězec
    "units[0].vocIndex" // path: odkud vzít hodnotu — cesta v JSON telemetrických datech.
                        //   To je TEN SAMÝ prvek, který jsme doplnili v kapitole 5:
                        //   doc["units"][0]["vocIndex"]. Jména se musí shodovat
                        //   písmeno za písmenem, jinak bude na kartě pomlčka.
);
```

Senzor je dlaždice „pouze pro čtení": portál vezme hodnotu z telemetrie podle `path` a zobrazí ji. Senzor nemá žádný příkaz.

### Seznam výběru: režim práce

```cpp
// Varianty seznamu. Uživatel je uvidí v rozbalovacím menu takové jak jsou.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: vnitřní jméno entity
    "Mode",                  // label: nápis na kartě
    kModes,                  // options: pole variant (deklarováno výše)
    3,                       // počet variant v poli — auto, on, off = tři.
                             //   C++ sám nezná délku pole, řekneme ji my
    [](const char* opt) {    // callback: funkce, kterou jádro volá, když
                             //   uživatel vybere variantu v portálu.
                             //   opt — vybraný řetězec, např. "on"
        onModeSelected(opt); //   předáme ji do naší logiky (napíšeme v kapitole 7)
    }
);
```

Tím se aktivuje druhá polovina mechanismu: **ovládání**. Když uživatel vybere možnost v portálu, zařízení obdrží příkaz, jádro jej samo přijme, ověří (cizí řetězce, které nejsou v `options`, se k vám nedostanou) a zavolá váš callback s vybranou hodnotou. MQTT zprávy rozebírat ručně nemusíte — vaše zodpovědnost začíná až uvnitř `onModeSelected`.

### Číselné pole: práh spuštění

```cpp
s_link.card().number(
    "threshold",       // id: vnitřní jméno entity
    "VOC threshold",   // label: nápis na kartě
    100,               // min: méně než toto portál nezadá
    400,               // max: více než toto — taky; jádro navíc
                       //   ořízne hodnotu v těchto mezích na své straně
    10,                // step: krok změny hodnoty šipkami pole
    "",                // unit: jednotka měření; index ji nemá
    [](float v) {              // callback: volá se, když uživatel poslal
                               //   novou hodnotu; v — číslo v rozmezí min..max
        onThresholdChanged(v); //   předáme do naší logiky (napíšeme v kapitole 7)
    }
);
```

### Skládáme dohromady

Finální podoba bloku v `setup()` — to, co má v kódu zůstat. Funkce `onModeSelected` a `onThresholdChanged` napíšeme v kapitole 7; aby se kód přeložil již teď, deklarujte je jako prázdné zástupce **před** `setup()`:

```cpp
// Zástupci: skutečná těla napíšeme v kapitole 7 (logika automatiky).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetrické údaje: vlastní pole vocIndex (kapitola 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Karta: senzor + dva ovládací prvky.
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

A ventilátor? Deklarovat jej **není třeba**: příznak `hasFan = true` v `Config` již automaticky přidal dlaždici „Ventilátor" do manifestu — to je slovníková schopnost, jádro ji zná samo.

!!! note "Hranaté závorky v callbackech — vždy prázdné"
    `[](const char* opt) { ... }` — lambda, bezejmenná funkce; podrobně jsme ji rozebrali v [poznámce kapitoly 5](05-sensor-and-telemetry.md). Připomínka pravidla jádra: hranaté závorky jsou vždy prázdné (`[]`), do lambdy nic „s sebou" neberte, vše potřebné uchovávejte v globálních proměnných — jako `g_mode` a `g_threshold` z následující kapitoly.

## 2. Automatické rozvržení karty

Rozvržení vůbec nastavovat nemusíte. Portál kartu z deklarovaných entit sestaví sám — a sestaví ji pěkně: dlaždice s hodnotami se seskupí do řádků (nejvýše tři v řadě, pak zalomení), ovládací prvky jdou níže, každý na vlastním řádku, vše ve firemním designu portálu. Pro většinu zařízení to plně stačí — rozhraní vypadá úhledně bez jediné starosti o rozložení.

Pořadí entit na kartě odpovídá pořadí jejich deklarace v `setup()`.

## 3. Vlastní rozvržení karty (volitelné)

Nejprve — jak je karta uspořádaná. Karta je svislý sloupec **řádků**. Řádek buněk se senzory dělí šířku rovnoměrně: jedna buňka zabere celou šířku, dvě po polovině, tři po třetině. Ovládací prvky (seznam, pole, tlačítko) v řádku se skládají pod sebe na celou šířku.

Automatické rozvržení z předchozí sekce entity po těchto řádcích rozloží samo. Chcete-li sami rozhodovat, co s čím stojí vedle sebe, nastavte řádky ručně voláními `layoutRow`. Jedno volání = jeden řádek, pořadí volání = pořadí řádků shora dolů:

```cpp
// Řádek 1: dvě buňky — VOC index a ventilátor, každá po polovině šířky.
s_link.card().layoutRow("voc", "fan");

// Řádek 2: dva ovládací prvky — režim a práh, pod sebou.
s_link.card().layoutRow("mode", "threshold");
```

Do `layoutRow` se předávají **id** entit — interní jména přiřazená při deklaraci (právě proto bylo id potřebné). `"fan"` je id slovníkové entity ventilátoru, vytvořené příznakem `hasFan`.

Na kartě to vytvoří toto rozvržení:

```text
┌─ Air filter ────────────────────┐
│  ▮ 129            │  ≋ Off      │   ← řádek 1: voc, fan
├───────────────────┴─────────────┤
│  Mode                           │   ← řádek 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [100                         ] │
└─────────────────────────────────┘
```

Všimněte si buněk: portál vykresluje ikonu a hodnotu, bez popisku. `label` entity jde do jména pro čtečky obrazovky a do dalších rozhraní, a místo na kartě se tím šetří. U ovládacích prvků je popisek vidět — `Mode` a `VOC threshold` ve schématu výše.

Entity, které jste nezmínili v žádném řádku, se neztratí — portál je doplní pod ním automaticky. Takže stačí rozvrhnout jen „to nejdůležitější" a zbytek přenechat automatice.

## 4. Co se publikuje

Jádro zveřejní do topiku `idryer/{serial}/card` (retained):

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

Tento JSON rozebírat nemusíte — jádro jej generuje z vašich volání. Je ale dobré o něm vědět: pokud píšete firmware **ne** na `idryer-core` (Rust, MicroPython, cokoliv jiného), stačí takový JSON publikovat přímo — portál přijme cokoliv, pokud formát souhlasí.

## 5. Ověření

Nahrajte firmware a otevřete **dashboard portálu** — karta žije tam, kde karty sériových zařízení, a v mobilní aplikaci. Na stránce samotného zařízení není: tam jsou grafy slovníkových veličin, menu a integrace.

![Karta filtru na dashboardu portálu](../../img/10-filter/06-portal-card.png)
*Karta je sestavena z manifestu: VOC index a ventilátor v jednom řádku, režim a práh níže.*

![Táž karta v mobilní aplikaci](../../img/10-filter/06-app-card.png)
*Aplikace staví kartu z téhož manifestu — samostatný kód pro telefon není potřeba.*

- dlaždice indexu zobrazuje živou hodnotu (vydechněte na senzor — při příštím obnovení číslo vzroste);
- dlaždice ventilátoru — Zap/Vyp;
- **Mode** — rozbalovací seznam, **VOC threshold** — číselné pole: hodnota se odešle asi 0,6 s po změně, bez potvrzovacího tlačítka. Seznam a pole začínají první možností a minimem — posílají příkazy a neukazují aktuální nastavení zařízení; stav ukazují buňky (například ventilátor).

Výběr režimu a prahu zatím nic nedělá — to jsou prázdné zástupce callbacků. Oživíme je v [následující kapitole](07-auto-logic.md).

!!! note "To je právě ta koncepce"
    Všimněte si, co se stalo: popsali jste rozhraní pěti řádky v programu — a objevilo se v portálu i v aplikaci. Stejný postup funguje pro jakékoli vaše zařízení: mění se jen id, popisky a callbacky.

!!! note "Operace se spuštěním a zastavením"
    Seznam, pole a tlačítko posílají hodnotu hned. Operace se spuštěním a zastavením — sušení, ohřev, osvětlení — se deklarují jako **akce** karty s režimem a parametry spuštění: karta ukáže formulář spuštění a během operace blok relace s tlačítkem zastavení. Příklad — [kapitola 7](../09-build-a-device/07-heating-control.md) části o skříni.
