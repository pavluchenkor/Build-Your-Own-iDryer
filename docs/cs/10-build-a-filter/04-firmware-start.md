---
title: "Chytrý filtr: start programu a připojení k portálu"
description: "Kostra programu filtru na idryer-core: Config nestandardního typu zařízení, první spuštění, spárování s účtem v aplikaci."
---

# Start programu

Kostra projektu je totožná s [kapitolou z příkladu se skříní](../09-build-a-device/04-firmware-start.md): PlatformIO, `idryer-core` v `lib/`, stejný `platformio.ini` (změňte pouze název prostředí na `filter`). Zde — jen to, co se liší.

Hotový projekt této kapitoly — [example/10-filter](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/10-filter) v repozitáři učebnice: odtud se berou `platformio.ini` a všechen kód, který se dále rozebírá po částech. Knihovna jádra — [idryer-core](https://github.com/pavluchenkor/idryer-core).

!!! note "Log do portu: dva příznaky sestavení"
    U ESP32-C3 jde výstup `Serial` ve výchozím nastavení na piny UART0, nikoli na USB port desky — monitor portu zůstává prázdný. Abyste log viděli, musí být v `build_flags` dva řádky:

    ```ini
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    ```

    Zprávy samotného ESP-IDF (chyby mDNS a podobné) jdou na USB i bez nich, takže „něco se vypisuje, ale moje řádky chybí" je známkou právě těchto chybějících příznaků.

## Config: zařízení nestandardního typu

Filtr nemá ani ohřívač, ani klimatický senzor ze slovníku ekosystému. Ze „slovníkových" schopností má pouze ventilátor. V `src/main.cpp`:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // nestandardní zařízení
    .unitsCount        = 1,
    // Periférie: ze slovníku ekosystému máme jen ventilátor.
    .hasFan            = true,
    // Období automatického zveřejňování:
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identifikace v portálu:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
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

!!! note "DeviceType::Unknown — to je v pořádku"
    Typ `Unknown` znamená „portál takový výrobek nezná". Dříve to byl problém: portál neměl kartu pro neznámý typ. Dnes je to standardní cesta: rozhraní zařízení popíše manifest karty ([kapitola 6](06-card.md)) a portál si kartu sestaví podle něj. Typ je potřebný jen pro vlastní výrobky iDryer, které mají firemní karty.

Příznak `hasFan = true` přináší zdarma: pole `fanStatus` v telemetrii, dlaždici „Ventilátor" na kartě a entitu v manifestu — vše ze slovníku ekosystému.

## VOC senzor v Config není — a být nemá

Všimněte si: v `Config` žádný příznak „hasVoc" není. Slovník `has*` popisuje periferie, které ekosystém zná. Vlastní senzor nepřidáte přes slovník, ale dvěma jinými mechanismy: jeho hodnotu dopíšete do telemetrie vlastním polem a deklarujete ho v manifestu karty — to jsou následující dvě kapitoly. V tom spočívá smysl tohoto přístupu: slovník není potřeba rozšiřovat pro každé nové zařízení.

## První spuštění a spárování

Postup je stejný jako u skříně:

1. Nahrajte firmware a otevřete Serial Monitor: dokud zařízení nemá Wi-Fi, log mlčí.
2. V aplikaci iDryer: **Připojit nové zařízení** → krok **Wi-Fi** (síť a heslo, **Připojit zařízení**) → krok **Spárování** → **Spárovat**.
3. Po zprávě **Zařízení spárováno** je zařízení propojené s vaším účtem a na portálu přejde do stavu `Online`; v logu je `MQTT: Connected!`.

Podrobnosti, možné chyby a opětovné propojení — v [kapitole příkladu se skříní](../09-build-a-device/04-firmware-start.md).

![Stránka zařízení na portálu hned po spárování](../../img/10-filter/04-portal-device.png)
*Zařízení na portálu: název, stav Idle, ikona spojení. Graf je prázdný a menu nepřišlo — zařízení o nich nic neohlásilo.*

V portálu je zařízení vidět, ale karta je zatím téměř prázdná — data ještě nejsou. Jdeme připojit senzor.
