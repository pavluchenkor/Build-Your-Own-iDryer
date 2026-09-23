---
title: "Filtre d'air intelligent : logique d'automatisation"
description: "Seuil et hystérésis par indice VOC, modes auto/on/off depuis le portail, sauvegarde des paramètres en NVS et publication de l'état du ventilateur."
---

# Logique d'automatisation

Assemblons tout : le capteur décide, le ventilateur tourne, le portail commande.

## 1. État et paramètres

Au début de `src/main.cpp` :

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // indice VOC de déclenchement
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) — mémoire non-volatile de l'ESP32 : mode et seuil survivent à un redémarrage.

## 2. Commande du ventilateur

Centralisez tout allumage et éteignage dans une seule fonction `setFan`. Elle prend un argument `on` — l'état désiré : `true` = allumer, `false` = éteindre. Plus loin dans le code, on appelera toujours `setFan(true)` / `setFan(false)`, et elle fait toute la routine : tire la broche, mémorise l'état et signale au portail.

```cpp
static void setFan(bool on) {      // on — argument: true = allumer, false = éteindre
    if (g_fanOn == on) return;     // déjà dans l'état désiré — ne faire rien
    g_fanOn = on;                  // mémoriser le nouvel état en variable globale
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // allumer/éteindre physiquement la clé du ventilateur

    // Signaler au noyau l'état : fanOn[0] — champ du dictionnaire en télémétrie
    // (apparu de hasFan = true; [0] — notre seule unité, comme chapitre 5).
    // De là il ira au cloud et à la cellule « Ventilateur » de la fiche.
    s_link.telemetry.fanOn[0] = on;

    // Changement d'état — raison d'envoyer la télémétrie tout de suite, pas attendre la période.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` fait la réponse instantanée : on clique sur le portail — en une seconde la fiche affiche l'état confirmé. Exactement confirmé : le portail iDryer ne « devine » jamais l'état, il affiche ce que l'appareil a vraiment envoyé.

## 3. Automatisation avec hystérésis

Si on allume le ventilateur juste sur le seuil, il va se mettre à vibrer allumé/éteint près du seuil. On soigne ça avec un écart :

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: allumage au seuil, éteignage 20 points au-dessous.
    if (g_vocIndex < 0) return;                      // capteur est muet encore
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

On appelle `tickAutoLogic()` là où on lit le capteur — dans `loop()` par minuteur de seconde. C'est ce même `loop()` du chapitre 5, on y ajoute une ligne. Complet, il ressemble à :

```cpp
void loop() {
    s_link.loop();                        // réseau, télémétrie, commandes — toujours d'abord

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // une fois par seconde:
        s_lastReadMs = now;
        readVocSensor();                  //   lire VOC (chapitre 5)
        tickAutoLogic();                  //   et aussitôt décider du ventilateur
    }
}
```

L'ordre à l'intérieur du bloc de seconde n'est pas du hasard : d'abord lecture du capteur, puis décision selon elle.

## 4. Callbacks depuis le portail

Exactement les fonctions qu'on a promises au [chapitre 6](06-card.md) :

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // appliquer tout de suite, pas attendre le prochain tick
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

Ces fonctions **remplacent** les stubs du chapitre 6 — supprimez les versions vides.

Remarquez ce qu'il n'y a **pas** dans ce code : parsing MQTT, topics, commandes JSON. L'utilisateur a sélectionné `on` dans la liste sur le portail → le noyau a reçu la commande, l'a vérifiée et appelé `onModeSelected("on")`. Toute la mécanique de transport — c'est du noyau.

## 5. setup() final

Reste à ajouter deux choses : chargement des paramètres sauvegardés depuis NVS (au début, pour que la logique travaille avec) et configuration de la broche du ventilateur. Le `setup()` complet après ce chapitre ressemble à :

```cpp
void setup() {
    Serial.begin(115200);

    // Paramètres depuis NVS : ce que l'utilisateur a sélectionné auparavant.
    s_prefs.begin("filter");   // ouvrir l'espace de noms "filter" en NVS
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Deuxièmes arguments getUChar/getInt — valeurs par défaut : reviennent
    // au tout premier démarrage, quand il n'y a encore rien en NVS.

    pinMode(FAN_PIN, OUTPUT);  // broche de la clé du ventilateur — en sortie

    s_link.begin();
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Télémétrie: votre champ vocIndex (chapitre 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Fiche: capteur + organes de commande (chapitre 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Mise en page (chapitre 6, optionnel).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Vérification des scénarios

| Action | Attente |
|---|---|
| Mode `auto`, souffler sur le capteur | VOC croît, au seuil le ventilateur démarre, fiche affiche « Allumé » |
| Air nettoyé | en-dessous du seuil−20 le ventilateur s'éteint seul |
| Mode `on` depuis le portail | ventilateur tourne indépendamment de VOC |
| Mode `off` depuis le portail | ventilateur arrêté, VOC continue d'afficher |
| Redémarrage de la carte | mode et seuil sauvegardés |

Voilà à quoi ça ressemble en vrai. Seuil `150`, l'indice l'a atteint — le ventilateur s'est allumé tout seul :

![L'automatisation a allumé le ventilateur au seuil](../../img/10-filter/07-portal-auto-on.png)
*Mode `auto` : indice `151` pour un seuil `150` — ventilateur allumé.*

![La même chose dans l'application mobile](../../img/10-filter/07-app-auto-on.png)
*L'application affiche le même état : la valeur a monté, le ventilateur tourne.*

Le mode manuel prend le dessus sur l'automatisation : `Mode` → `on`, et le ventilateur tourne même quand l'air est déjà propre :

![Mode manuel : ventilateur allumé alors que l'air est propre](../../img/10-filter/07-portal-mode-on.png)
*Mode `on`, indice `132` — en dessous du seuil, mais le ventilateur tourne : la commande depuis le portail prime sur l'automatisation.*

## 7. Code final : src/main.cpp complet

Tout le code des chapitres 4–7, rassemblé en un seul fichier. Si quelque chose ne correspond pas au vôtre — vérifiez avec ce listing.

```cpp
// ============================================================
// Filtre d'air intelligent sur idryer-core.
// SGP40 (VOC) + ventilateur via MOSFET, mode auto/manuel,
// commande et fiche sur portail via card-manifeste.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>
#include "demo_voc.h"                 // indice sans capteur (-DDEMO_VOC=1)

// ── Broches ──────────────────────────────────────────────────
static const int FAN_PIN = 4;         // grille MOSFET du ventilateur
// SDA=8, SCL=9 — définies dans Wire.begin() ci-dessous

// ── Passeport de l'appareil (chapitre 4) ─────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // appareil non-standard
    .unitsCount        = 1,
    .hasFan            = true,        // seule compétence du dictionnaire
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── État (chapitre 7) ────────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // indice VOC de déclenchement
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: paramètres survivent au redémarrage

// ── Capteur VOC (chapitre 5) ────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = pas encore de données

static void initVocSensor() {
#ifndef DEMO_VOC
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
#endif
}

// Capteur ou, avec -DDEMO_VOC=1, modèle d'air (chapitre 5).
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    // Indice: ~100 = air normal, plus haut = plus sale (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}

// ── Ventilateur (chapitre 7) ────────────────────────────────
static void setFan(bool on) {         // on: true = allumer, false = éteindre
    if (g_fanOn == on) return;        // déjà dans l'état désiré
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // champ du dictionnaire → cloud → fiche
    s_link.publishTelemetryNow();     // changement d'état — publier tout de suite
}

// ── Automatisation avec hystérésis (chapitre 7) ──────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: allumage au seuil, éteignage 20 points au-dessous.
    if (g_vocIndex < 0) return;       // capteur est muet encore
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Callbacks de commandes depuis le portail (chapitres 6–7) ──
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // appliquer tout de suite
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: paramètres, réseau, capteur, fiche ───────────────
void setup() {
    Serial.begin(115200);

    // Paramètres depuis NVS (deuxièmes arguments — défauts du premier démarrage).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, attachement — tout dedans
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Télémétrie: ajout du champ vocIndex personnel (chapitre 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Fiche: capteur + organes de commande (chapitre 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Mise en page usine de la fiche (chapitre 6, optionnel).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: réseau toujours, capteur et logique une fois par seconde ──
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // réseau, télémétrie, commandes — toujours d'abord

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // lecture fraîche…
        tickAutoLogic();              // …et aussitôt décision selon elle
    }
}
```
