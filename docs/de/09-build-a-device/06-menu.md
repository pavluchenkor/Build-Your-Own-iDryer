---
title: "Gerätemenü aus YAML: Einstellungen in NVS und auf dem Portal"
description: "Wie man ein Gerätemenü auf idryer-core in menu.yaml beschreibt: Zieltemperatur und Hysterese werden in NVS gespeichert und im Gerätemenü auf dem iDryer-Portal angezeigt."
---

# Menü aus YAML

Das Menü ist ein Satz von Geräteeinstellungen: Zieltemperatur, Hysterese, Ventilator-Schwellwerte. Auf `idryer-core` wird das Menü in einer einzigen Datei `menu.yaml` beschrieben, und alles Übrige – C++-Strukturen, Speicherung im nichtflüchtigen Speicher (NVS) und Veröffentlichung auf dem Portal – wird automatisch generiert.

Dies ist einer der Schlüsselbausteine des Kerns. Sie schreiben keinen Code zur Speicherung von Einstellungen und erfinden keine Formate für das Portal – Sie zählen nur die Parameter in YAML auf.

## Warum ein Menü

Nach den vorherigen Schritten liest das Gerät Sensoren, aber alle Schwellwerte sind im Code hartcodiert. Das Menü löst drei Aufgaben gleichzeitig:

- **Speicherung**: Werte überstehen einen Neustart (NVS);
- **Fernverwaltung vom Portal**: das Portal zeigt jeden Menüpunkt nach seinem Typ (Zahl, Schalter);
- **Single Source of Truth**: eine Datei beschreibt Speicher und Schnittstelle.

## Wie es funktioniert

Eine Datei `menu.yaml` wird während des Builds durch einen Generator verarbeitet:

```text
menu.yaml → (pio run build) → C++-Dateien in src/menu/ + NVS + JSON für Portal
```

Das Portal zeichnet jeden Menüpunkt nach seinem Typ. `role:` gibt einem Punkt eine übersetzte Beschriftung aus dem Kernvertrag; ein Punkt ohne `role:` erscheint mit seinem `title`.

!!! warning "Bearbeiten Sie nicht die generierten Dateien"
    Die Dateien `menu_state.*`, `menu_bindings.*`, `menu_ids.h` und andere werden vom Generator erstellt. Bearbeiten Sie nur `menu.yaml` und bauen Sie neu auf – andernfalls werden Ihre Änderungen überschrieben.

## Schritt 1. Kopieren Sie die Vorlage

Es gibt eine Menü-Vorlage in der Bibliothek. Kopieren Sie sie in Ihr Projekt:

```bash
mkdir -p src/menu
cp path/to/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## Schritt 2. Aktivieren Sie die Generierung beim Build

Kopieren Sie das Hook-Beispiel aus dem `iDryer-Storage`-Projekt (Sie können es unverändert verwenden):

```bash
mkdir -p extra_scripts
cp path/to/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

Fügen Sie dann in `platformio.ini` in die Sektion `[env:cabinet]` die Zeile `-Isrc/menu` ein (damit der Code `#include <menu_state.h>` sehen kann) und verbinden Sie den Hook über `extra_scripts`:

```ini
[env:cabinet]
; ... platform / board / lib_deps aus Kapitel 4 – ohne Änderungen ...

build_flags =
    -Isrc/menu                      ; ← hinzugefügt: Pfad zum generierten Menu
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← hinzugefügt
    pre:extra_scripts/pre_gen_menu.py
```

Der Hook findet den Generator selbst unter `lib/idryer-core/menu/menu_gen.py`, daher muss die Bibliothek über `lib/` (Symlink oder Kopie) verbunden sein, wie in Kapitel 4 beschrieben.

## Schritt 3. Beschreiben Sie die Schrank-Parameter

Öffnen Sie `src/menu/menu.yaml`. Die Vorlage enthält bereits einen Root-Punkt `root` mit einem `children`-Array und Beispielparametern. Löschen Sie die Beispiele (`my_param`, `my_flag`, `my_mode_group`) und fügen Sie Ihre eigenen in `children` hinzu. Die letzten zwei Punkte – `units_count` und `language` – lassen Sie an Ort und Stelle: Dies ist ein fester Vertrag mit dem Portal.

Für einen Basis-Schrank reichen einige wenige Parameter.

Zieltemperatur für die Lagerung:

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # Beschriftung aus dem Kernvertrag
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # NVS-Schlüssel (≤ 15 Zeichen)
  persist: true
  scope: global
  default: 45
```

Hysterese (um wie viele Grad die Temperatur unter dem Sollwert abfallen kann, bevor die Heizung wieder anspringt):

```yaml
- id: hysteresis
  type: value
  title: { ru: "ГИСТЕРЕЗИС", en: "HYSTERESIS" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint8
  min: 1
  max: 5
  step: 1
  bind: hysteresis
  persist: true
  scope: global
  default: 2
```

!!! note "role: – eine abgeschlossene Liste"
    Der Wert `role:` kann nicht willkürlich erfunden werden – er muss aus der Liste `canonical_roles` des Kernvertrags stammen. Wenn keine passende Rolle existiert, stoppt der Build und zeigt die zulässigen Optionen an. Für einen Lagerungsschrank sind die Rollen der Familie `storage.*` geeignet: `storage.target_temperature`, `storage.target_humidity`, `storage.start`, `storage.stop`. Die vollständige Liste steht in der Kopfzeile von `menu.template.yaml`. `role:` ist optional: ein Parameter ohne sie (wie die Hysterese oben) wird genauso gespeichert und veröffentlicht, nur die Beschriftung kommt aus `title`.

Einschränkungen, die nicht verletzt werden dürfen:

- `bind` – nicht länger als 15 Zeichen (NVS-Schlüssellimit);
- fügen Sie kein Feld `widget:` in `menu.yaml` ein — Portal und App lesen es nicht: ein Menüpunkt wird nach seinem Typ gezeichnet.

!!! warning "Überprüfen Sie den Punkt ignore_external_cmd aus der Vorlage"
    In der Vorlage gibt es einen Punkt `ignore_external_cmd`, und sein `bind` beträgt 19 Zeichen, was das Limit von 15 überschreitet. Wenn Sie es so lassen, schlägt die Generierung fehl: `bind 'ignore_external_cmd' ... hat 19 Zeichen, Limit 15`. Entweder löschen Sie diesen Punkt oder verkürzen Sie `bind` auf `ign_ext_cmd` (wie in echten Produkten). Für einen Basis-Schrank können Sie ihn einfach löschen.

## Schritt 4. Erstellen Sie das Projekt und überprüfen Sie die Generierung

```bash
pio run -e cabinet
```

Beim Build wird der Pre-Hook Abhängigkeiten selbst installieren (einmalig) und C++-Menu-Dateien generieren. Wenn `menu.yaml` sich nicht geändert hat – wird die Generierung übersprungen (`up-to-date`).

Überprüfen Sie, dass die Generierung erfolgreich war. Im Build-Log erscheint eine Zeile über die Menu-Generierung, und im Ordner `src/menu/` befinden sich die generierten Dateien:

```text
src/menu/
├── menu.yaml          # Ihre Datei (Quelle)
├── menu_state.h/.cpp  # Menü-Objekt mit allen Parametern
├── menu_bindings.*    # Zugriff per bind + Speicherung in NVS
├── menu_ids.h
└── menu_meta.h        # und weitere
```

Wenn der Build mit einer Nachricht über eine unbekannte `role:` fehlschlägt – bedeutet das, dass die Rolle nicht aus der Liste `canonical_roles` stammt. Korrigieren Sie sie und bauen Sie neu auf. Bearbeiten Sie Dateien mit der Kennzeichnung autogen nicht von Hand.

## Schritt 5. Menü beim Start laden

Binden Sie das generierte Menü in `src/main.cpp` ein und laden Sie es in `setup()` — **vor** `s_link.begin()`:

```cpp
#include <menu_state.h>      // Menü-Objekt mit allen Parametern
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // Standardwerte aus YAML setzen
menu.loadFromNVS();          // gespeicherte Werte; beim ersten Start werden die Standardwerte gespeichert
menu_sync_state_to_cache();  // Werte in den Cache, aus dem das veröffentlichte Menü gebaut wird
```

Danach sind die Parameter über das globale Objekt `menu` erreichbar:

```cpp
uint16_t target = menu.target_temp;   // direkter Zugriff auf den Wert
```

## Schritt 6. Das Menü im Portal: veröffentlichen und Änderungen annehmen

Das Portal liest das Menü nicht selbst vom Gerät: die Firmware veröffentlicht es und übernimmt die Änderungen, die zurückkommen. Drei Teile:

- **veröffentlichen** — `menu_buildFullJson()` aus dem Core baut das Menü-JSON aus `menu.yaml` und den aktuellen Werten; `devicePublisher()->publishConfigRaw()` sendet es ans Portal (MQTT-Topic `config`) und über das lokale Netz an die App;
- **wann** — wenn das Gerät online geht und auf den Befehl `get_config`: das Portal sendet ihn, wenn Sie das Gerätemenü öffnen (Zahnrad auf der Karte);
- **ändern** — das Portal sendet `set` mit der `id` des Punkts und dem neuen Wert `val`. `menu_apply_by_bind()` schreibt den Wert in `menu`, in den NVS und in den Cache, dann wird das Menü erneut veröffentlicht und das Portal zeigt den bestätigten Wert.

Fügen Sie nach den Includes hinzu:

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // Menü aus loop() veröffentlichen

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
        if (v < m.min_val) v = m.min_val;              // Grenzen aus menu.yaml
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + Cache
        s_menuPending = true;                          // neuen Wert im Portal zeigen
        return;
    }
}
```

In `setup()`, nach `s_link.begin()`:

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

In `loop()`, nach `s_link.loop()`:

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // gerade online gegangen
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "Warum das Menü aus loop() veröffentlicht wird"
    Befehls-Callbacks werden tief im Netzwerk-Handler aufgerufen. Das Menü-JSON dort zu bauen kostet viel Stack, deshalb setzt der Callback nur ein Flag und `loop()` veröffentlicht.

`applySet()` begrenzt den Wert auf `min`/`max` des Punkts aus `menu.yaml`: einer eingehenden Zahl traut das Gerät nicht blind.

## Vollständiger `src/main.cpp` nach diesem Kapitel

Gegenüber dem vorigen Kapitel sind die mit `// ← Kapitel 6` markierten Zeilen hinzugekommen: Menü laden, veröffentlichen und Änderungen annehmen.

??? note "Was war – `src/main.cpp` nach Kapitel 5"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"

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

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        s_link.begin();
        // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();

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

## Ergebnis überprüfen

Nach der Firmware:

- das Zahnrad auf der Gerätekarte öffnet die Geräteseite mit dem Menü: die Zieltemperatur (das Portal beschriftet sie nach ihrer Rolle — „Storage temperature“) und **HYSTERESIS**;
- ändern Sie dort einen Wert — das Gerät übernimmt ihn, speichert ihn im NVS und veröffentlicht das Menü erneut, das Portal zeigt den bestätigten Wert;
- nach einem Neustart veröffentlicht das Gerät die gespeicherten Werte;
- interne Parameter (Hysterese) sind im Code über `menu` erreichbar.

## Was kommt als Nächstes

Die Einstellungen sind beschrieben und gespeichert. Jetzt verbinden wir sie mit der Hardware in [Heizungssteuerung](07-heating-control.md): Die Heizung hält die Zieltemperatur, der Ventilator schaltet sich basierend auf dem Schwellwert ein.
