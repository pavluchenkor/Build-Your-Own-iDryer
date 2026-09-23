---
title: "Assemblage du boîtier chauffant et vérification avant démarrage"
description: "Assemblage final du boîtier personnalisé sur ESP32 : montage dans le boîtier, premier préchauffage, étalonnage de la température et liste de contrôle de sécurité avant fonctionnement permanent."
---

# Assemblage et vérification

Sur cette page, vous assemblez le dispositif dans un boîtier, effectuez un premier préchauffage contrôlé et vérifiez que le boîtier fonctionne en toute sécurité. Effectuez les vérifications dans l'ordre et ne laissez pas le dispositif sans surveillance lors du premier démarrage.

## Ordre de montage

1. Fixez l'ESP32 et la section de puissance dans le boîtier de sorte que les zones basse tension et puissance soient séparées.
2. Placez le capteur SHT31 dans le boîtier à l'écart du flux direct du radiateur — sinon il affichera la température du flux d'air et non celle de l'air dans le volume.
3. Fixez la thermistance en contact thermique avec le radiateur.
4. Vérifiez que les câbles ne touchent pas le radiateur et ne pénètrent pas dans le ventilateur.
5. Dans la version B (`220V`), assurez-vous que les câbles secteur sont fixés dans les bornes, l'isolation est intacte et le boîtier est mis à la terre.

Les exigences relatives au boîtier et à la disposition des assemblages — [Conception du boîtier](../07-3d-printing/05-enclosure-design.md).

!!! warning "Pièces imprimées à proximité de la chaleur"
    Le PLA se ramollit à une température qui se trouve facilement près du radiateur. Imprimez les pièces près de la chaleur à partir d'un matériau résistant à la chaleur : ABS, ASA ou PA (nylon). Voir [Matériaux résistants à la chaleur](../07-3d-printing/04-heat-resistant-materials.md) et [Pourquoi le PLA est un choix risqué](../07-3d-printing/06-why-pla-is-risky.md).

## Vérification avant mise sous tension

Testez à l'aide d'un multimètre avant le premier démarrage :

- pas de court-circuit entre l'alimentation et la terre ;
- alimentation des capteurs `3.3V`, pas `5V`;
- terre commune du contrôleur et de l'alimentation puissance ;
- thermistance et résistance de division assemblées correctement ;
- dans la version B — mise à la terre du boîtier et fusible en place.

Comment utiliser un multimètre — [Multimètre](../05-tools/02-multimeter.md).

## Premier démarrage

1. Appliquez la tension uniquement au contrôleur et aux capteurs. Si un seul bloc d'alimentation alimente tout, alimentez le contrôleur séparément : par l'USB ou via un convertisseur abaisseur, et ne connectez pas encore le fil du radiateur.
2. Assurez-vous que le dispositif est en ligne sur le portail et affiche la température et l'humidité.
3. Connectez le radiateur et le ventilateur.
4. Démarrez le mode maintien de la chaleur depuis le portail et observez.

!!! danger "Ne laissez pas le premier préchauffage sans surveillance"
    Lors du premier démarrage, surveillez le dispositif. Assurez-vous que le radiateur s'éteint à l'atteinte de l'objectif et par la protection de la thermistance, plutôt que de chauffer en continu.

Ce qu'il faut observer dans les premières minutes :

- la température de l'air augmente et se stabilise près de l'objectif ;
- la température du radiateur ne dépasse pas le plafond défini ;
- le chauffage s'éteint à l'atteinte de l'objectif et se rallume après refroidissement de la valeur d'hystérésis ;
- le ventilateur fonctionne et ne touche pas les câbles ;
- le contrôleur ne redémarre pas lors de la mise sous tension de la charge.

## Étalonnage

Après le premier préchauffage, comparez les lectures avec un thermomètre séparé dans le boîtier :

- si la température de l'air dans le boîtier diffère de l'objectif — vérifiez le placement de SHT31 (il ne doit pas être dans le flux ou près d'une paroi) ;
- si la température du radiateur semble invraisemblable — vérifiez le type de thermistance et la valeur de la résistance du diviseur ;
- si nécessaire, ajustez la température objectif et l'hystérésis dans le [menu](06-menu.md).

## Si quelque chose ne fonctionne pas

| Symptôme | Où chercher |
|---------|---------------|
| Le contrôleur redémarre sous charge | [Erreurs d'alimentation](../08-common-mistakes/02-power-mistakes.md) |
| Le capteur affiche des absurdités | [Erreurs de câblage](../08-common-mistakes/03-wiring-mistakes.md), [Vérification de la thermistance](../06-practical-guides/02-checking-thermistor.md) |
| Le dispositif ne se connecte pas au Wi-Fi | [Erreurs de contrôleur](../08-common-mistakes/04-controller-mistakes.md) |
| Le radiateur/SSR chauffe beaucoup | [Erreurs de radiateur et SSR](../08-common-mistakes/05-heater-ssr-mistakes.md) |

La séquence de diagnostic générale — [Liste de contrôle de diagnostic](../08-common-mistakes/06-diagnostic-checklist.md).

## Liste de contrôle avant fonctionnement permanent

- [ ] Le dispositif maintient la température objectif et ne chauffe pas en continu.
- [ ] La protection du radiateur par thermistance fonctionne.
- [ ] Les câbles ne touchent pas le radiateur et le ventilateur.
- [ ] Les pièces imprimées près de la chaleur sont résistantes à la chaleur.
- [ ] Dans la version B : boîtier mis à la terre, fusible installé, isolation intacte.
- [ ] Les données du portail correspondent à la température réelle dans le boîtier.

## Résumé

Vous avez assemblé un boîtier de stockage chauffé sur ESP32 et `idryer-core` : le dispositif lit le climat et la température du radiateur, maintient la température définie, protège le radiateur contre la surchauffe et est contrôlé depuis le portail. Ceci est une base complète sur laquelle vous pouvez construire vos propres modules d'écosystème.

Les composants supplémentaires — éclairage, balances, RFID — sont également pris en charge par le noyau ; ils peuvent être ajoutés selon le même schéma : capteur ou périphérique → télémétrie ou commande → affichage sur le portail.
