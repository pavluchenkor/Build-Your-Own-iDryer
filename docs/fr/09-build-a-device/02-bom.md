---
title: "Composition du système d'armoire chauffée : composants et deux versions de la partie puissance"
description: "Liste des composants pour un armoire chauffée DIY basée sur ESP32 : capteur SHT31, thermistor, élément chauffant et ventilateur en versions basse tension (24V) et secteur (220V)."
---

# Composition du système

Cette page présente la liste des composants du dispositif et deux variantes de la partie puissance. La partie faible courant (contrôleur et capteurs) est identique dans les deux versions. Seule la commutation du chauffage et du ventilateur diffère.

## Partie faible courant (commune aux deux versions)

| Nœud | Objectif | Remarque |
|------|----------|---------|
| ESP32-C3 ou ESP32-S3 | Contrôleur : logique, Wi-Fi, portail | DevKit ou Super Mini conviennent |
| Capteur SHT31 | Température et humidité de l'air dans l'armoire | Interface I2C |
| Thermistor NTC 100K | Surveillance de la température du chauffage | Par exemple, Generic 3950 |
| Résistance de tirage du thermistor | Diviseur de tension pour ADC | Généralement `4.7 kΩ` |
| Bloc d'alimentation | Alimentation du contrôleur et des périphériques basse tension | Tension selon la version choisie |

Pour cet exemple, le C3 et le S3 sont équivalents : le code, le câblage et l'ordre de montage sont identiques. Le S3 est plus puissant et dispose de plus de mémoire — prenez-le si vous souhaitez plus tard ajouter un écran, un ruban adressable ou votre propre logique lourde.

L'ESP32 a été choisi parce qu'il dispose du Wi-Fi, des interfaces nécessaires (I2C pour le SHT31, ADC pour le thermistor, PWM pour la commande de charge) et est directement supporté par `idryer-core`. Plus de détails — [Contrôleur ESP32](../02-controllers/01-esp32-controller.md).

!!! warning "Logique ESP32 — 3.3V"
    L'ESP32 fonctionne en `3.3V`. N'appliquez pas `5V` sur ses broches. Cela s'applique aux capteurs, modules et adaptateurs. Plus de détails — [Erreurs des contrôleurs](../08-common-mistakes/04-controller-mistakes.md).

## Capteurs

**SHT31** mesure la température et l'humidité de l'air à l'intérieur de l'armoire. C'est la rétroaction principale : elle vous permet de vérifier si le climat défini est maintenu. Il se connecte en I2C (deux lignes : `SDA`, `SCL`). Plus de détails — [Thermistors et capteurs climatiques](../03-common-components/04-thermistors.md).

**Thermistor** mesure la température du chauffage lui-même, et non de l'air. Il est nécessaire pour empêcher le chauffage de surchauffer : l'air se réchauffe lentement, tandis que le chauffage se réchauffe rapidement. Le thermistor est connecté en tant que diviseur de tension sur une broche ADC. [Vérification du thermistor](../06-practical-guides/02-checking-thermistor.md).

!!! note "Pourquoi deux capteurs de chaleur"
    SHT31 indique « quelle est la température dans l'armoire », le thermistor — « le chauffage ne surchauffe-t-il pas ». Le premier définit l'objectif, le second protège contre une défaillance.

## Partie puissance : choisissez une version

Le chauffage et le ventilateur sont une charge contrôlée par le contrôleur. L'ESP32 ne peut pas commuter directement une telle charge : sa broche ne délivre qu'un signal faible de `3.3V`. Entre le contrôleur et la charge, il faut un interrupteur.

Il existe deux versions fondamentalement différentes. Choisissez l'une en fonction du chauffage et du ventilateur que vous utilisez.

### Version A — basse tension (24V ou 12V)

Le chauffage et le ventilateur sont alimentés par `24V` (ou `12V`) en courant continu. C'est une voie plus simple et plus sûre pour l'assemblage autonome.

| Nœud | Composant |
|------|-----------|
| Chauffage | Élément chauffant `12V` ou `24V` (chauffage PTC) |
| Ventilateur | Ventilateur `24V` ou `12V` (2-pin ou 4-pin) |
| Clé de chauffage | Module MOSFET |
| Clé de ventilateur | Module MOSFET (ou 4-pin PWM directement) |
| Bloc d'alimentation | `24V DC` avec marge de puissance |

Le contrôleur commande le module MOSFET avec un signal d'une broche ESP32. Le module commute la charge basse tension. C'est la même logique que dans un contrôleur prêt à l'emploi. Plus de détails — [Module MOSFET](../01-electronics-basics/02-mosfet-module.md).

La puissance du bloc d'alimentation est calculée pour la charge totale avec marge — voir [Calcul du courant de charge 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Version recommandée pour le premier dispositif"
    Si vous assemblez un appareil pour la première fois, commencez par la version A. Il n'y a pas de tension secteur sur la charge ici, et une erreur de montage est moins dangereuse.

### Version B — secteur (110–230V AC)

Le chauffage et le ventilateur sont alimentés par le secteur `110–230V`. C'est ce qu'on fait quand on a besoin d'un chauffage secteur puissant — par exemple, un chauffage prêt à l'emploi avec ventilateur pour armoire. Ici, au lieu d'un module MOSFET, des modules de commutation AC sont utilisés.

| Nœud | Composant |
|------|-----------|
| Chauffage | Chauffage secteur `110–230V AC` |
| Ventilateur | Ventilateur secteur `110–230V AC` |
| Clé de chauffage | Relais à semi-conducteur (SSR) pour AC |
| Clé de ventilateur | SSR ou relais conventionnel pour AC |
| Bloc d'alimentation | `24V`/`5V DC` séparé pour contrôleur et capteurs |
| Protection | Fusible, mise à la terre de protection du boîtier |

!!! danger "La tension secteur est dangereuse"
    La version B fonctionne avec une tension de `110–230V`. Une erreur de montage peut entraîner une électrocution ou un incendie. Avant de commencer l'assemblage, lisez obligatoirement les matériaux de sécurité : [Triac](../01-electronics-basics/03-triac.md), [Relais à semi-conducteur (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Erreurs de chauffage et SSR](../08-common-mistakes/05-heater-ssr-mistakes.md). Si vous n'avez pas d'expérience avec la tension secteur, choisissez la version A.

Le contrôleur et les capteurs de la version B sont toujours alimentés par une source basse tension séparée (`5V`/`24V`). La partie secteur et la partie faible courant doivent être physiquement et électriquement séparées.

## Modules optionnels

Ces nœuds ne sont pas obligatoires pour l'armoire, mais sont supportés par le cœur et peuvent être ajoutés ultérieurement :

- éclairage LED adressable (`hasLed`);
- capteur de poids pour le débit du filament (`hasWeight`);
- tag RFID pour la bobine (`hasRfid`).

L'armoire de base ne les utilise pas — on commence par le minimum.

## Et ensuite

Une fois les composants choisis, passez au [Schéma de connexion](03-wiring.md) : quelle broche ESP32 est responsable de quoi et comment organiser les parties faible courant et puissance.
