---
title: "Heizungsmanagement des Schranks: Temperaturregelung und Lüfter"
description: "Logik des beheizten Schranks auf idryer-core: Halten der Zieltemperatur durch Hysterese, Heizerschutz durch Thermistor, Lüfter und Portal-Befehle."
---

# Heizungsmanagement

Auf dieser Seite verbinden Sie Sensoren, Einstellungen und Leistungsteile zu einer funktionierenden Logik. Das Gerät hält die eingestellte Temperatur im Schrank, schützt den Heizer vor Überhitzung und reagiert auf Befehle vom Portal.

Die Logik wird in `loop()` neben der Netzwerkbereitstellung ausgeführt. Alle Timer und Schwellwerte sind nicht-blockierend, ohne `delay()`.

## Was sollte passieren

Das Verhalten des Schranks besteht aus drei einfachen Regeln:

1. **Temperaturregelung.** Wenn die Luft im Schrank unter dem Ziel um den Hysterese-Wert liegt – Heizung einschalten. Wenn das Ziel erreicht ist – ausschalten.
2. **Heizerschutz.** Der Thermistor überwacht den Heizer selbst. Wenn er über das zulässige Maß hinaus überhitzt wird – wird die Heizung unabhängig von der Lufttemperatur ausgeschaltet.
3. **Lüfter.** Er wird eingeschaltet, um die Wärme im Schrank zu verteilen, und ausgeschaltet, wenn keine Heizung erforderlich ist.

## Schalter für Heizer und Lüfter

Der Controller schaltet Heizer und Lüfter über einen Schalter ein: MOSFET-Modul (Version A) oder SSR (Version B) – siehe [Schaltplan](03-wiring.md). Aus Code-Sicht ist dies einfach ein GPIO-Ausgang: `HIGH` – eingeschaltet, `LOW` – ausgeschaltet.

Beschreiben Sie einen solchen Schalter mit einer kleinen Struktur und erstellen Sie zwei Instanzen – für Heizer und Lüfter. Fügen Sie dies zu `src/main.cpp` (vor `setup()`) hinzu:

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 – Heizungssteuerung
static GpioOutput myFan{5};      // GPIO5 – Lüftersteuerung
```

Die Pin-Nummern sind dieselben wie im [Schaltplan](03-wiring.md). In `setup()` müssen beide Schalter initialisiert werden: `myHeater.begin();` und `myFan.begin();`.

!!! warning "Sicherer Zustand beim Start"
    `begin()` setzt sofort `LOW` – Heizer und Lüfter sind ausgeschaltet, bis die Logik anderes entscheidet. Das ist wichtig: Bei der Stromversorgung sollte der Heizer nicht versehentlich eingeschaltet werden.

## Temperaturregelung durch Hysterese

Für einen Schrank bei `40–45 °C` ist eine einfache Hysterese ausreichend: Die Heizung wird um das Ziel herum ein- und ausgeschaltet. Dies ist einfacher als vollwertiger PID und funktioniert bei sanfter Wärmeregelung zuverlässig.

Die Hysterese kommt aus dem Menü (`menu.hysteresis`) — es ist bereits in [Kapitel 6](06-menu.md) angebunden. Die Zieltemperatur legt der Nutzer beim Start des Schranks über die Gerätekarte fest (`s_targetC`; die Karte wird später in diesem Kapitel angebunden). Geheizt wird nur im Storage-Modus. Fügen Sie den Zustand und die Entscheidungsfunktion hinzu:

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // Ziel des aktuellen Laufs, von der Karte

static void controlLoop() {
    // Nur im Storage-Modus heizen: nach Stopp kühlt der Schrank ab.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // von der Karte
    float hyst   = (float)menu.hysteresis;           // aus Menü

    if (air < target - hyst) {
        s_heating = true;     // abgekühlt – heizen
    } else if (air >= target) {
        s_heating = false;    // Ziel erreicht – stopp
    }
}
```

Die Zieltemperatur kommt mit dem Startbefehl von der Karte; ihre Grenzen und ihr Standardwert sind der Punkt `target_temp` im [Menü](06-menu.md).

## Heizerschutz durch Thermistor

Die Luft wärmt sich langsam auf, die Heizerspirale schnell. Ohne separate Kontrolle kann sich der Heizer überhitzen, bevor die Luft das Ziel erreicht. Daher setzt der Heizer-Thermistor eine harte Obergrenze.

```cpp
static const float HEATER_MAX_C = 80.0f;   // Obergrenze der Heizer-Temperatur

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // Thermistor

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // in Telemetrie widerspiegeln
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "Heizer-Obergrenze ist Schutz, nicht Klimaregelung"
    `HEATER_MAX_C` begrenzt die Temperatur des Heizers selbst, nicht der Luft. Der Wert hängt von der Heizerkonstruktion und den Gehäusematerialien ab. Wählen Sie ihn mit einem Puffer unterhalb der Temperatur, bei der die gedruckten Teile verformen – siehe [Hitzebeständige Materialien](../07-3d-printing/04-heat-resistant-materials.md).

Für sanfteres Heizen können Sie die Leistung statt Ein-/Ausschalten über PWM steuern und das Feld `heaterPower01[0]` akzeptiert Werte von `0.0` bis `1.0`. Für einen Schrank mit sanfter Wärmeregelung ist die obige einfache Logik normalerweise ausreichend.

## Lüfter

Der Lüfter verteilt die Wärme im Schrank. Die einfachste Logik – schalte ihn zusammen mit der Heizung ein:

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // läuft, während wir heizen
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // in Telemetrie widerspiegeln
}
```

Im seriellen Controller wird der Lüfter nach Temperatur mit separaten Ein- und Ausschaltgrenzen gesteuert (z.B. Einschalten bei `55 °C`, Ausschalten bei `35 °C`), damit er an der Grenze nicht zittert. Für den Schrank können Sie denselben Ansatz anwenden und die Grenzen an Menüparameter binden.

## In loop() zusammenstellen

```cpp
void loop() {
    s_link.loop();          // Netzwerk und automatische Veröffentlichung

    // Sensoren (siehe Schritt "Sensoren"):
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // entscheiden, zu heizen oder nicht
    applyHeater();   // auf Heizer anwenden + Schutz
    applyFan();      // auf Lüfter anwenden
}
```

Die Telemetrie-Felder (`heaterPower01`, `fanOn`) veröffentlicht die Fassade selbst – im Portal ist sichtbar, ob das Gerät gerade heizt und ob der Lüfter läuft.

## Karte: Start und Stopp

Start und Stopp kommen von der Gerätekarte im Portal und in der App. Die Firmware deklariert sie als **Aktionen** der Karte: der Core fügt sie ins Card-Manifest ein, Portal und App zeichnen Formular und Schaltflächen selbst. Befehle im Code auszuwerten ist nicht nötig — der Core ruft Ihre Funktion auf.

Die Grenzen des Temperaturfelds und der Standardwert kommen aus dem Menüpunkt `target_temp` (30–50 °C, 45) über die Brücke `card_menu_bridge.h`. Der eingegebene Wert geht mit dem Startbefehl mit und wird nicht ins Menü geschrieben. Fügen Sie den Header neben den Menü-Headern aus Kapitel 6 hinzu:

```cpp
#include <card/card_menu_bridge.h>
```

Callbacks der Aktionen — vor `setup()`:

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // bereits innerhalb 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}
```

Deklarieren Sie in `setup()` nach den Menübefehlen aus Kapitel 6 die Aktionen. Die Menüwerte liegen bereits im Cache, aus dem die Karte liest: `setup()` ruft seit Kapitel 6 `menu_sync_state_to_cache()` auf.

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` und `"IDLE"` — der Modus der Einheit nach der Aktion. Solange der Schrank ruht, zeigt die Karte das Startformular; im Modus `STORAGE` den Sitzungsblock und die Stopp-Schaltfläche.
- `MENU_TARGET_TEMP` — die id des Punkts `target_temp`; der Generator legt sie in `menu_ids.h` ab.
- `s_link.status.mode[0]` und `targetTempC[0]` zeigen den aktuellen Zustand der Kammer. Rufen Sie nach jeder Änderung `publishStatusNow()` auf, damit die Karte sofort umschaltet.
- `iDryer::UnitMode::Storage` — Modus der sanften Wärmehaltung. Das ist der Hauptmodus des Schranks.
- Ändern Sie die Lagertemperatur im Gerätemenü im Portal — der Standardwert des Kartenfelds folgt ihr: der Core bemerkt die Menüänderung selbst und veröffentlicht das Manifest neu.

Der Core fügt ins Card-Manifest ein:

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

Im Portal bekommt die Karte des ruhenden Schranks das Feld `Temp.` mit 45 °C und die Schaltfläche `Lagerung`; nach dem Start den Sitzungsblock mit dem Ziel und die Schaltfläche `Stop`. In der App zeigt die Startseite Messwerte und die laufende Sitzung, Start und Stopp liegen auf der Geräteseite. Sensoren, Felder und Layout der Karte behandelt das Kapitel [Gerätekarte](../10-build-a-filter/06-card.md) im Abschnitt zum Luftfilter.

!!! warning "Kein delay() in den Callbacks"
    Die Callbacks der Aktionen werden aus dem Netzwerk-Handler aufgerufen. Jede Blockierung darin unterbricht die MQTT-Sitzung. Ändern Sie Ziel und Status, die eigentliche Arbeit gehört in `loop()`.

## Vollständige `src/main.cpp` nach diesem Kapitel

Dies ist die endgültige, vollständige Datei des Geräts. Neue Zeilen gegenüber dem vorherigen Kapitel sind mit `// ← Kapitel 7` gekennzeichnet. Diese Datei liegt auch als fertiges Beispiel im Ordner `example/09-cabinet/` des Repositoriums und wird mit dem Befehl `pio run -e cabinet` erstellt.

??? note "Was vorher war – `src/main.cpp` nach Kapitel 6"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include <menu_state.h>                      // ← Kapitel 6: Parameter (menu.target_temp …)
    #include <menu_bindings.h>                   // ← Kapitel 6: menu_apply_by_bind
    #include <menu_commands.h>                   // ← Kapitel 6: menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← Kapitel 6: publishConfigRaw

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Dryer,
        .unitsCount        = 1,
        .hasHeater         = true,
        .hasFan            = true,
        .hasAirTemp        = true,
        .hasAirHumidity    = true,
        .hasHeaterTemp     = true,
        .telemetryPeriodMs = 5000,
        .statusPeriodMs    = 10000,
        .hardwareVersion   = "1.0",
        .firmwareVersion   = "0.1.0",
        .model             = "DIY Storage Cabinet",
    };
    static iDryer::Link s_link(CFG);

    static Sht31ClimateSensor s_climate(&Wire);
    static bool               s_climateOk = false;

    static const int   THERM_PIN  = 2;
    static const float SERIES_R   = 4700.0f;
    static const float NOMINAL_R  = 100000.0f;
    static const float NOMINAL_T  = 25.0f;
    static const float BETA       = 3950.0f;

    static float readHeaterTempC() {
        int   raw = analogRead(THERM_PIN);
        float v   = (float)raw / 4095.0f;
        float r   = SERIES_R * (1.0f - v) / v;
        float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
        return tK - 273.15f;
    }

    // ← Kapitel 6: Menü im Portal
    static bool s_menuPending = false;

    static void publishMenu() {
        static char buf[MENU_FULL_JSON_BUF_SIZE];
        const size_t len = menu_buildFullJson(buf, sizeof(buf));
        if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
    }

    static void applySet(JsonObjectConst data) {
        const int id = data["id"] | -1;
        float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                         : data["val"].as<float>();
        for (uint16_t i = 0; i < g_bindings_count; i++) {
            if ((int)g_bindings[i].id != id) continue;
            const MenuMeta& m = g_menu_meta[id];
            if (v < m.min_val) v = m.min_val;
            if (v > m.max_val) v = m.max_val;
            menu_apply_by_bind(g_bindings[i].bind, v);
            s_menuPending = true;
            return;
        }
    }

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        menu.initDefaults();                     // ← Kapitel 6
        menu.loadFromNVS();                      // ← Kapitel 6
        menu_sync_state_to_cache();              // ← Kapitel 6
        s_link.begin();
        // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← Kapitel 6
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← Kapitel 6
    }

    void loop() {
        s_link.loop();

        // ← Kapitel 6: Menü beim Online-Gehen und auf Anfrage veröffentlichen
        static bool s_wasOnline = false;
        const bool online = s_link.isOnline();
        if (online && !s_wasOnline) s_menuPending = true;
        s_wasOnline = online;
        if (s_menuPending) {
            s_menuPending = false;
            publishMenu();
        }

        if (s_climateOk) {
            s_climate.tick(millis());
            SensorReading r = s_climate.get();
            if (r.ok) {
                s_link.telemetry.airTempC[0]       = r.temperature;
                s_link.telemetry.airHumidityPct[0] = r.humidity;
            }
        }
        s_link.telemetry.heaterTempC[0] = readHeaterTempC();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>
#include <math.h>
#include "Sht31ClimateSensor.h"
#include <menu_state.h>
#include <menu_bindings.h>
#include <menu_commands.h>
#include <local_access/device_publisher.h>
#include <card/card_menu_bridge.h>        // ← Kapitel 7

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    .hasHeater         = true,
    .hasFan            = true,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = true,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

static const int   THERM_PIN  = 2;
static const float SERIES_R   = 4700.0f;
static const float NOMINAL_R  = 100000.0f;
static const float NOMINAL_T  = 25.0f;
static const float BETA       = 3950.0f;

static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);
    float v   = (float)raw / 4095.0f;
    float r   = SERIES_R * (1.0f - v) / v;
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}

static bool s_menuPending = false;

static void publishMenu() {
    static char buf[MENU_FULL_JSON_BUF_SIZE];
    const size_t len = menu_buildFullJson(buf, sizeof(buf));
    if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
}

static void applySet(JsonObjectConst data) {
    const int id = data["id"] | -1;
    float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                     : data["val"].as<float>();
    for (uint16_t i = 0; i < g_bindings_count; i++) {
        if ((int)g_bindings[i].id != id) continue;
        const MenuMeta& m = g_menu_meta[id];
        if (v < m.min_val) v = m.min_val;
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);
        s_menuPending = true;
        return;
    }
}

// ← Kapitel 7: Schalter für Heizer und Lüfter
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← Kapitel 7: Logik der Temperaturregelung
static bool        s_heating    = false;
static float       s_targetC    = 0.0f;
static const float HEATER_MAX_C = 80.0f;

static void controlLoop() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) { s_heating = false; return; }
    float air    = s_link.telemetry.airTempC[0];
    float target = s_targetC;
    float hyst   = (float)menu.hysteresis;
    if (air < target - hyst)  s_heating = true;
    else if (air >= target)   s_heating = false;
}

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];
    bool  allow = s_heating && heaterTemp < HEATER_MAX_C;
    if (allow) myHeater.on(); else myHeater.off();
    s_link.telemetry.heaterPower01[0] = allow ? 1.0f : 0.0f;
}

static void applyFan() {
    if (s_heating) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = s_heating;
}

// ← Kapitel 7: Aktionen der Karte
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);
    s_climateOk = s_climate.begin();
    myHeater.begin();              // ← Kapitel 7
    myFan.begin();                 // ← Kapitel 7
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← Kapitel 7
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}

void loop() {
    s_link.loop();

    static bool s_wasOnline = false;
    const bool online = s_link.isOnline();
    if (online && !s_wasOnline) s_menuPending = true;
    s_wasOnline = online;
    if (s_menuPending) {
        s_menuPending = false;
        publishMenu();
    }

    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // ← Kapitel 7
    applyHeater();   // ← Kapitel 7
    applyFan();      // ← Kapitel 7
}
```

## Überprüfung des Ergebnisses

Nach diesem Schritt:

- die Schaltfläche `Lagerung` auf der Gerätekarte versetzt den Schrank mit der eingegebenen Temperatur in den Storage-Modus, das Gerät beginnt zu heizen;
- Die Lufttemperatur nähert sich dem Ziel an und bleibt im Hysterese-Bereich;
- Der Heizer bleibt nicht über `HEATER_MAX_C`;
- Lüfter und Heizleistung sind in der Telemetrie sichtbar;
- die Schaltfläche `Stop` schaltet die Heizung aus und wechselt in Idle; bis zum nächsten Start heizt der Schrank nicht.

## Was kommt als nächstes

Die Logik ist fertig. Es bleibt, das Gerät in das Gehäuse einzubauen und unter Strom zu überprüfen – [Montage und Überprüfung](08-assembly-and-check.md).
