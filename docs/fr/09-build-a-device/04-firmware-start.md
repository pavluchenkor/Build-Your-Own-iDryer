---
title: "Démarrage du firmware sur idryer-core : premier lancement et connexion au portail"
description: "Créer un projet PlatformIO avec la bibliothèque idryer-core : platformio.ini, Config de l'appareil, premier flash de l'ESP32, réglage du Wi-Fi et association de l'appareil au portail iDryer dans l'application."
---

# Démarrage du firmware sur le cœur

Sur cette page, vous créez un projet de firmware, mettez l'ESP32 en état Online sur le portail et vérifiez que la partie réseau fonctionne. Les capteurs et la logique de chauffage seront ajoutés aux étapes suivantes.

L'approche est construite sur la façade `iDryer::Link`. Vous décrivez l'appareil avec une seule structure `iDryer::Config`, appelez `link.begin()` et `link.loop()` — le cœur gère automatiquement toute la connexion au réseau.

## 1. Préparez les outils

Vous aurez besoin de :

- VS Code avec l'extension PlatformIO ;
- Un câble USB ;
- Un réseau Wi-Fi `2.4 GHz` (ESP32 ne fonctionne pas avec les réseaux uniquement `5 GHz`) ;
- un smartphone avec l'application iDryer ([App Store](https://apps.apple.com/app/idryer/id6760609044), [Google Play](https://play.google.com/store/apps/details?id=org.idryer.mobile)), connectée à votre compte du portail iDryer : c'est par elle que l'appareil reçoit le réseau Wi-Fi et est associé au compte ;
- la bibliothèque du cœur [idryer-core](https://github.com/pavluchenkor/idryer-core) ;
- le projet prêt à l'emploi de ce chapitre — [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet) dans le dépôt du manuel : c'est de là que viennent le pilote du capteur et les autres fichiers qu'il est proposé de copier par la suite.

Ce qu'est le firmware du contrôleur et comment il arrive sur la carte — [Flashage du contrôleur](../02-controllers/11-flashing-controller.md).

## 2. Créez un projet

Dans PlatformIO, un projet est un dossier avec une structure fixe. Créez un dossier de projet (par exemple `my-cabinet`) et ouvrez-le dans VS Code. À l'intérieur, vous devez avoir ces fichiers :

```text
my-cabinet/
├── platformio.ini        # paramètres de compilation (remplis à l'étape 4)
├── lib/
│   └── idryer-core/      # bibliothèque du cœur (lien symbolique ou copie)
└── src/
    └── main.cpp          # code de l'appareil : Config + setup() + loop()
```

Tous les fragments de code ci-dessous vont exactement dans ces fichiers — chaque étape indique dans lequel. Créez manuellement les dossiers `include/`, `lib/` et `src/` s'ils n'existent pas.

Mettez la bibliothèque `idryer-core` dans `lib/` — PlatformIO trouve automatiquement les bibliothèques là. La façon la plus simple est de faire un lien symbolique vers la bibliothèque téléchargée :

```bash
git clone https://github.com/pavluchenkor/idryer-core.git ~/idryer-core
ln -s ~/idryer-core lib/idryer-core
```

Au lieu d'un lien symbolique, vous pouvez simplement copier le dossier de la bibliothèque dans `lib/idryer-core` — cela fonctionne pareil.

Ceci est également requis pour la génération du menu (chapitre 6) — le hook cherche le générateur à l'intérieur de `lib/idryer-core/`.

## 3. Le Wi-Fi et l'association ne sont pas dans le code

Le firmware ne contient ni le mot de passe du réseau ni les données du compte. Au premier démarrage, l'appareil n'a pas de Wi-Fi et attend des réglages : l'application iDryer les envoie par les ondes (ESPTouch), puis associe l'appareil à votre compte avec un jeton d'association à usage unique. Le core fait tout cela dans `s_link.begin()` et `s_link.loop()` ; il vous reste à suivre les étapes de l'application — section 9.

**Comment le réseau arrive dans l'appareil.** Une carte sans réseau enregistré écoute les ondes, comme un récepteur non réglé sur une station. Pendant ce temps, le téléphone « tapote » le nom du réseau et le mot de passe dans l'air — à peu près comme en morse, mais avec des paquets Wi-Fi. La carte capte cette émission, rejoint le réseau et s'y connecte ensuite d'elle-même à chaque mise sous tension. Aucune broche ni aucun fil supplémentaire n'est nécessaire : l'antenne d'origine de la carte s'en charge, cela démarre tout seul tant qu'il n'y a pas de réseau et dure jusqu'à 90 secondes.

Si la transmission par les ondes échoue, il existe une voie filaire : l'installateur web [install.idryer.org](https://install.idryer.org) transmet à la carte le réseau et le jeton d'association par l'USB — la même chose que fait l'application, mais par câble. Un simple redémarrage de la carte aide aussi : ensuite, elle attend de nouveau les réglages.

## 4. Configurez platformio.ini

Remplissez `platformio.ini` à la racine du projet :

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; Les bibliothèques du core (MQTT, ArduinoJson, WebSockets, Improv) arrivent
; d'elles-mêmes depuis lib/idryer-core/library.json.
; ESPAsyncTCP est le transport ESP8266 venant des dépendances d'espMqttClient :
; il ne compile pas sur ESP32 et doit être exclu.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Remplacez `board` par votre carte (par exemple, `esp32-s3-devkitc-1`). Pas besoin de spécifier `idryer-core` dans `lib_deps` — elle se trouve dans `lib/` (étape 2).

!!! note "Ce que font ces lignes"
    Inutile de lister les dépendances du core : PlatformIO les prend dans `lib/idryer-core/library.json`. `lib_ignore = ESPAsyncTCP` est obligatoire — sans lui, la compilation échoue dans `ESPAsyncTCP.cpp`. Les flags `MQTT_BROKER` et `MQTT_PORT` sont aussi obligatoires — sans eux le core ne compile pas (`'MQTT_BROKER' was not declared`).

## 5. Décrivez l'appareil dans Config

À partir de maintenant, tout se passe dans un seul fichier — `src/main.cpp`. Ouvrez-le et entrez le code de cette étape et des suivantes.

`iDryer::Config` est le passeport de l'appareil. Les drapeaux `has*` indiquent au portail ce que possède l'appareil et déterminent quels champs de télémétrie sont publiés.

Pour un armoire chauffée, au début de `src/main.cpp`

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
```

!!! note "Les drapeaux has* — c'est un contrat avec le portail"
    Un champ de télémétrie dont le drapeau correspondant `has*` est `false` ne sera pas publié. Par exemple, sans `hasAirHumidity = true`, l'humidité ne sera pas envoyée au cloud, même si vous l'écrivez dans le code. N'activez que ce qui est physiquement présent sur l'appareil.

Liste des composants et drapeaux — [Composition du système](02-bom.md).

## 6. Fonction principale minimale

Dans le même fichier, après le bloc `Config`, ajoutez les fonctions `setup()` et `loop()`. Pour le premier lancement, il suffit d'initialiser le lien et de l'exécuter dans `loop()` :

```cpp
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

`s_link.begin()` démarre le Wi-Fi, l'association et la connexion au portail. La commande `revoke` vient du portail quand l'appareil est dissocié du compte : `handleRevoke()` efface le secret de l'appareil, qui attend alors une nouvelle association. Les capteurs arrivent à l'étape [Capteurs](05-sensors.md).

### `src/main.cpp` complet après ce chapitre

Prenez les deux blocs ci-dessus dans un seul fichier — c'est tout `src/main.cpp` à cette étape :

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

Le chapitre précédent montre **ce qu'il faut ajouter** et le **`src/main.cpp` complet après les modifications**, afin que vous voyiez toujours l'image complète, et non des fragments épars.

## 7. Flashez

```bash
pio run -e cabinet -t upload
```

## 8. Ouvrez le Serial Monitor

```bash
pio device monitor -b 115200
```

Tant que l'appareil n'a pas de Wi-Fi, le journal reste muet : le core réserve le port série à l'installateur web (Improv). Les journaux s'activent dès que le Wi-Fi est en place. Sur un appareil pas encore associé, le journal se termine ainsi :

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

La dernière ligne est exactement ce qu'il faut à cette étape : le réseau est là, il n'y a pas de secret d'association, l'appareil attend le jeton de l'application. Laissez le moniteur ouvert et passez à l'application.

## 9. Connectez le Wi-Fi et associez l'appareil dans l'application

1. Connectez le téléphone au réseau Wi-Fi où fonctionnera l'appareil (`2.4 GHz`) et connectez-vous à l'application iDryer avec votre compte du portail.
2. Sur l'écran d'accueil, touchez **Connecter un nouvel appareil** — l'étape **Wi-Fi** s'ouvre.
3. Vérifiez le nom du réseau (l'application le remplit elle-même si la localisation est activée), saisissez le mot de passe et touchez **Connecter l’appareil**. L'application envoie les réglages pendant 90 secondes au maximum ; quand l'appareil rejoint le réseau, **Appareil connecté** s'affiche. Touchez **Suivant**.
4. À l'étape **Association**, touchez **Associer**. L'application trouve l'appareil sur le réseau, obtient du portail un jeton d'association à usage unique, le remet à l'appareil et attend que le portail confirme que l'appareil est en ligne.
5. Après **Appareil associé**, l'appareil apparaît dans la liste des appareils du portail et de l'application.

Si l'appareil est déjà sur le réseau, ouvrez directement l'étape **Association** — touchez sa puce en haut de la fenêtre.

![Étape Wi-Fi dans l'application : nom du réseau et mot de passe](../../img/09-cabinet/04-app-wifi.png)
*Étape **Wi-Fi** : l'application transmet le réseau à l'appareil par les ondes.*

![Étape d'association : l'application a trouvé l'appareil sur le réseau](../../img/09-cabinet/04-app-pairing.png)
*Étape **Association** : l'application a trouvé l'appareil sur le réseau grâce à son numéro de série. Les appareils appartenant à d'autres sont marqués comme occupés.*

![Message « appareil associé »](../../img/09-cabinet/04-app-paired.png)
*C'est fait : l'appareil est associé au compte et va apparaître dans la liste.*

Le journal montre l'association :

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## Vérification du résultat

À ce stade, l'appareil doit être Online sur le portail. Il n'y a pas encore de données de capteurs — c'est normal : le `Config` n'en a encore rien déclaré, et la carte n'a rien à afficher.

![Carte de l'appareil sur le portail juste après l'association](../../img/09-cabinet/04-portal-card.png)
*L'appareil sur le portail : nom, état Idle, icône de liaison. Pas de relevés — ils apparaîtront au chapitre suivant.*

Le nom `Device DEVICE_…` est celui d'usine. Renommez l'appareil avec le crayon à côté du nom : dans la suite des exemples, il s'appelle « Storage cabinet ».

Si quelque chose s'est mal passé :

- l'application n'a pas vu l'appareil rejoindre le réseau — vérifiez le mot de passe et que le réseau est en `2.4 GHz` ; avec un mauvais mot de passe, l'appareil attend de nouveau les réglages, redémarrez la carte et refaites l'étape Wi-Fi ;
- le réseau ne passe toujours pas par les ondes — faites la même chose par l'USB avec l'installateur web [install.idryer.org](https://install.idryer.org) ;
- à l'étape **Association**, l'application n'a pas trouvé l'appareil — le téléphone et l'appareil doivent être sur le même réseau, et le réseau ne doit pas bloquer la découverte d'appareils (les réseaux invités le font souvent) ;
- l'appareil redémarre — vérifiez l'alimentation de l'ESP32 (les chutes de tension au démarrage sont une cause fréquente de redémarrages) ;
- la compilation échoue avec une erreur — demandez à la communauté : [Telegram](https://t.me/iDryer), [Discord](https://discord.gg/jGce5eeHHz) ;
- voir [Erreurs d'alimentation](../08-common-mistakes/02-power-mistakes.md) et [Erreurs de contrôleur](../08-common-mistakes/04-controller-mistakes.md).

## Prochaines étapes

La partie réseau fonctionne. Passez à [Capteurs](05-sensors.md) : nous allons brancher le SHT31 et la thermistance et voir leurs données sur le portail.
