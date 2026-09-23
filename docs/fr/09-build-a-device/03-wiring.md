---
title: "Schéma de câblage de l'armoire chauffée sur ESP32"
description: "Carte des broches ESP32 pour une armoire maison : SHT31 en I2C, thermistance sur ADC, radiateur et ventilateur via un relais. Séparation des circuits faible et puissance."
---

# Schéma de câblage

Cette page explique comment connecter les composants autour de l'ESP32. D'abord la carte générale des broches, puis la connexion de chaque nœud et les règles de routage de la partie puissance.

!!! warning "Vérifiez d'abord le brochage de votre carte"
    Les numéros de broches ci-dessous sont un exemple. Les différentes cartes ESP32-C3 et ESP32-S3 ont une numérotation et un positionnement différents. Avant le montage, consultez le brochage de votre carte de développement spécifique : pour les DevKit et Super Mini, il est publié par le fabricant et figure dans la description de la carte. Toutes les broches ne peuvent pas être utilisées librement : certaines sont occupées par le chargement, le flash ou l'USB.

## Carte des broches (exemple)

| Nœud | Ligne | Broche ESP32 (exemple) |
|------|-------|----------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Thermistance | signal ADC | GPIO2 |
| Radiateur (relais) | commande | GPIO4 |
| Ventilateur (relais/PWM) | commande | GPIO5 |

L'alimentation des capteurs est `3.3V` et `GND` depuis la carte. La partie puissance est alimentée séparément.

## SHT31 en I2C

Le SHT31 se connecte avec quatre fils :

1. `VCC` du capteur — sur `3.3V` de la carte.
2. `GND` du capteur — sur `GND` de la carte.
3. `SDA` du capteur — sur la broche `SDA` (exemple : GPIO8).
4. `SCL` du capteur — sur la broche `SCL` (exemple : GPIO9).

Les lignes I2C doivent être courtes. Si le capteur est loin de la carte, maintenez les câbles aussi courts que possible et torsadés. La plupart des modules SHT31 disposent déjà de résistances de tirage montées sur le module.

!!! note "Adresse du SHT31"
    Le SHT31 a généralement l'adresse `0x44` (parfois `0x45`). Si le capteur ne répond pas, vérifiez l'adresse et les lignes `SDA`/`SCL`.

## Thermistance sur ADC

La thermistance est insérée dans un diviseur de tension avec une résistance de tirage :

1. Une broche de la thermistance — sur `3.3V`.
2. L'autre broche de la thermistance — au point de connexion avec la résistance `4.7 kΩ` et sur la broche ADC (exemple : GPIO2).
3. L'autre broche de la résistance `4.7 kΩ` — sur `GND`.

Le contrôleur mesure la tension au point milieu du diviseur et en déduit la résistance de la thermistance, puis la température. Le type de thermistance est spécifié dans le firmware (voir [Contrôle du chauffage](07-heating-control.md)).

Pour plus de détails sur la vérification et le montage — [Vérification de la thermistance](../06-practical-guides/02-checking-thermistor.md).

## Radiateur et ventilateur via relais

L'ESP32 commande la charge non pas directement, mais via un relais. Quel relais — dépend de la version de la [Composition du système](02-bom.md).

### Version A (24V/12V) — Module MOSFET

1. L'entrée de signal du module (`PWM`/`SIG`) — sur la broche de commande ESP32 (exemple : GPIO4 pour le radiateur, GPIO5 pour le ventilateur).
2. `GND` du module — sur le `GND` commun avec l'ESP32.
3. L'entrée d'alimentation du module et la charge — sur l'alimentation `24V`.

!!! warning "Masse commune"
    Le `GND` du contrôleur et le `GND` de l'alimentation puissance doivent être connectés. Sans une masse commune, le signal de commande n'a pas de niveau de référence, et le relais fonctionne de façon imprévisible.

La connexion du ventilateur avec commande est détaillée dans [Connexion du ventilateur](../06-practical-guides/01-connecting-fan.md). La logique du relais — [Module MOSFET](../01-electronics-basics/02-mosfet-module.md).

### Version B (220V) — SSR/relais

!!! danger "Avant le montage de la partie réseau"
    Toutes les connexions au réseau doivent être faites sur un appareil complètement hors tension. Le boîtier avec la partie réseau doit avoir une mise à la terre de protection et un fusible. Les câbles réseau doivent avoir une section suffisante et être fixés solidement dans les bornes.

Un SSR a deux côtés. **Commande** — l'entrée basse tension commandée par le contrôleur. **Puissance** — les broches par lesquelles passe la tension réseau de la charge. Les côtés sont isolés l'un de l'autre par un opto-isolateur à l'intérieur du SSR, vous pouvez donc commander le réseau avec un faible signal `3.3V`.

1. L'entrée de commande est généralement marquée `DC+` et `DC-` (parfois `+` et `-`) et est conçue pour `3–32V` de courant continu. Connectez `DC+` à la broche de commande ESP32 (exemple : GPIO4), et `DC-` au `GND` du contrôleur. Une tension `3.3V` depuis la broche ESP32 suffit pour ouvrir le SSR.
2. Les broches de puissance (souvent marquées comme réseau/`AC` et charge/`LOAD`) sont insérées dans l'interruption d'un des câbles réseau du radiateur — de la même manière qu'un interrupteur dans le câble.
3. Le ventilateur est commuté par un SSR ou relais séparé de la même manière.

!!! note "Pourquoi un radiateur pour le SSR"
    Lors de la commutation, le SSR s'échauffe légèrement, et plus le courant de charge est important, plus l'échauffement est important. C'est pourquoi le SSR est boulonné à un radiateur (une plaque métallique pour la dissipation thermique), et le SSR lui-même est surdimensionné en courant — notablement plus que le courant de charge. Quel excédent et quel radiateur sont nécessaires pour votre courant — [Relais à état solide (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Routage : circuits faible et puissance

- Gardez les câbles de signal (capteurs, commande) séparés des câbles de puissance.
- Ne routez pas les câbles de thermistance et I2C le long des câbles de puissance du radiateur — c'est une source de perturbations.
- Dans la version B, séparez physiquement les zones réseau et basse tension à l'intérieur du boîtier.
- Rassemblez toutes les masses de la partie basse tension en un seul point.

Les perturbations du ventilateur et la mauvaise masse sont une cause fréquente de lectures « flottantes » et de redémarrages. Voir [Erreurs de câblage](../08-common-mistakes/03-wiring-mistakes.md).

## À vérifier avant mise sous tension

- L'alimentation des capteurs est `3.3V`, pas `5V`.
- La thermistance et la résistance diviseur sont correctement assemblées, la broche ADC au point milieu.
- Masse commune du contrôleur et de l'alimentation puissance.
- Dans la version B — mise à la terre du boîtier, fusible, bornes fiables, isolation.
- Pas de courts-circuits entre l'alimentation et la masse (testez avec un multimètre).

Test au multimètre — [Multimètre](../05-tools/02-multimeter.md).

## Et ensuite ?

La partie matérielle est assemblée. Allez à [Démarrage du firmware sur le cœur](04-firmware-start.md) : créez un projet et amenez l'appareil à l'état En ligne sur le portail.
