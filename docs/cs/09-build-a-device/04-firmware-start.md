---
title: "Spuštění firmwaru na idryer-core: první spuštění a vazba na portál"
description: "Vytvoření projektu PlatformIO na knihovně idryer-core: platformio.ini, Config zařízení, první nahrání firmwaru do ESP32, nastavení Wi-Fi a propojení zařízení s portálem iDryer v aplikaci."
---

# Spuštění firmwaru na jádru

Na této stránce vytvoříte projekt firmwaru, uvedete ESP32 do stavu Online na portálu a ověříte, že síťová část funguje. Senzory a logiku ohřevu přidáme v dalších krocích.

Přístup je postaven na fasádě `iDryer::Link`. Popisujete zařízení jedinou strukturou `iDryer::Config`, voláte `link.begin()` a `link.loop()` — veškeré připojení k síti jádro provádí samo.

## 1. Připravte si nástroje

Budete potřebovat:

- VS Code s rozšířením PlatformIO;
- USB kabel;
- Wi-Fi síť `2.4 GHz` (ESP32 nefunguje se sítěmi pouze `5 GHz`);
- chytrý telefon s aplikací iDryer ([App Store](https://apps.apple.com/app/idryer/id6760609044), [Google Play](https://play.google.com/store/apps/details?id=org.idryer.mobile)) přihlášenou k vašemu účtu na portálu iDryer: přes ni zařízení dostane síť Wi-Fi a propojí se s účtem;
- knihovna jádra [idryer-core](https://github.com/pavluchenkor/idryer-core);
- hotový projekt této kapitoly — [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet) v repozitáři učebnice: odtud se berou ovladač senzoru a další soubory, které se dále doporučuje zkopírovat.

Co je firmware kontroléru a jak se dostane do desky — [Firmware kontroléru](../02-controllers/11-flashing-controller.md).

## 2. Vytvořte projekt

V PlatformIO je projekt složka s pevnou strukturou. Vytvořte složku projektu (například `my-cabinet`) a otevřete ji ve VS Code. Uvnitř by měly být tyto soubory:

```text
my-cabinet/
├── platformio.ini        # nastavení sestavení (vyplníme v kroku 4)
├── lib/
│   └── idryer-core/      # knihovna jádra (symlink nebo kopie)
└── src/
    └── main.cpp          # kód zařízení: Config + setup() + loop()
```

Všechny fragmenty kódu níže se umísťují právě do těchto souborů — v každém kroku je uvedeno, do kterého. Vytvořte složky `include/`, `lib/` a `src/` ručně, pokud neexistují.

Knihovnu `idryer-core` vložte do `lib/` — PlatformIO tam automaticky hledá knihovny. Nejjednodušší je vytvořit symlink na staženou knihovnu:

```bash
git clone https://github.com/pavluchenkor/idryer-core.git ~/idryer-core
ln -s ~/idryer-core lib/idryer-core
```

Místo symlinku můžete složku knihovny jednoduše zkopírovat do `lib/idryer-core` — funguje to stejně.

To je také nutné pro generování menu (kapitola 6) — háček hledá generátor uvnitř `lib/idryer-core/`.

## 3. Wi-Fi a propojení nejsou v kódu

Firmware neobsahuje heslo k síti ani údaje účtu. Při prvním spuštění zařízení nemá Wi-Fi a čeká na nastavení: aplikace iDryer je pošle vzduchem (ESPTouch) a potom zařízení propojí s vaším účtem jednorázovým párovacím tokenem. Jádro to vše dělá uvnitř `s_link.begin()` a `s_link.loop()`, vy jen projdete kroky v aplikaci — oddíl 9.

**Jak se síť dostane do zařízení.** Deska bez uložené sítě poslouchá éter jako přijímač neladěný na stanici. Telefon mezitím „vyťukává" název sítě a heslo do vzduchu — přibližně jako morseovkou, jen pakety Wi-Fi. Deska tento přenos zachytí, připojí se k síti a dále do ní vstupuje sama při každém zapnutí. Zvláštní piny ani vodiče k tomu nejsou potřeba: pracuje standardní anténa desky, zapíná se to samo, dokud síť není, a trvá až 90 sekund.

Pokud se to vzduchem nepodařilo, existuje drátová cesta: webový instalátor [install.idryer.org](https://install.idryer.org) předá desce síť a párovací token přes USB — totéž, co dělá aplikace, jen kabelem. Pomáhá i běžný restart desky: po něm znovu čeká na nastavení.

## 4. Nakonfigurujte platformio.ini

Vyplňte `platformio.ini` v kořeni projektu:

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; Knihovny jádra (MQTT, ArduinoJson, WebSockets, Improv) přijdou
; samy z lib/idryer-core/library.json.
; ESPAsyncTCP je transport pro ESP8266 ze závislostí espMqttClient:
; na ESP32 se nesestaví, je nutné ho vyloučit.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Nahraďte `board` svou deskou (například `esp32-s3-devkitc-1`). Samotnou `idryer-core` nemusíte zadávat v `lib_deps` — leží v `lib/` (krok 2).

!!! note "Co tyto řádky dělají"
    Závislosti jádra nevypisujete: PlatformIO je vezme z `lib/idryer-core/library.json`. `lib_ignore = ESPAsyncTCP` je povinné — bez něj sestavení spadne v `ESPAsyncTCP.cpp`. Povinné jsou i příznaky `MQTT_BROKER` a `MQTT_PORT` — bez nich se jádro nezkompiluje (`'MQTT_BROKER' was not declared`).

## 5. Popište zařízení v Config

Dále se vše odehrává v jednom souboru — `src/main.cpp`. Otevřete jej a zapište kód z tohoto a dalších kroků.

`iDryer::Config` — to je pas zařízení. Příznaky `has*` informují portál, co zařízení má, a určují, která telemetrická pole se publikují.

Pro vytápěnou skříň na začátek `src/main.cpp`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // vlastní zařízení: kartu sestavuje manifest
    .unitsCount        = 1,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "Příznaky has* — to je kontrakt s portálem"
    Pole telemetrie, které má odpovídající příznak `false`, se nepublikuje. Například bez `hasAirHumidity = true` se vlhkost nedostane do cloudu, i když ji napíšete do kódu. Zapínejte pouze to, co je fyzicky v zařízení.

Výčet součástek a příznaků — [Součásti systému](02-bom.md).

## 6. Minimální hlavní

Ve stejném souboru za blokem `Config` přidejte funkce `setup()` a `loop()`. Pro první spuštění stačí inicializovat odkaz a točit jej v `loop()`:

```cpp
void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

`s_link.begin()` spustí Wi-Fi, propojení a spojení s portálem. Příkaz `revoke` přijde z portálu, když zařízení od účtu odpojíte: `handleRevoke()` smaže tajný klíč zařízení a to čeká na nové propojení. Senzory přidáme v kroku [Senzory](05-sensors.md).

### Úplný `src/main.cpp` po této kapitole

Vezměte oba bloky výše do jednoho souboru — to je celý `src/main.cpp` v tomto kroku:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // vlastní zařízení: kartu sestavuje manifest
    .unitsCount        = 1,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

Předchozí kapitola ukazuje, **co přidat** a **úplný `src/main.cpp` po změnách**, aby jste vždy viděli celý obrázek, ne rozprostřené kusy.

## 7. Prošijte

```bash
pio run -e cabinet -t upload
```

## 8. Otevřete Serial Monitor

```bash
pio device monitor -b 115200
```

Dokud zařízení nemá Wi-Fi, log mlčí: jádro drží sériový port pro webový instalátor (Improv). Logy se zapnou, jakmile naběhne Wi-Fi. U zařízení, které ještě není propojené, log končí takto:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Poslední řádek je přesně to, co je v tomto kroku potřeba: síť je, párovací tajný klíč není, zařízení čeká na token z aplikace. Nechte monitor otevřený a přejděte do aplikace.

## 9. Připojte Wi-Fi a propojte zařízení v aplikaci

1. Připojte telefon k síti Wi-Fi, ve které bude zařízení pracovat (`2.4 GHz`), a přihlaste se do aplikace iDryer svým účtem portálu.
2. Na hlavní obrazovce klepněte na **Připojit nové zařízení** — otevře se krok **Wi-Fi**.
3. Zkontrolujte název sítě (aplikace ho doplní sama, pokud je zapnutá poloha), zadejte heslo a klepněte na **Připojit zařízení**. Aplikace posílá nastavení až 90 sekund; když se zařízení připojí k síti, zobrazí se **Zařízení připojeno**. Klepněte na **Další**.
4. V kroku **Spárování** klepněte na **Spárovat**. Aplikace najde zařízení v síti, získá od portálu jednorázový párovací token, předá ho zařízení a počká, až portál potvrdí, že je zařízení online.
5. Po zprávě **Zařízení spárováno** se zařízení objeví v seznamu zařízení na portálu i v aplikaci.

Pokud už je zařízení v síti, otevřete rovnou krok **Spárování** — klepněte na jeho čip nahoře v okně.

![Krok Wi-Fi v aplikaci: název sítě a heslo](../../img/09-cabinet/04-app-wifi.png)
*Krok **Wi-Fi**: aplikace předává síť zařízení vzduchem.*

![Krok spárování: aplikace našla zařízení v síti](../../img/09-cabinet/04-app-pairing.png)
*Krok **Spárování**: aplikace našla zařízení v síti podle jeho sériového čísla. Cizí zařízení jsou označena jako obsazená.*

![Zpráva „zařízení spárováno"](../../img/09-cabinet/04-app-paired.png)
*Hotovo: zařízení je spárováno s účtem a za okamžik se objeví v seznamu.*

V logu je propojení vidět:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## Ověření výsledku

V této fázi by mělo být zařízení na portálu Online. Data ze senzorů zatím nejsou — to je v pořádku: `Config` o nich zatím nic nedeklaroval a karta nemá co ukázat.

![Karta zařízení na portálu hned po spárování](../../img/09-cabinet/04-portal-card.png)
*Zařízení na portálu: název, stav Idle, ikona spojení. Hodnoty nejsou — objeví se v další kapitole.*

Název `Device DEVICE_…` je tovární. Přejmenujte zařízení tužkou vedle názvu: dále se v příkladech jmenuje „Storage cabinet".

Pokud se něco nepovedlo:

- aplikace se nedočkala připojení zařízení k síti — zkontrolujte heslo a že síť je `2.4 GHz`; při špatném hesle zařízení znovu čeká na nastavení, restartujte desku a zopakujte krok Wi-Fi;
- síť se vzduchem stále nepředává — udělejte totéž přes USB pomocí webového instalátoru [install.idryer.org](https://install.idryer.org);
- v kroku **Spárování** aplikace zařízení nenašla — telefon a zařízení musí být ve stejné síti a síť nesmí blokovat vyhledávání zařízení (hostovské sítě to často dělají);
- zařízení se restartuje — zkontrolujte napájení ESP32 (poklesy napětí při startu jsou častou příčinou resetů);
- sestavení padá s chybou — zeptejte se v komunitě: [Telegram](https://t.me/iDryer), [Discord](https://discord.gg/jGce5eeHHz);
- viz [Chyby napájení](../08-common-mistakes/02-power-mistakes.md) a [Chyby řadiče](../08-common-mistakes/04-controller-mistakes.md).

## Co dál

Síťová část funguje. Přejděte na [Senzory](05-sensors.md): připojíme SHT31 a termistor a uvidíme jejich data na portálu.
