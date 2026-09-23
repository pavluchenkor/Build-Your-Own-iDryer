---
title: "Chytrý filtr: senzor VOC a telemetrie"
description: "Čtení SGP40 přes I2C a publikace vlastního pole vocIndex v telemetrii iDryer přes callback onTelemetryPublish."
---

# Senzor a telemetrie

V této kapitole filtr začíná měřit vzduch a posílat data do cloudu. Klíčová technika — **vlastní pole v telemetrii**: slovník ekosystému o VOC nic neví, ale jádro vám umožňuje do telemetrie přidat libovolné pole.

## 1. Knihovna senzoru

V `platformio.ini` přidejte do `lib_deps`:

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. Čtení SGP40

V `src/main.cpp` (piny — ze [schéma](03-wiring.md)):

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = údaje ještě nejsou

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() sám vede interní kompenzaci senzoru.
    // Index: ~100 = normální vzduch, více = špinavěji (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
}
```

Volání `initVocSensor()` přidejte do `setup()` za `s_link.begin()`, a `readVocSensor()` — do `loop()` jednou za sekundu (pomocí millis-časovače, nikoli `delay`!):

```cpp
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();
    }
}
```

!!! warning "Žádné delay() v loop"
    `s_link.loop()` musí být voláno nepřetržitě — závisí na něm Wi-Fi, MQTT i příkazy z portálu. `delay(1000)` vše zablokuje. Používejte výhradně millis-časovače.

## 3. Vlastní pole v telemetrii

Každých `telemetryPeriodMs` jádro samo sestaví JSON zprávu telemetrie a odešle ji do cloudu. Pro naše zařízení (jeden modul, ze slovníkových schopností jen ventilátor) jádro sestaví tuto zprávu:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

Rozeberme strukturu:

- `units` — pole **modulů** (komor) zařízení. Sériový sušák iDryer může mít až čtyři nezávislé komory, proto je telemetrie vždy pole, i když je komora jen jedna;
- `units[0]` — první (a u nás jediný) modul: v `Config` jsme zadali `unitsCount = 1`;
- `fanStatus` — slovníkové pole, přidané díky `hasFan = true`;
- `rssi`, `uptime` — síla Wi-Fi signálu a doba provozu, jádro je přidává vždy.

O VOC tato zpráva neříká nic — jádro o našem senzoru neví. Ale těsně před odesláním jádro vašemu kódu umožní do zprávy dopsat vlastní pole. Zaregistrujete proto **callback** (zpětné volání) — funkci, kterou předáte jádru, a jádro ji samo zavolá při každé publikaci a předá jí sestavený JSON (argument `doc`).

V `setup()`:

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc — ta samá telemetrická zpráva, která byla sebrana jádrem (viz JSON výše).
    // Doplňujeme do prvního bloku naše pole vocIndex.
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

Řádek `doc["units"][0]["vocIndex"] = g_vocIndex;` se čte takto: „ve zprávě `doc` vezmi pole `units`, v něm prvek `0` (náš jediný modul) a zapiš tam pole `vocIndex`". Název pole vymýšlíte sami — v [následující kapitole](06-card.md) se na něj odkážete, abyste zobrazili hodnotu na kartě.

!!! note "Pokud narazíte na slovo hook"
    Ve zdrojovém kódu jádra se tento callback nazývá `PublishHook` — „hook" (háček) znamená totéž: místo, kde vám knihovna dovolí „zavěsit" vlastní funkci. Termíny jsou zaměnitelné; v této dokumentaci používáme „callback".

!!! note "Lambda a proč je „prázdná""
    Konstrukce `[](JsonObject doc) { ... }` se nazývá **lambda** — bezejmenná funkce zapsaná přímo na místě použití, takže ji nemusíte vyčleňovat a vymýšlet pro ni jméno.

    Hranaté závorky na začátku jsou „seznam zachytávání": uvádíte v nich lokální proměnné, které chcete funkci předat. Pravidlo jádra: **závorky jsou vždy prázdné** (`[]`) — lambda nic nezachytává a nenese s sebou žádný stav (anglicky *stateless*, „bez stavu").

    Důvod je technický: lambdy se zachytáváním vyžadují dynamickou paměť, a její časté alokování na ESP32 fragmentuje haldu a může v krajním případě shodit Wi-Fi. Proto jádro přijímá pouze prosté funkce.

    Praktický závěr je jediný: vše, co callback potřebuje, uchovávejte v **globálních** proměnných — jako naše `g_vocIndex`. Toto pravidlo platí pro všechny callbacky `idryer-core`.

Stav ventilátoru se publikuje slovníkovou cestou — zapište jej do pole jádra při zapnutí/vypnutí (logika — v [kapitole 7](07-auto-logic.md)):

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. Nemáte senzor po ruce? Demo režim

Projít cestu až ke kartě lze i bez SGP40: index spočítá model vzduchu. Zkopírujte soubor `demo_voc.h` z příkladu:

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

a přidejte do `platformio.ini` příznak sestavení:

```ini
build_flags =
    -DDEMO_VOC=1
```

Obě větve jsou schované za jednou funkcí a `loop()` neví, odkud hodnota přišla:

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

Model se chová jako místnost, ve které tiskne tiskárna: dokud ventilátor stojí, index stoupá od klidové hodnoty kolem `100`, a při běžícím ventilátoru klesá. Stav ventilátoru model bere z vaší vlastní logiky, takže práh a hystereze z [kapitoly 7](07-auto-logic.md) jsou vidět v provozu.

Pro provozní zařízení se příznak nenastavuje: pak se sestaví větev se skutečným senzorem.

## 5. Ověření

Po nahrání firmwaru v MQTT streamu zařízení (nebo v Serial logu publikací) bude telemetrie vypadat takto:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` — vaše vlastní pole, které odešlo do cloudu společně se slovníkovým `fanStatus`. Portál jej již přijímá, ale zatím neví, co s ním dělat: ukažte mu to v následující kapitole.

!!! note "Vlastní pole žije v reálném čase"
    Portál rozesílá telemetrii klientům tak, jak přišla, takže karta ukazuje `vocIndex` okamžitě. Do historie pro grafy ale server zapisuje jen slovníkové veličiny — teplotu, vlhkost, ohřev. Vlastní pole na grafu telemetrie nebude; pokud potřebujete historii, sbírejte si ji sami (například Home Assistant nebo vlastní databáze).

Vydechněte na senzor nebo přiložte fix — index by měl za pár sekund znatelně vzrůst.
