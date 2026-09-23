---
title: "Filtre d'air intelligent : composition du système (BOM)"
description: "Liste des composants du filtre d'air : ESP32-C3, capteur VOC SGP40, ventilateur 120 mm, filtres HEPA et charbon, clé MOSFET, alimentation."
---

# Composition du système

Liste complète des composants. Les prix sont approximatifs, tout se trouve sur n'importe quel marché.

## Électronique

| Composant | Exemple | Prix | Utilité |
|---|---|---|---|
| Carte ESP32-C3 | ESP32-C3 Super Mini ou similaire | ~$3 | cerveau de l'appareil, Wi-Fi |
| Capteur VOC | SGP40 (module, I2C) | ~$4 | indice de qualité de l'air |
| Ventilateur 120 mm, 12 V | n'importe quel boîtier, de préférence avec roulement fluide | ~$5 | circulation de l'air à travers le filtre |
| Clé MOSFET | module sur AO3400/IRLZ44N ou « module commutateur MOSFET » prêt | ~$1 | allumer le ventilateur depuis GPIO 3,3 V |
| Alimentation 12 V / 1 A | n'importe quelle qualité | ~$4 | alimentation du ventilateur |
| Module abaisseur 12→5 V | mini-360 (buck) | ~$1 | alimentation ESP32 depuis la même alimentation |

Pour le choix des cartes — [Contrôleurs](../02-controllers/00-how-to-choose-controller.md), pour l'alimentation et les modules abaisseurs — [Bases de l'électronique](../01-electronics-basics/01-load-calculation-24v.md).

## Partie filtrante

| Composant | Exemple | Utilité |
|---|---|---|
| Filtre HEPA | cartouche ronde d'un purificateur automobile/domestique | capture les particules |
| Charbon activé | granulés en cassette ou tapis de charbon | absorbe VOC et odeurs |
| Boîtier | imprimé (vous concevez le STL pour votre filtre) ou n'importe quelle boîte adaptée | tient tout ensemble |

!!! note "Ordre des couches"
    L'air doit passer : entrée → HEPA → charbon → ventilateur → sortie. Le ventilateur se place « en extraction », après les filtres : ainsi l'air sale n'est pas aspiré par les fentes du boîtier en contournant le filtre. Ce n'est pas si critique — ce qui compte, c'est le renouvellement du volume d'air par unité de temps : plus le débit du ventilateur (CFM) est élevé, plus ce temps est court.

## Pourquoi SGP40

- I2C, alimentation 3,3 V — se connecte à l'ESP32 avec deux fils de signal ;
- fournit un **indice VOC** 0..500 (100 — « air normal », plus c'est haut, plus c'est sale), ne nécessite pas d'étalonnage ;
- il y a une bibliothèque Adafruit prête à l'emploi.

Alternatives :

- **ENS160** — indice VOC + estimation eCO2, aussi I2C. Une bonne option « deux en un » ;
- **MH-Z19B/C** — vrai capteur NDIR CO2 (ppm), UART, ~$20. Excessif pour un filtre.

## Outils

Fer à souder, flux, étain, multimètre, gaine thermorétractable. En détail — [Outils](../05-tools/02-multimeter.md).
