---
title: "Filtre d'air intelligent : assemblage et vérification finale"
description: "Assemblage du filtre dans le boîtier, ordre des couches de filtration et checklist complet : capteur, télémétrie, fiche, commandes, automatisation."
---

# Assemblage et vérification

## Assemblage

1. **Boîtier.** Une boîte avec deux ouvertures : entrée d'air et sortie. Vous l'imprimez pour votre cartouche de filtre ou adaptez une prête.
2. **Couches dans le flux d'air :** entrée → HEPA → charbon → ventilateur (en extraction) → sortie. Pas de fentes aux jonctions : l'air est paresseux et ira en contournement du filtre s'il peut.
3. **Capteur** — sur le flux d'entrée, avant les filtres : il doit sentir l'air sale de la pièce, pas l'air épuré.
4. **Électronique** — dans un compartiment séparé ou sur le mur, loin du flux de poussière. Carte sur entretoises, pas « en vrac ».
5. Fixez les câbles : la vibration du ventilateur avec le temps déserrera tout ce qui n'est pas fixé.

## Checklist complet

Vérifiez dans l'ordre — chaque point s'appuie sur les précédents.

| # | Vérification | Comment |
|---|---|---|
| 1 | Alimentation | 12 V sur la ligne du ventilateur, 5 V après buck, 3,3 V sur le capteur |
| 2 | Capteur vivant | dans le log Serial indice ~100 en air pur, croît du souffle |
| 3 | Appareil Online | statut sur le portail après association dans l'application |
| 4 | Télémétrie | `vocIndex` et `fanStatus` dans le flux de l'appareil |
| 5 | Fiche | cellules VOC et Ventilateur, liste Mode, champ Threshold |
| 6 | Commande du portail | Mode → `on`: ventilateur allumé, fiche affiche « Allumé » |
| 7 | Automatisation | Mode → `auto`, respirer fort : allumé au seuil, éteint au-dessous |
| 8 | Redémarrage | mode et seuil sauvegardés, fiche s'est réveillée seule |

## Quoi après

Le filtre est prêt. Après — selon le goût :

- **Plus d'entités** : bouton « soufflage 5 min » (`card().button(...)`), deuxième capteur, compteur de motos du filtre avec rappel de remplacement ;
- **Mise en page jolie** : vous avez déjà vu le `layoutRow` usine ;
- **Vos propres appareils** : toute cette section — c'est un patron. Remplacez capteur, actionneur et logique — et par le même schéma vous assemblerez un humidificateur, une extraction, un contrôleur de n'importe quoi. Le manifeste fera l'interface seul.

Si quelque chose ne marche pas — [Erreurs typiques](../08-common-mistakes/01-power-mistakes.md).
