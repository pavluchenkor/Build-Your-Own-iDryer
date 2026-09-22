---
title: "Filtre d'air intelligent : concept et bénéfices du portail"
description: "Exemple complet n°2 : filtre d'air pour zone d'impression 3D sur ESP32 et idryer-core — capteur VOC personnalisé, gestion autonome et fiche automatique sur le portail via manifeste card."
---

# Filtre d'air intelligent : concept

Ceci est le deuxième exemple complet de la section « Construis toi-même ». Dans le [premier exemple](../09-build-a-device/01-concept.md), vous assembliez une armoire chauffée à partir de « briques de dictionnaire » de l'écosystème : température, humidité, chauffage. Ici, nous franchissons une étape supplémentaire — nous construisons un appareil **qui n'existe pas du tout dans l'écosystème iDryer** : un filtre d'air avec capteur de composés organiques volatiles (VOC).

## Idée principale : c'est un concept, pas une recette pour un seul appareil

Lisez attentivement ce paragraphe — il est plus important que tout le reste du chapitre.

Le filtre ici est simplement un exemple. L'approche présentée fonctionne pour **n'importe quel appareil que vous imaginez** : humidificateur, station de soufflage, contrôleur d'extraction, moniteur de stockage de filament, peu importe. Vous déclarez dans le firmware les capteurs et organes de commande de votre appareil — une ou deux lignes de code pour chacun — et l'appareil **apparaît automatiquement sur le portail et dans l'application mobile** avec une fiche prête : lectures en direct, boutons, champs de saisie. Pas une seule ligne de code côté portail, pas d'accords avec l'équipe iDryer, pas de pull-requests.

Cela fonctionne grâce au mécanisme des **fiches dynamiques** (entity manifest) : l'appareil publie une description lisible par machine « quoi afficher et comment commander », et le portail et l'application construisent l'interface selon cette description. À quoi cela ressemble dans le code — [chapitre sur la fiche](06-card.md).

!!! note "Que signifie cela en pratique"
    Vous avez une idée d'appareil → l'assemblez sur ESP32 → décrivez les capteurs et boutons dans le firmware → associez-le à votre compte dans l'application. C'est tout : l'appareil a une interface sur le portail et dans l'application. La distance de l'idée à « je contrôle depuis mon téléphone » — une soirée.

## Exactement ce que nous construisons

**Filtre d'air pour zone d'impression 3D** : une boîte avec ventilateur, filtre HEPA et couche de charbon activé, qui :

- mesure la qualité de l'air avec un capteur VOC (SGP40) ;
- allume le ventilateur automatiquement quand l'air est sale, l'éteint quand il s'est purifié ;
- affiche l'indice VOC et l'état du ventilateur sur le portail ;
- permet de sélectionner le mode depuis le portail (`auto` / `on` / `off`) et configurer le seuil de déclenchement.

L'ABS et l'ASA sentent le styrène lors de l'impression, les résines ont leur propre bouquet. Un filtre près de l'imprimante n'est pas un luxe, c'est l'hygiène.

## Pourquoi c'est le premier projet idéal

Si l'armoire de la section 09 vous a semblé complexe — commencez par le filtre :

- **pas de radiateur** — donc pas de système de puissance, pas de fusibles thermiques ni de risques ;
- nombre minimal de composants : carte, capteur, ventilateur, transistor ;
- budget environ `$15` sans boîtier ;
- en cas d'erreur de code, le pire qui puisse arriver est que le ventilateur ne s'allume pas.

## Limites du projet

Soyons honnêtes sur ce que ce filtre **n'est pas** :

- ce n'est pas une extraction : l'air est recirculé à travers le filtre, pas expulsé à l'extérieur ;
- ce n'est pas un équipement médical : SGP40 affiche un **indice** relatif de qualité de l'air, pas la concentration d'un gaz spécifique en ppm ;
- le filtre ne remplace pas l'aération.

!!! note "VOC ou CO2?"
    Pour les vapeurs d'impression, le bon capteur est VOC : il réagit aux composés organiques (styrène, solvants). Les capteurs CO2 (par exemple, le capteur NDIR MH-Z19) mesurent le dioxyde de carbone — c'est un indicateur d'air vicié, pas de pollution d'impression. Si vous voulez les deux, ENS160 donne l'indice VOC et une estimation eCO2 simultanément ; l'approche de cette section ne change pas — simplement une ligne supplémentaire dans le manifeste de la fiche.

## Parcours de la section

1. [Composition du système](02-bom.md) — quoi acheter.
2. [Schéma de câblage](03-wiring.md) — comment connecter.
3. [Démarrage du firmware](04-firmware-start.md) — structure sur `idryer-core`, attachement au portail.
4. [Capteur et télémétrie](05-sensor-and-telemetry.md) — lire VOC et envoyer au cloud.
5. [Fiche de l'appareil](06-card.md) — déclarer les capteurs et la gestion, obtenir l'interface.
6. [Logique d'automatisation](07-auto-logic.md) — seuil, hystérésis, mode manuel depuis le portail.
7. [Assemblage et vérification](08-assembly-and-check.md) — checklist final.
