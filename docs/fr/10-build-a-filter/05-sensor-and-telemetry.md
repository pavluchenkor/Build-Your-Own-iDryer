---
title: "Filtre d'air intelligent : capteur VOC et télémétrie"
description: "Lecture SGP40 via I2C et publication du champ vocIndex personnalisé en télémétrie iDryer via callback onTelemetryPublish."
---

# Capteur et télémétrie

Dans ce chapitre, le filtre commence à mesurer l'air et envoyer les données au cloud. La technique clé — **votre propre champ en télémétrie** : le dictionnaire de l'écosystème ne sait rien sur VOC, mais le noyau vous permet d'ajouter n'importe quel champ à la télémétrie.

## 1. Bibliothèque du capteur

Dans `platformio.ini` ajoutez à `lib_deps` :

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. Lecture SGP40

Dans `src/main.cpp` (broches — du [schéma](03-wiring.md)) :

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = pas encore de données

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() gère sa propre compensation interne du capteur.
    // Indice : ~100 = air normal, plus haut = plus sale (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
}
```

Ajoutez l'appel `initVocSensor()` dans `setup()` après `s_link.begin()`, et `readVocSensor()` dans `loop()` une fois par seconde (via un minuteur millis, pas via `delay`!) :

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

!!! warning "Pas de delay() dans loop"
    `s_link.loop()` doit être appelé constamment — il gère Wi-Fi, MQTT et les commandes du portail. `delay(1000)` gèlerait tout ça. Utilisez uniquement des minuteurs millis.

## 3. Votre propre champ en télémétrie

Tous les `telemetryPeriodMs`, le noyau rassemble lui-même un message JSON de télémétrie et l'envoie au cloud. Pour notre appareil (un unité, du vocabulaire juste le ventilateur), le noyau rassemble ce message :

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

Décomposons la structure :

- `units` — tableau des **unités** (chambres) de l'appareil. Un sèchoir iDryer en série peut avoir jusqu'à quatre chambres indépendantes, donc la télémétrie est toujours un tableau, même s'il y a une chambre ;
- `units[0]` — première (et notre seule) unité : nous avons défini `unitsCount = 1` dans `Config` ;
- `fanStatus` — champ du dictionnaire, apparu à cause de `hasFan = true` ;
- `rssi`, `uptime` — niveau Wi-Fi et temps de fonctionnement, le noyau les ajoute toujours.

Il n'y a rien sur VOC dans ce message — le noyau ne connaît pas notre capteur. Mais juste avant d'envoyer, le noyau donne à votre code la possibilité d'ajouter ses propres champs au message. Pour cela, vous enregistrez un **callback** (rappel, « appel de retour ») — une fonction que vous donnez au noyau, et le noyau l'appelle lui-même à chaque publication, en passant à l'intérieur le JSON rassemblé (l'argument `doc` — c'est exactement ça).

Dans `setup()` :

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc — c'est exactement ce message de télémétrie rassemblé par le noyau (voir JSON ci-dessus).
    // Nous ajoutons à la première unité notre champ vocIndex.
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

La ligne `doc["units"][0]["vocIndex"] = g_vocIndex;` se lit comme : « dans le message `doc` prends le tableau `units`, dedans l'élément `0` (notre seule unité) et écris-y le champ `vocIndex` ». Vous inventez vous-même le nom du champ — dans [le chapitre suivant](06-card.md), vous vous y référerez pour afficher la valeur sur la fiche.

!!! note "Si vous rencontrez le mot hook"
    Dans le code source du noyau, ce callback s'appelle `PublishHook` — « hook » (« crochet ») signifie la même chose : un point où la bibliothèque vous laisse « accrocher » votre fonction. Les termes sont interchangeables ; dans cette documentation nous disons « callback ».

!!! note "Lambda et pourquoi elle est « vide »"
    La construction `[](JsonObject doc) { ... }` s'appelle une **lambda** — c'est une fonction sans nom, écrite directement au point d'utilisation pour ne pas l'externaliser et ne pas inventer un nom.

    Les crochets au début — « liste de capture » : on y énumère les variables locales que la fonction prend avec elle. Règle du noyau : **les crochets sont toujours vides** (`[]`) — la lambda ne capture rien et ne traîne aucun état avec elle (on appelle ça *stateless*, « sans état »).

    Raison technique : les lambdas avec capture nécessitent de la mémoire dynamique, et sur ESP32 les allocations fréquentes fragmentent le tas et dans le pire cas cassent Wi-Fi. Donc le noyau accepte juste des fonctions simples.

    Conclusion pratique : tout ce que le callback doit faire, stockez-le dans des variables **globales** — comme notre `g_vocIndex`. Cette règle s'applique à tous les callbacks de `idryer-core`.

L'état du ventilateur se publie par la voie du dictionnaire — écrivez juste dans le champ du noyau quand vous allumez/éteignez (logique — [chapitre 7](07-auto-logic.md)) :

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. Pas de capteur sous la main ? Mode démo

On peut parcourir tout le chemin jusqu'à la fiche même sans SGP40 : l'indice sera calculé par un modèle d'air. Copiez le fichier `demo_voc.h` de l'exemple :

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

et ajoutez à `platformio.ini` le drapeau de compilation :

```ini
build_flags =
    -DDEMO_VOC=1
```

Les deux branches sont cachées derrière une seule fonction, et `loop()` ne sait pas d'où vient la valeur :

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

Le modèle se comporte comme une pièce dans laquelle une imprimante travaille : tant que le ventilateur est arrêté, l'indice monte depuis une valeur de fond autour de `100`, et quand le ventilateur tourne il redescend. Le modèle prend l'état du ventilateur dans votre propre logique, donc le seuil et l'hystérésis du [chapitre 7](07-auto-logic.md) se voient à l'œuvre.

Pour un appareil réel on ne met pas ce drapeau : c'est alors la branche avec le vrai capteur qui est compilée.

## 5. Vérification

Après flashage, dans le flux MQTT de l'appareil (ou dans le log Serial des publications), la télémétrie ressemble à ceci :

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` — votre propre champ, parti au cloud à côté du `fanStatus` du dictionnaire. Le portail le reçoit déjà, mais ne sait pas encore quoi en faire : montrez-lui ça dans le chapitre suivant.

!!! note "Votre champ vit en temps réel"
    Le portail distribue la télémétrie aux clients telle quelle, donc la fiche affiche `vocIndex` tout de suite. En revanche, dans l'historique pour les graphiques, le serveur n'écrit que les grandeurs du dictionnaire — température, humidité, chauffage. Votre champ ne sera pas sur le graphique de télémétrie ; s'il vous faut un historique, collectez-le chez vous (par exemple Home Assistant ou votre propre base).

Soufflez sur le capteur ou approchez un marqueur — l'indice devrait augmenter notablement en quelques secondes.
