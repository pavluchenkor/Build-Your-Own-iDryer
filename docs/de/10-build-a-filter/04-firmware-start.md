---
title: "Intelligenter Filter: Firmware-Start und Bindung ans Portal"
description: "Filter-Firmware-Gerüst auf idryer-core: Config für Gerätetyp unbekannt, erste Inbetriebnahme, Kopplung mit dem Konto in der App."
---

# Firmware-Start

Das Projekt-Gerüst wiederholt vollständig [das Kapitel aus dem Gehäuse-Beispiel](../09-build-a-device/04-firmware-start.md): PlatformIO, `idryer-core` in `lib/`, gleiches `platformio.ini` (ersetzen Sie nur die Umgebung auf `filter`). Hier – nur das, was anders ist.

## Config: Gerät unbekannten Typs

Der Filter hat weder einen Heizer noch einen Klima-Sensor aus dem Ökosystem-Wörterbuch. Von „Wörterbuch"-Fähigkeiten hat er nur einen Lüfter. In `src/main.cpp`:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // Gerät unbekannten Typs
    .unitsCount        = 1,
    // Peripherie: vom Ökosystem-Wörterbuch haben wir nur einen Lüfter.
    .hasFan            = true,
    // Telemetrie-Veröffentlichungs-Perioden:
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identifikation im Portal:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

!!! note "DeviceType::Unknown – das ist normal"
    Der Typ `Unknown` bedeutet „das Portal kennt diese Geräteart nicht". Früher war das ein Problem: Das Portal hatte keine Karte für unbekannte Typen. Jetzt ist das der Standardweg: Das gesamte Gerät-Interface wird vom Card-Manifest beschrieben ([Kapitel 6](06-card.md)), und das Portal baut die Karte danach. Der Typ wird nur für iDryer-eigene Geräte benötigt, die fertige Karten haben.

Das Flag `hasFan = true` gibt uns kostenlos: das Feld `fanStatus` in der Telemetrie, die „Lüfter"-Zelle auf der Karte und die Entity im Manifest – alles aus dem Ökosystem-Wörterbuch.

## VOC-Sensor ist nicht in Config – und sollte nicht sein

Achten Sie darauf: In `Config` gibt es kein Flag „hasVoc". Das Wörterbuch `has*` beschreibt Peripherie, die das Ökosystem kennt. Ihren eigenen Sensor fügen Sie nicht über das Wörterbuch hinzu, sondern durch zwei andere Mechanismen: Sie schreiben seine Messwerte in die Telemetrie mit eigenem Feld und deklarieren ihn im Card-Manifest – das sind die nächsten zwei Kapitel. Das ist die Essenz des Ansatzes: Das Wörterbuch muss nicht für jedes neue Gerät erweitert werden.

## Erste Inbetriebnahme und Bindung

Der Ablauf ist derselbe wie beim Schrank:

1. Flashen Sie die Platine und öffnen Sie den Serial Monitor: solange das Gerät kein WLAN hat, bleibt das Log still.
2. In der iDryer-App: **Neues Gerät verbinden** → Schritt **WLAN** (Netz und Passwort, **Gerät verbinden**) → Schritt **Kopplung** → **Koppeln**.
3. Nach **Gerät gekoppelt** ist das Gerät mit Ihrem Konto gekoppelt und geht im Portal `Online`; im Log steht `MQTT: Connected!`.

Details, mögliche Fehler und erneutes Koppeln — im [Kapitel des Schrank-Beispiels](../09-build-a-device/04-firmware-start.md).

Das Gerät ist bereits im Portal sichtbar, aber die Karte ist noch fast leer – wir haben ja noch keine Daten. Gehen wir den Sensor anschließen.
