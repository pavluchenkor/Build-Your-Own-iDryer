---
title: "Contrôle du chauffage du cabinet : maintien de la température et ventilateur"
description: "Logique du cabinet chauffé sur idryer-core : maintien de la température cible par hystérésis, protection du radiateur par thermistance, ventilateur et commandes du portail."
---

# Contrôle du chauffage

Cette page vous montre comment connecter les capteurs, les paramètres et la partie puissance dans une logique opérationnelle. Le dispositif maintient une température définie dans le cabinet, protège le radiateur de la surchauffe et répond aux commandes du portail.

La logique s'exécute dans `loop()` à proximité de la maintenance du réseau. Tous les minuteurs et seuils sont non-bloquants, sans `delay()`.

## Ce qui doit se passer

Le comportement du cabinet repose sur trois règles simples :

1. **Maintien de la température.** Si l'air du cabinet est plus froid que la cible d'une quantité d'hystérésis — allumer le chauffage. Une fois la cible atteinte — éteindre.
2. **Protection du radiateur.** La thermistance contrôle le radiateur lui-même. S'il surchauffe au-delà du seuil autorisé — le chauffage s'éteint indépendamment de la température de l'air.
3. **Ventilateur.** Il s'allume pour distribuer la chaleur dans le cabinet, et s'éteint quand le chauffage n'est pas nécessaire.

## Commutateurs du radiateur et du ventilateur

Le contrôleur allume le radiateur et le ventilateur via un commutateur : module MOSFET (version A) ou SSR (version B) — voir [Schéma de câblage](03-wiring.md). Du point de vue du code, c'est simplement une broche GPIO : `HIGH` — allumée, `LOW` — éteinte.

Décrivons un tel commutateur avec une petite structure et créons deux instances — pour le radiateur et le ventilateur. Ajoutez ceci à `src/main.cpp` (avant `setup()`) :

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — contrôle du radiateur
static GpioOutput myFan{5};      // GPIO5 — contrôle du ventilateur
```

Les numéros de broches sont les mêmes que dans le [Schéma de câblage](03-wiring.md). Dans `setup()`, les deux commutateurs doivent être initialisés : `myHeater.begin();` et `myFan.begin();`.

!!! warning "État sûr au démarrage"
    `begin()` met immédiatement `LOW` — le radiateur et le ventilateur sont éteints jusqu'à ce que la logique décide autrement. C'est important : à la mise sous tension, le radiateur ne doit pas se retrouver accidentellement allumé.

## Maintien de la température par hystérésis

Pour un cabinet à `40–45 °C`, une hystérésis simple suffit : le chauffage s'allume et s'éteint autour de la cible. C'est plus simple qu'un PID complet et fonctionne de manière fiable pour un maintien doux de la chaleur.

L'hystérésis vient du menu (`menu.hysteresis`), déjà branché au [chapitre 6](06-menu.md). La température cible est fixée par l'utilisateur au lancement de l'armoire depuis la carte de l'appareil (`s_targetC` ; la carte est branchée plus loin dans ce chapitre). On ne chauffe qu'en mode Storage. Ajoutez l'état et la fonction de décision :

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // cible du lancement en cours, depuis la carte

static void controlLoop() {
    // Chauffer seulement en mode Storage : après Arrêt, l'armoire refroidit.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // depuis la carte
    float hyst   = (float)menu.hysteresis;           // du menu

    if (air < target - hyst) {
        s_heating = true;     // refroidi — chauffer
    } else if (air >= target) {
        s_heating = false;    // cible atteinte — arrêt
    }
}
```

La température cible arrive avec la commande de lancement depuis la carte ; ses limites et sa valeur par défaut sont l'élément `target_temp` du [menu](06-menu.md).

## Protection du radiateur par thermistance

L'air se réchauffe lentement, mais la spirale du radiateur se réchauffe rapidement. Sans contrôle séparé, le radiateur aura le temps de surchauffer avant que l'air n'atteigne la cible. C'est pourquoi la thermistance du radiateur définit une limite stricte.

```cpp
static const float HEATER_MAX_C = 80.0f;   // plafond de température du radiateur

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // thermistance

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // refléter dans la télémétrie
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "Le plafond du radiateur est une protection, pas un réglage climatique"
    `HEATER_MAX_C` limite la température du radiateur lui-même, pas l'air. La valeur dépend de la conception du radiateur et des matériaux du boîtier. Choisissez-la avec une marge en dessous de la température à laquelle les pièces imprimées se déforment — voir [Matériaux thermostables](../07-3d-printing/04-heat-resistant-materials.md).

Pour un chauffage plus fluide au lieu d'un mode tout ou rien, vous pouvez contrôler la puissance via PWM, et le champ `heaterPower01[0]` accepte des valeurs de `0.0` à `1.0`. Pour un cabinet avec maintien doux de la chaleur, la logique simple ci-dessus est généralement suffisante.

## Ventilateur

Le ventilateur distribue la chaleur dans le cabinet. La logique la plus simple consiste à l'allumer avec le chauffage :

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // tourner pendant que nous chauffons
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // refléter dans la télémétrie
}
```

Dans le contrôleur en série, le ventilateur est contrôlé par la température avec des seuils d'allumage et d'extinction séparés (par exemple, allumage à `55 °C`, extinction à `35 °C`), pour qu'il ne vibre pas à la limite. Pour le cabinet, vous pouvez appliquer la même approche, en reliant les seuils aux paramètres du menu.

## Assemblage dans loop()

```cpp
void loop() {
    s_link.loop();          // réseau et publication automatique

    // capteurs (voir l'étape « Capteurs ») :
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // décider de chauffer ou non
    applyHeater();   // appliquer au radiateur + protection
    applyFan();      // appliquer au ventilateur
}
```

Les champs de télémétrie (`heaterPower01`, `fanOn`) sont publiés par la façade elle-même — sur le portail, vous voyez si le dispositif chauffe actuellement et si le ventilateur fonctionne.

## Carte : démarrage et arrêt

Le démarrage et l'arrêt viennent de la carte de l'appareil dans le portail et dans l'application. Le firmware les déclare comme **actions** de la carte : le core les ajoute au card manifest, et le portail et l'application dessinent eux-mêmes le formulaire et les boutons. Inutile d'analyser des commandes dans votre code : le core appelle votre fonction.

Les limites du champ de température et sa valeur par défaut viennent de l'élément de menu `target_temp` (30–50 °C, 45) via le pont `card_menu_bridge.h`. La valeur saisie part avec la commande de démarrage et n'est pas écrite dans le menu. Ajoutez l'en-tête à côté des en-têtes du menu du chapitre 6 :

```cpp
#include <card/card_menu_bridge.h>
```

Callbacks des actions — avant `setup()` :

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // déjà dans 30..50
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

Dans `setup()`, après les commandes du menu du chapitre 6, déclarez les actions. Les valeurs du menu sont déjà dans le cache lu par la carte : `setup()` appelle `menu_sync_state_to_cache()` depuis le chapitre 6.

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` et `"IDLE"` — le mode de l'unité après l'action. Tant que l'armoire est au repos, la carte affiche le formulaire de démarrage ; en mode `STORAGE`, le bloc de session et le bouton Arrêter.
- `MENU_TARGET_TEMP` — l'id de l'élément `target_temp` ; le générateur le place dans `menu_ids.h`.
- `s_link.status.mode[0]` et `targetTempC[0]` montrent l'état actuel de la chambre. Appelez `publishStatusNow()` après chaque changement pour que la carte bascule tout de suite.
- `iDryer::UnitMode::Storage` — mode de maintien doux de la chaleur. C'est le mode principal de l'armoire.
- Modifiez la température de stockage dans le menu de l'appareil sur le portail — la valeur par défaut du champ de la carte suit : le core remarque lui-même le changement du menu et republie le manifeste.

Le core ajoute au card manifest :

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

Dans le portail, la carte de l'armoire au repos reçoit le champ `Temp.` à 45 °C et le bouton `Stockage` ; après le démarrage, le bloc de session avec la cible et le bouton `Arrêter`. Dans l'application, l'accueil montre les mesures et la session en cours ; le démarrage et l'arrêt sont sur la page de l'appareil. Les capteurs, champs et la disposition de la carte sont traités dans le chapitre [Carte de l'appareil](../10-build-a-filter/06-card.md) de la section sur le filtre à air.

!!! warning "Pas de delay() dans les callbacks"
    Les callbacks des actions sont appelés depuis le gestionnaire réseau. Tout blocage à l'intérieur coupe la session MQTT. Changez la cible et le statut, faites le vrai travail dans `loop()`.

## Fichier `src/main.cpp` complet après ce chapitre

C'est le fichier final et complété du dispositif. Les nouvelles lignes par rapport au chapitre précédent sont marquées `// ← chapitre 7`. Ce même fichier se trouve comme exemple prêt à l'emploi dans le dossier `example/09-cabinet/` du dépôt et est compilé par la commande `pio run -e cabinet`.

??? note "Ce qui était — `src/main.cpp` après le chapitre 6"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include <menu_state.h>                      // ← chapitre 6 : paramètres (menu.target_temp …)
    #include <menu_bindings.h>                   // ← chapitre 6 : menu_apply_by_bind
    #include <menu_commands.h>                   // ← chapitre 6 : menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← chapitre 6 : publishConfigRaw

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

    // ← chapitre 6 : menu sur le portail
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
        menu.initDefaults();                     // ← chapitre 6
        menu.loadFromNVS();                      // ← chapitre 6
        menu_sync_state_to_cache();              // ← chapitre 6
        s_link.begin();
        // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← chapitre 6
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← chapitre 6
    }

    void loop() {
        s_link.loop();

        // ← chapitre 6 : publier le menu au passage en ligne et sur demande
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
#include <card/card_menu_bridge.h>        // ← chapitre 7

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

// ← chapitre 7 : commutateurs du radiateur et du ventilateur
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← chapitre 7 : logique de maintien de la température
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

// ← chapitre 7 : actions de la carte
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
    myHeater.begin();              // ← chapitre 7
    myFan.begin();                 // ← chapitre 7
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← chapitre 7
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

    controlLoop();   // ← chapitre 7
    applyHeater();   // ← chapitre 7
    applyFan();      // ← chapitre 7
}
```

## Vérification du résultat

Après cette étape :

- le bouton `Stockage` de la carte de l'appareil met l'armoire en mode Storage avec la température saisie, l'appareil commence à chauffer ;
- la température de l'air remonte jusqu'à la cible et se maintient dans les limites de l'hystérésis ;
- le radiateur ne dépasse pas `HEATER_MAX_C` ;
- le ventilateur et la puissance de chauffage sont visibles dans la télémétrie ;
- le bouton `Arrêter` coupe le chauffage et passe en Idle ; jusqu'au prochain démarrage, l'armoire ne chauffe pas.

## Étape suivante

La logique est prête. Il reste à assembler le dispositif dans le boîtier et le tester sous tension — [Assemblage et vérification](08-assembly-and-check.md).
