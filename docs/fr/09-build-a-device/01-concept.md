---
title: "Construire son propre appareil sur le cœur iDryer : concept"
description: "Exemple transversal : comment construire de zéro un armoire de stockage chauffée pour filament avec ESP32 et la bibliothèque idryer-core, connectée au portail iDryer."
---

# Construire son propre appareil : concept

Cette section est un exemple transversal. Les sections précédentes ont expliqué les éléments individuels : alimentation, contrôleurs, capteurs, éléments chauffants, sécurité. Ici, vous assemblez ces briques en un appareil complet et le mettez en état de fonctionnement avec la connexion au [portail iDryer](https://portal.idryer.org/).

L'exemple est basé sur la bibliothèque `idryer-core`. La bibliothèque gère toute l'infrastructure réseau : connexion Wi-Fi, liaison au compte, session MQTT sécurisée, publication périodique de la télémétrie. Vous écrivez uniquement ce qui est spécifique à votre appareil : lecture des capteurs, contrôle du chauffage et du ventilateur, logique de maintien de la température.

## Ce que nous construisons exactement

Nous construisons **une armoire de stockage chauffée pour filament**. C'est une armoire fermée pour 10-40 bobines, dans laquelle une température d'environ `40-45 °C` est maintenue.

Il est important de définir les limites de la tâche dès le départ.

!!! note "Ce n'est pas un séchoir haute température"
    Nous ne prétendons pas à un séchage rapide à haute température. L'objectif de l'appareil est de maintenir dans l'armoire une chaleur douce qui garde le filament sec lors du stockage.

Une température de `40-45 °C` est suffisante pour stocker la plupart des plastiques peu exigeants — du PLA à l'ABS — à l'état sec. Pour le séchage actif des matériaux exigeants (nylon, polycarbonate, PA-CF), des températures plus élevées et une construction différente sont nécessaires — de tels séchoirs sont assemblés séparément, selon les principes des autres sections.

## Pourquoi le faire soi-même

Le contrôleur iDryer fini sait déjà faire tout ce qui est décrit ci-dessous. Cet exemple n'est pas un substitut à celui-ci, mais pour montrer **comment l'appareil est structuré à l'intérieur** et donner une base pour vos propres modules.

L'assemblage indépendant a du sens quand :

- vous avez besoin d'une armoire de taille ou de forme non standard ;
- vous voulez comprendre comment le contrôleur gère le chauffage et communique avec le portail ;
- vous envisagez de créer votre propre module d'écosystème et prenez cet exemple comme point de départ.

## En quoi cela diffère du contrôleur V2

Le contrôleur iDryer V2 en série est dual-processeur : la logique principale s'exécute sur un microcontrôleur séparé, tandis que le module ESP32 fonctionne uniquement comme pont vers Wi-Fi et le portail. C'est justifié pour un produit en série avec écran, balance, RFID et plusieurs caméras.

Pour un armoire faite maison, cette complexité n'est pas nécessaire. Nous simplifions l'architecture à **un seul ESP32**, qui fait tout lui-même :

- lit les capteurs ;
- contrôle le chauffage et le ventilateur ;
- se connecte à Wi-Fi et au portail via `idryer-core`.

Fonctionnellement, nous reproduisons le comportement d'une chambre du contrôleur V2 (capteur climatique, chauffage avec rétroaction par thermistance, ventilateur), mais dans une exécution DIY honnête sur une seule carte.

!!! note "Le servomoteur n'est pas utilisé"
    Dans le contrôleur V2, le servomoteur contrôle le volet d'air de la chambre. Pour une armoire de stockage avec chauffage doux uniforme, le volet n'est pas nécessaire, il n'y a donc pas de servomoteur dans cet exemple.

## Ce que donne la connexion au cœur

Quand l'appareil est assemblé sur `idryer-core` et lié au compte, vous obtenez sans code supplémentaire :

- gestion et surveillance via le [portail](https://portal.idryer.org/) et l'application mobile ;
- graphique de température et d'humidité dans l'armoire ;
- démarrage et arrêt du mode maintien de chaleur à distance ;
- configuration des paramètres (température cible, hystérésis) via le menu de l'appareil.

!!! note "Capteurs et contrôles personnalisés sur la fiche de l'appareil"
    La fiche de l'appareil n'est pas limitée aux capacités du dictionnaire (`has*`). Via le card-manifest, le firmware peut déclarer n'importe quels capteurs et commandes personnalisés — ils apparaîtront automatiquement sur le portail et dans l'application. Comment faire — l'exemple transversal [« Filtre à air intelligent »](../10-build-a-filter/01-concept.md), notamment [le chapitre sur la fiche](../10-build-a-filter/06-card.md). Le démarrage et l'arrêt du maintien en chaleur se déclarent comme actions de la carte au [chapitre 7](07-heating-control.md).

## De quoi se compose cette section

Ensuite vient le chemin étape par étape d'une carte vierge à une armoire qui fonctionne :

1. [Composition du système](02-bom.md) — quels composants prendre et deux versions de la partie puissance (basse tension et réseau).
2. [Schéma de connexion](03-wiring.md) — carte des broches ESP32, séparation des parties faible signal et puissance, sécurité.
3. [Démarrage du firmware sur le cœur](04-firmware-start.md) — projet PlatformIO, premier lancement, liaison au portail.
4. [Capteurs](05-sensors.md) — connexion de SHT31 et thermistance, récupération des données.
5. [Menu en YAML](06-menu.md) — description des paramètres de l'appareil, ils vont dans NVS et au portail.
6. [Contrôle du chauffage](07-heating-control.md) — logique de maintien de la température, ventilateur, démarrage et arrêt depuis la carte de l'appareil.
7. [Assemblage et vérification](08-assembly-and-check.md) — assemblage final, premier réchauffement, liste de vérification de sécurité.

!!! tip "Exemple complet"
    Si vous voulez voir le résultat immédiatement — le projet fini se trouve dans le dossier `example/09-cabinet/` du référentiel et se compile avec la commande `pio run -e cabinet`. Les chapitres ci-dessous analysent ce même code étape par étape.

## Voir aussi

- [Par où commencer](../00-start-here/01-introduction.md) — ordre général de lecture de la section.
- [Contrôleur ESP32](../02-controllers/01-esp32-controller.md) — pourquoi ESP32 est pratique pour un appareil avec Wi-Fi.
- [Composants courants](../03-common-components/01-overview.md) — carte des détails de l'appareil.
