---
title: "Connecter les capteurs SHT31 et thermistance à idryer-core"
description: "Lecture du capteur climatique SHT31 et de la thermistance du radiateur sur ESP32 : remplissage de la télémétrie idryer-core et transmission des données au portail iDryer."
---

# Capteurs

Sur cette page, vous connectez deux capteurs et envoyez leurs données au portail. D'abord le SHT31 (climat du placard), puis la thermistance (température du radiateur). C'est l'étape « récolter les données » avant d'ajouter la logique de contrôle.

Le principe de travail avec le cœur est simple : votre code dans `loop()` écrit les lectures frais dans les champs `s_link.telemetry.*`, et la façade publie automatiquement vers le cloud tous les `telemetryPeriodMs` définis dans `Config`. Aucun appel de publication manuel n'est nécessaire.

## Champs de télémétrie

Pour notre placard, nous utilisons trois champs (l'index `[0]` est la première et unique chambre) :

| Champ | Contient | Flag dans Config |
|-------|----------|------------------|
| `s_link.telemetry.airTempC[0]` | température de l'air, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | humidité de l'air, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | température du radiateur, °C | `hasHeaterTemp` |

Ces trois flags s'activent ici même, dans `Config` (voir le listing complet à la fin du chapitre). Le flag indique au portail et à l'application que l'appareil possède ce capteur : sans lui, la cellule n'apparaît pas sur la carte.

## Règle : le code du capteur ne doit pas bloquer loop()

La façade `idryer-core` gère le Wi-Fi et MQTT dans le même `loop()`. Par conséquent, lors de la lecture des capteurs, vous ne pouvez pas appeler `delay()` — une pause interrompt la session réseau. Le capteur est interrogé par une minuterie et la valeur prête est simplement lue. Les pilotes fournis dans l'écosystème sont déjà organisés de cette façon.

## Étape 1. SHT31 : climat du placard

Vous n'avez pas besoin d'écrire le pilote SHT31 à partir de zéro — la classe prête `Sht31ClimateSensor` se trouve dans l'exemple de ce chapitre, [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet). Elle utilise la bibliothèque `robtillaart/SHT31` et lit le capteur sans blocage.

1. Ajoutez la bibliothèque SHT31 à `lib_deps` dans votre `platformio.ini` :

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Copiez dans votre dossier `src/` les quatre fichiers du pilote :

    ```bash
    git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
    cp ~/byo-idryer/example/09-cabinet/src/{Sht31ClimateSensor.h,Sht31ClimateSensor.cpp,IClimateSensor.h,sensor_reading.h} src/
    ```

3. Connectez le capteur via I2C (voir [Schéma de connexion](03-wiring.md)) et lisez-le dans `src/main.cpp` :

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — broches de votre carte
    s_climateOk = s_climate.begin();  // trouve automatiquement l'adresse 0x44 ou 0x45
    s_link.begin();
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
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
}
```

Le pilote renvoie un instantané des relevés dans la structure `SensorReading` de `sensor_reading.h` :

```cpp
struct SensorReading {
    float    temperature = NAN;   // °C, NAN s'il n'y a pas de valeur
    float    humidity    = NAN;   // % RH, NAN s'il n'y a pas de valeur
    float    pressure    = NAN;   // hPa, pour de futurs capteurs
    uint32_t ts_ms       = 0;     // millis() au moment de la lecture
    bool     ok          = false; // true si la température et l'humidité sont valides
    int      err         = 0;     // code d'erreur, 0 — pas d'erreur
};
```

Après flashage, la température et l'humidité du placard apparaîtront sur le portail — c'est la première rétroaction du périphérique.

## Étape 2. Thermistance : température du radiateur

Je n'ai pas de classe thermistance prête pour ESP32, donc nous écrivons nous-mêmes la lecture directement dans `src/main.cpp`. La thermistance est connectée à une broche ADC via un convertisseur de tension (voir [Schéma de connexion](03-wiring.md)) : le contrôleur mesure la tension au point milieu, ce qui permet de calculer la résistance de la thermistance, puis la température.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // broche ADC
static const float SERIES_R   = 4700.0f;   // résistance du diviseur, Ω
static const float NOMINAL_R  = 100000.0f; // résistance de la thermistance à 25 °C, Ω
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // coefficient B de la fiche technique de la thermistance

// Retourne la température du radiateur en °C.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 sur ESP32
    float v   = (float)raw / 4095.0f;           // fraction de l'échelle complète
    float r   = SERIES_R * (1.0f - v) / v;      // résistance de la thermistance, Ω
    // Équation de Steinhart–Hart sous forme de paramètre B — voir Wikipédia :
    // https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

Dans `loop()`, écrivez le résultat dans la télémétrie près de la lecture du SHT31 :

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "C'est une lecture simplifiée — adaptez les paramètres à votre thermistance"
    Les constantes `NOMINAL_R` et `BETA` dépendent de la thermistance spécifique — prenez-les dans sa fiche technique (thermistance générique courante — Generic 3950, `100 kΩ`). La formule du diviseur correspond au schéma de [Schéma de connexion](03-wiring.md) : thermistance à `3.3V`, résistance à `GND`. Avec une autre disposition, la formule change. L'ADC sur ESP32 est non linéaire, donc pour des mesures précises les lectures sont étalonnées — dans les contrôleurs iDryer en série, une table thermistance est utilisée pour cela (bibliothèque `Thermistor`).

Test thermistance au multimètre — [Vérification de la thermistance](../06-practical-guides/02-checking-thermistor.md).

## Étape 3. Pas de capteurs sous la main ? Le mode démo

Vous pouvez parcourir tout le chemin jusqu'à la carte sans matériel : les relevés seront calculés par un modèle de placard. Copiez le fichier `demo_sensors.h` depuis le même exemple :

```bash
cp ~/byo-idryer/example/09-cabinet/src/demo_sensors.h src/
```

et ajoutez un flag de compilation dans `platformio.ini` :

```ini
build_flags =
    -DDEMO_SENSORS=1
```

Les deux branches sont cachées derrière une seule fonction, et `loop()` ne sait pas d'où viennent les valeurs :

```cpp
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    // lecture du SHT31 et de la thermistance — comme ci-dessus
#endif
}
```

Le modèle se comporte comme un vrai placard : la pièce oscille lentement autour de `24 °C`, un radiateur allumé réchauffe l'air, un radiateur éteint le laisse refroidir, et l'humidité baisse pendant le chauffage. Le modèle lit la puissance de chauffe dans la télémétrie, donc la logique du chapitre [Contrôle du chauffage](07-heating-control.md) voit une réponse et l'hystérésis fonctionne. Les captures d'écran de cette section ont été faites exactement ainsi.

Pour un appareil réel, ce flag n'est pas activé : c'est alors la branche avec les vrais capteurs qui est compilée.

## Fichier `src/main.cpp` complet après ce chapitre

Ci-dessous — tout le fichier au complet. Les nouvelles lignes par rapport au chapitre précédent sont marquées `// ← chapitre 5`; le reste n'a pas changé.

??? note "Ce qui était — `src/main.cpp` après le chapitre 4"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // appareil personnalisé : la carte est construite par le manifeste
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
        // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← chapitre 5
#include <math.h>                  // ← chapitre 5
#include "Sht31ClimateSensor.h"    // ← chapitre 5
#include "demo_sensors.h"    // ← chapitre 5 : relevés sans capteurs (-DDEMO_SENSORS=1)

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // appareil personnalisé : la carte est construite par le manifeste
    .unitsCount        = 1,
    .hasAirTemp        = true,     // ← chapitre 5
    .hasAirHumidity    = true,     // ← chapitre 5
    .hasHeaterTemp     = true,  // ← chapitre 5
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← chapitre 5 : capteur climatique SHT31
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← chapitre 5 : thermistance du radiateur
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

// ← chapitre 5 : capteurs ou, avec -DDEMO_SENSORS=1, modèle du placard
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();
#endif
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // ← chapitre 5  (SDA, SCL — broches de votre carte)
    s_climateOk = s_climate.begin();  // ← chapitre 5
    s_link.begin();
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    readSensors();   // ← chapitre 5
}
```

## Vérification des résultats

![Page de l'appareil sur le portail : relevés et graphique](../../img/09-cabinet/05-portal-device.png)
*Les relevés sur la carte et le graphique de télémétrie. La température du radiateur suit une courbe distincte sur le graphique. Le menu en bas de la page est encore vide — ce sera l'affaire du chapitre suivant.*

Après cette étape, trois valeurs doivent s'afficher sur le portail :

- température de l'air dans le placard ;
- humidité du placard ;
- température du radiateur.

Si les lectures « flottent » ou sont clairement incorrectes :

- vérifiez la masse commune et le câblage (interférences des fils de puissance) — [Erreurs de câblage](../08-common-mistakes/03-wiring-mistakes.md) ;
- vérifiez la valeur de la résistance du diviseur et le type de thermistance ;
- assurez-vous que le SHT31 répond sur I2C (adresse correcte et lignes).

Diagnostic « le capteur affiche des bêtises » — [Vérification de la thermistance](../06-practical-guides/02-checking-thermistor.md) et [Erreurs courantes](../08-common-mistakes/01-overview.md).

## Prochaine étape

Les données des capteurs sont disponibles. Maintenant, décrivons les paramètres du périphérique (température cible, hystérésis) dans [Menu de YAML](06-menu.md) afin qu'ils puissent être modifiés à partir du portail et stockés en mémoire.
