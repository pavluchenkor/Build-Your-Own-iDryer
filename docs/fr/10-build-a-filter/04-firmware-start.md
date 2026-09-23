---
title: "Filtre d'air intelligent : démarrage du firmware et attachement au portail"
description: "Charpente du firmware du filtre sur idryer-core : Config pour type d'appareil non-standard, premier démarrage, association au compte dans l'application."
---

# Démarrage du firmware

La charpente du projet répète complètement [le chapitre de l'exemple avec l'armoire](../09-build-a-device/04-firmware-start.md) : PlatformIO, `idryer-core` dans `lib/`, même `platformio.ini` (changez juste le nom de l'environnement en `filter`). Ici — juste ce qui diffère.

Le projet fini de ce chapitre — [example/10-filter](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/10-filter) dans le dépôt du manuel : c'est de là que viennent `platformio.ini` et tout le code décomposé par parties plus loin. La bibliothèque du noyau — [idryer-core](https://github.com/pavluchenkor/idryer-core).

!!! note "Journal sur le port : deux drapeaux de compilation"
    Sur l'ESP32-C3, la sortie `Serial` part par défaut sur les broches UART0, et non sur le port USB de la carte — le moniteur de port reste vide. Pour voir le journal, `build_flags` a besoin de deux lignes :

    ```ini
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    ```

    Les messages de l'ESP-IDF lui-même (erreurs mDNS et similaires) passent par USB même sans elles, donc « quelque chose s'imprime, mais pas mes lignes » est justement le signe de ces drapeaux manquants.

## Config : appareil de type non-standard

Le filtre n'a ni radiateur ni capteur climatique du dictionnaire de l'écosystème. Du vocabulaire de l'écosystème, il n'a que le ventilateur. Dans `src/main.cpp` :

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // appareil non-standard
    .unitsCount        = 1,
    // Périphérie : du dictionnaire de l'écosystème nous avons juste le ventilateur.
    .hasFan            = true,
    // Périodes de publication automatique :
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identification sur le portail :
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
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

!!! note "DeviceType::Unknown — c'est normal"
    Le type `Unknown` signifie « le portail ne connaît pas ce produit ». Autrefois, c'était un problème : le portail n'avait pas de fiche pour les types inconnus. Maintenant c'est la voie standard : l'interface de l'appareil est entièrement décrite par le manifeste card ([chapitre 6](06-card.md)), et le portail construit la fiche d'après celui-ci. Le type est nécessaire seulement pour les produits « propres » iDryer qui ont des fiches de marque.

Le drapeau `hasFan = true` nous donne gratuitement : le champ `fanStatus` en télémétrie, la cellule « Ventilateur » sur la fiche et une entité dans le manifeste — tout du dictionnaire de l'écosystème.

## Capteur VOC : pas dans Config — et c'est normal

Remarquez : il n'y a pas de drapeau « hasVoc » dans `Config`. Le dictionnaire `has*` décrit la périphérie connue de l'écosystème. Votre capteur personnalisé vous l'ajouterez non pas via le dictionnaire, mais par deux autres mécanismes : en ajoutant ses lectures à la télémétrie via votre propre champ et en le déclarant dans le card-manifeste — ce sont les deux prochains chapitres. C'est ça l'idée : le dictionnaire n'a pas besoin d'être étendu pour chaque nouvel appareil.

## Premier démarrage et attachement

La procédure est la même que pour l'armoire :

1. Flashez la carte et ouvrez le Serial Monitor : tant que l'appareil n'a pas de Wi-Fi, le journal reste muet.
2. Dans l'application iDryer : **Connecter un nouvel appareil** → étape **Wi-Fi** (réseau et mot de passe, **Connecter l’appareil**) → étape **Association** → **Associer**.
3. Après **Appareil associé**, l'appareil est associé à votre compte et passe `Online` sur le portail ; le journal affiche `MQTT: Connected!`.

Détails, erreurs possibles et nouvelle association — dans [le chapitre de l'exemple de l'armoire](../09-build-a-device/04-firmware-start.md).

![Page de l'appareil sur le portail juste après l'association](../../img/10-filter/04-portal-device.png)
*L'appareil sur le portail : nom, état Idle, icône de liaison. Le graphique est vide et le menu n'est pas arrivé — l'appareil n'a rien déclaré à leur sujet.*

Sur le portail, l'appareil est déjà visible, mais la fiche est presque vide — il n'y a pas de données encore. Allons connecter le capteur.
