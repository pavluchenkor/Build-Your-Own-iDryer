---
title: "Intelligenter Filter: Gerätekarte im Portal (Card-Manifest)"
description: "Dynamische Gerätekarte: VOC-Sensor, Modus und Schwelle über link.card() deklarieren – Portal und App bauen die Benutzeroberfläche automatisch."
---

# Gerätekarte

Dies ist das Haupt-Kapitel des Abschnitts. Hier bekommt das Gerät eine Benutzeroberfläche im Portal und in der mobilen App – **ohne eine Codezeile auf deren Seite**.

## Wie das funktioniert

Das Gerät veröffentlicht ein **Card-Manifest** – eine maschinell lesbare Beschreibung „was anzeigen und steuern". Portal und App lesen das Manifest und bauen die Karte: Sensoren werden zu Zellen mit Live-Werten, Steuerelemente – zu Tasten, Eingabefeldern und Listen. Die Layoutbeschreibung kann auch aus der Firmware vorgegeben werden.

Sie müssen nichts manuell veröffentlichen: Sie deklarieren Entities durch `link.card()`, und der Kern sammelt das Manifest selbst und sendet es bei Verbindung.

## 1. Entities deklarieren

Alle Deklarationen werden in `setup()` nach `s_link.begin()` gemacht. Unser Filter hat drei Entities: VOC-Messwert, Modus-Liste und Schwellen-Feld. Zerlegen wir jede einzeln und bauen sie am Ende zusammen.

### Allgemeines Prinzip: id und label

Jede Entity hat zwei Namen, verwechseln Sie sie nicht:

- **id** – intern, maschinell (`"voc"`, `"mode"`). Latein, Ziffern, Unterstrich, keine Leerzeichen. Nach id erkennt die Layoutbeschreibung, Befehle und Portal die Entity untereinander. Einmal erfunden – ändern Sie nicht;
- **label** – Beschriftung für Mensch (`"VOC index"`, `"Mode"`). Was Sie schreiben, sieht der Nutzer auf der Karte. Frei änderbar.

### Sensor: VOC-Messwert

```cpp
s_link.card().sensor(
    "voc",              // id: interner Name der Entity
    "VOC index",        // label: Beschriftung auf der Karte
    "",                 // unit: Maßeinheit rechts der Zahl ("°C", "%", "g");
                        //   VOC-Index hat keine Einheit – leerer String
    "units[0].vocIndex" // path: Woher den Wert nehmen – Pfad in Telemetrie-JSON.
                        //   Das ist GENAU das Feld, das wir in Kapitel 5 hinzufügten:
                        //   doc["units"][0]["vocIndex"]. Namen müssen exakt
                        //   übereinstimmen, sonst Strich auf der Karte.
);
```

Sensor – das ist Zelle „nur Lesezugriff": Das Portal nimmt den Wert aus Telemetrie über `path` und zeigt ihn. Es gibt keinen Befehl für Sensor.

### Auswahlliste: Betriebsmodus

```cpp
// Varianten der Liste. Der Nutzer sieht sie im Dropdown-Menü.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: interner Name der Entity
    "Mode",                  // label: Beschriftung auf der Karte
    kModes,                  // options: Array von Varianten (oben deklariert)
    3,                       // Anzahl Varianten im Array – auto, on, off = drei.
                             //   C++ weiß selbst nicht die Array-Länge, wir sagen sie
    [](const char* opt) {    // Callback: Funktion, die der Kern aufruft, wenn
                             //   der Nutzer eine Variante im Portal wählt.
                             //   opt – gewählter String, z.B. "on"
        onModeSelected(opt); //   übergeben an unsere Logik (schreiben in Kapitel 7)
    }
);
```

Hier erscheint die zweite Hälfte des Mechanismus: **Steuerung**. Wenn der Nutzer eine Variante im Portal wählt, bekommt das Gerät einen Befehl, der Kern fängt ihn auf, überprüft (fremde Strings, die nicht in `options` sind, erreichen Sie nicht) und ruft Ihren Callback mit gewähltem Wert auf. MQTT-Nachrichten manuell parsen ist nicht nötig – Ihre Verantwortung beginnt innen in `onModeSelected`.

### Zahlenfeld: Auslöseschwelle

```cpp
s_link.card().number(
    "threshold",       // id: interner Name der Entity
    "VOC threshold",   // label: Beschriftung auf der Karte
    100,               // min: weniger kann das Portal nicht eingeben
    400,               // max: mehr auch nicht; der Kern schneidet zusätzlich
                       //   auf diese Grenzen auf seiner Seite ab
    10,                // step: Schrittweite beim Ändern mit Pfeilen
    "",                // unit: Maßeinheit; beim Index keine
    [](float v) {              // Callback: aufgerufen, wenn Nutzer
                               //   neuen Wert sendet; v – Zahl in Grenzen min..max
        onThresholdChanged(v); //   übergeben an unsere Logik (schreiben in Kapitel 7)
    }
);
```

### Zusammenbau

Endgültige Form des Blocks in `setup()` – das, was in Ihrem Code bleiben sollte. Funktionen `onModeSelected` und `onThresholdChanged` schreiben wir in Kapitel 7; damit der Code jetzt schon kompiliert, deklarieren Sie sie als Stubs **über** `setup()`:

```cpp
// Stubs: echte Implementierung in Kapitel 7 (Automatik-Logik).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetrie: eigenes Feld vocIndex (Kapitel 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Karte: Sensor + zwei Steuerelemente.
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

Und der Lüfter? Ihn zu deklarieren **ist nicht nötig**: Das Flag `hasFan = true` in `Config` hat die „Lüfter"-Zelle ins Manifest automatisch hinzugefügt – das ist Wörterbuch-Fähigkeit, der Kern weiß alles selbst.

!!! note "Eckige Klammern in Callbacks – immer leer"
    `[](const char* opt) { ... }` – das ist Lambda, eine unbenannte Funktion; wir zerlegen sie im [Hinweis von Kapitel 5](05-sensor-and-telemetry.md). Kernregel: Erfassungs-Klammern sind immer leer (`[]`), nichts wird in die Lambda „mitgenommen", alles nötige ist in globalen Variablen – wie `g_mode` und `g_threshold` aus nächstem Kapitel.

## 2. Automatisches Karten-Layout

Das Layout kann man gar nicht vorgeben. Das Portal sammelt die Karte aus deklarierten Entities – und sammelt ordentlich: Messwert-Zellen gruppieren sich in Reihen (bis drei pro Reihe, dann Umbruch), Steuerelemente stehen darunter, jedes auf seiner Zeile, alles im Portal-Design. Für die meisten Geräte reicht das – die Benutzeroberfläche sieht säuberlich aus ohne eine Gedanke zur Gestaltung.

Reihenfolge der Entities auf der Karte – Reihenfolge ihrer Deklaration in `setup()`.

## 3. Eigenes Karten-Layout (optional)

Zuerst — wie die Karte aufgebaut ist. Eine Karte ist ein vertikaler Stapel von **Reihen**. Eine Reihe von Sensorzellen teilt die Breite gleichmäßig: eine Zelle nimmt die ganze Breite, zwei je die Hälfte, drei je ein Drittel. Bedienelemente (Liste, Feld, Schaltfläche) in einer Reihe stehen untereinander in voller Breite.

Das automatische Layout aus vorherigem Abschnitt verteilt Entities auf diese Reihen selbst. Wenn Sie selbst entscheiden wollen, was mit was in einer Reihe steht – vorgeben Sie Reihen mit `layoutRow`-Aufrufen. Ein Aufruf = eine Reihe, Reihenfolge der Aufrufe = Reihenfolge sichtbar oben nach unten:

```cpp
// Reihe 1: zwei Zellen – VOC-Index und Lüfter, jede je Hälfte Breite.
s_link.card().layoutRow("voc", "fan");

// Reihe 2: zwei Bedienelemente — Modus und Schwelle, untereinander.
s_link.card().layoutRow("mode", "threshold");
```

In `layoutRow` übergeben Sie **ids** der Entities – jene internen Namen, die Sie ihnen gegeben haben (darum brauchte id sich). `"fan"` – id der Wörterbuch-Lüfter-Entity, ihre schuf das Flag `hasFan`.

Auf der Karte gibt das folgendes Layout:

```text
┌─ DIY Air Filter ────────────────┐
│  VOC index        │  Lüfter │   ← Reihe 1: voc, fan
│  103              │  Aus     │
├───────────────────┴─────────┤
│  Mode                           │   ← Reihe 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [150                         ] │
└─────────────────────────────────┘
```

Entities, die Sie in keiner Reihe erwähnen, verschwinden nicht – das Portal zeichnet sie darunter automatisch. So können Sie nur das „Hauptsächliche" gestalten, Rest bleibt Automatik.

## 4. Was in die Luft geht

Der Kern veröffentlicht in Topic `idryer/{serial}/card` (retained):

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

Das JSON zu verstehen ist nicht nötig – der Kern generiert es aus Ihren Aufrufen. Aber wissen ist nützlich: Falls Sie Firmware **nicht** auf `idryer-core` schreiben (Rust, MicroPython, was auch immer), genügt es, so ein JSON selbst zu veröffentlichen – das Portal ist allesfressend, nur Format muss stimmen.

## 5. Überprüfung

Flashen Sie und öffnen Sie das Dashboard des Portals:

- Zelle **VOC index** zeigt Live-Index (pusten Sie auf Sensor – Zahl wächst beim nächsten Update);
- Zelle **Lüfter** – An/Aus;
- **Mode** — Dropdown-Liste, **VOC threshold** — Zahlenfeld: der Wert wird etwa 0,6 s nach der Änderung gesendet, ohne Bestätigungsknopf. Liste und Feld beginnen mit der ersten Option und dem Minimum — sie senden Befehle und zeigen nicht die aktuelle Einstellung des Geräts; den Zustand zeigen die Zellen (zum Beispiel der Lüfter).

Modus- und Schwellen-Wahl macht noch nichts – die Callbacks sind Stubs. Wir erwecken sie im [nächsten Kapitel](07-auto-logic.md) zum Leben.

!!! note "Das ist genau das Konzept"
    Beachten Sie, was passierte: Sie beschrieben die Benutzeroberfläche in fünf Zeilen Firmware – und sie erschien im Portal und in der App. Gleicher Trick funktioniert für jedes Ihre Gerät: wechseln sich nur ids, Beschriftungen und Callbacks.

!!! note "Operationen mit Start und Stopp"
    Liste, Feld und Schaltfläche senden einen Wert sofort. Operationen mit Start und Stopp — Trocknen, Heizen, Beleuchtung — werden als **Aktionen** der Karte mit Modus und Startparametern deklariert: die Karte zeigt ein Startformular und, solange die Operation läuft, einen Sitzungsblock mit Stopp-Schaltfläche. Beispiel — [Kapitel 7](../09-build-a-device/07-heating-control.md) im Abschnitt zum Schrank.
