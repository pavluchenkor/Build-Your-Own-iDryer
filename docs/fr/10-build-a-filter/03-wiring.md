---
title: "Filtre d'air intelligent : schéma de câblage"
description: "Connexion de SGP40 via I2C et du ventilateur via clé MOSFET à ESP32-C3 : broches, alimentation, erreurs courantes."
---

# Schéma de câblage

Le schéma est simple : capteur sur I2C, ventilateur via clé, alimentation commune 12 V.

```text
Alim 12 V ──┬────────────────────────► Ventilateur (+)
            │                          Ventilateur (−) ◄── MOSFET (drain)
            │                                             MOSFET (source) ─► GND
            │                                             MOSFET (grille) ◄─ GPIO4 ESP32
            │
            └─► buck 12→5 V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Broches

| Signal | Broche ESP32-C3 Super Mini |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| Grille MOSFET (ventilateur) | GPIO4 |

Vous pouvez choisir d'autres broches — mettez à jour les numéros dans le code ([chapitre 5](05-sensor-and-telemetry.md)).

## Règles de connexion

1. **Masse commune.** GND de l'alimentation, ESP32, module MOSFET et capteur doivent être connectés. La moitié des « ça ne marche pas » en bricolage — c'est la masse oubliée.
2. **Capteur — uniquement 3,3 V.** SGP40 ne supporte pas 5 V en alimentation.
3. **Ventilateur — uniquement via clé.** GPIO fournit des milliampères ; le ventilateur en consomme des centaines. La connexion directe grille la broche. Comment fonctionne la clé MOSFET — [Transistors et clés](../01-electronics-basics/02-mosfet-module.md).
4. **Diode de protection externe** pour ventilateur informatique généralement optionnelle : le ventilateur a sa propre électronique de commutation à l'intérieur, et de l'extérieur il ressemble à une charge électronique plutôt qu'à une pure inductance. Mais lors de la commutation de la ligne d'alimentation par la clé (surtout avec PWM), une diode de roue libre en parallèle avec le ventilateur est utile comme protection de la clé contre les pics inductifs — et s'il est déjà dans le module de clé, c'est un plus.

!!! warning "Vérifiez la polarité avant d'allumer"
    Les + et − inversés sur la ligne 12 V détruisent le module buck et souvent la carte. Mesurez avec un multimètre avant la première alimentation.

## Vérification sans firmware

Après assemblage, avant de charger le code principal :

1. Appliquez 12 V — ESP32 devrait se détecter dans le système comme périphérique USB lors de la connexion du câble (ou une LED d'alimentation s'allume).
2. Connectez brièvement la grille MOSFET à 3,3 V via une résistance de 1 kΩ — le ventilateur devrait démarrer.
3. Nous vérifierons le capteur I2C depuis le firmware avec un scanner de bus au [chapitre 5](05-sensor-and-telemetry.md).
