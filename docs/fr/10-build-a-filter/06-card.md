---
title: "Filtre d'air intelligent : fiche sur le portail (card-manifeste)"
description: "Fiche dynamique de l'appareil : déclarer le capteur VOC, mode, seuil et mise en page via link.card() — portail et app construisent l'interface automatiquement."
---

# Fiche de l'appareil

C'est le chapitre clé de la section. Ici, l'appareil reçoit son interface sur le portail et dans l'application mobile — **sans une seule ligne de code de leur côté**.

## Comment ça marche

L'appareil publie un **card-manifeste** — une description lisible par machine « quoi afficher et comment commander ». Le portail et l'application lisent le manifeste et construisent la fiche : les capteurs deviennent des cellules avec des valeurs en direct, les contrôles deviennent des boutons, des champs de saisie et des listes. La mise en page aussi peut être définie depuis le firmware.

Vous n'avez rien à publier manuellement : vous déclarez les entités via `link.card()`, et le noyau rassemble lui-même le manifeste et l'envoie lors de la connexion.

## 1. Déclarer les entités

Toutes les déclarations se font dans `setup()`, après `s_link.begin()`. Notre filtre a trois entités : la lecture VOC, la liste des modes et le champ seuil. Décomposons chacune séparément, et assemblerons le bloc au final.

### Principe général : id et label

Chaque entité a deux noms, ne les confondez pas :

- **id** — interne, lisible par machine (`"voc"`, `"mode"`). Latin, chiffres, tiret bas, pas d'espaces. Par l'id l'entité se reconnaît dans la mise en page, les commandes et le portail entre eux. Inventé une fois — ne change pas ;
- **label** — signature pour une personne (`"VOC index"`, `"Mode"`). Ce que vous écrivez, c'est ce que l'utilisateur verra sur la fiche. Libre de changer.

### Capteur : lecture VOC

```cpp
s_link.card().sensor(
    "voc",              // id: nom interne de l'entité
    "VOC index",        // label: signature sur la fiche
    "",                 // unit: unité de mesure à droite du nombre ("°C", "%", "g");
                        //   VOC-index n'a pas d'unité — chaîne vide
    "units[0].vocIndex" // path: d'où prendre la valeur — chemin dans le JSON de télémétrie.
                        //   C'est exactement le champ que nous avons ajouté chapitre 5 :
                        //   doc["units"][0]["vocIndex"]. Les noms doivent correspondre
                        //   caractère par caractère, sinon on verra un tiret sur la fiche.
);
```

Un capteur est une cellule « lecture seule » : le portail prend la valeur de la télémétrie par le `path` et l'affiche. Il n'y a pas de commande pour un capteur.

### Liste de sélection : mode de fonctionnement

```cpp
// Variantes de la liste. L'utilisateur les verra dans le menu déroulant tel quel.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: nom interne de l'entité
    "Mode",                  // label: signature sur la fiche
    kModes,                  // options: tableau des variantes (déclaré au-dessus)
    3,                       // nombre de variantes dans le tableau — auto, on, off = trois.
                             //   C++ ne connaît pas la longueur du tableau de lui-même,
                             //   nous la communiquons
    [](const char* opt) {    // callback: fonction que le noyau appelle quand
                             //   l'utilisateur sélectionne une variante sur le portail.
                             //   opt — la chaîne sélectionnée, par exemple "on"
        onModeSelected(opt); //   on la passe à notre logique (écrire chapitre 7)
    }
);
```

Ici apparaît la deuxième moitié du mécanisme : **la gestion**. Quand l'utilisateur sélectionne une variante sur le portail, l'appareil reçoit une commande, le noyau l'accepte lui-même, la vérifie (les chaînes étrangères qui ne sont pas dans `options` ne vous arriveront pas) et appelle votre callback avec la valeur sélectionnée. Parser les messages MQTT manuellement n'est pas nécessaire — votre zone de responsabilité commence à l'intérieur de `onModeSelected`.

### Champ numérique : seuil de déclenchement

```cpp
s_link.card().number(
    "threshold",       // id: nom interne de l'entité
    "VOC threshold",   // label: signature sur la fiche
    100,               // min: le portail ne permettra pas moins que ça
    400,               // max: plus que ça non plus ; le noyau coupera aussi
                       //   la valeur dans ces limites de son côté
    10,                // step: pas de changement par les flèches du champ
    "",                // unit: unité de mesure; index n'en a pas
    [](float v) {              // callback: appelé quand l'utilisateur a envoyé
                               //   une nouvelle valeur; v — nombre entre min..max
        onThresholdChanged(v); //   on la passe à notre logique (écrire chapitre 7)
    }
);
```

### Assemblage

Vue finale du bloc dans `setup()` — ce qui doit rester dans votre code. Les fonctions `onModeSelected` et `onThresholdChanged` nous les écrirons chapitre 7 ; pour que le code compile dès maintenant, déclarez-les en stubs **avant** `setup()` :

```cpp
// Stubs: implémentations réelles chapitre 7 (logique d'automatisation).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Télémétrie: votre champ vocIndex (chapitre 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Fiche: capteur + deux organes de commande.
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });
}
```

Et le ventilateur ? Vous n'avez pas besoin de le déclarer : le drapeau `hasFan = true` dans `Config` a déjà ajouté la cellule « Ventilateur » au manifeste automatiquement — c'est une compétence du dictionnaire, le noyau sait tout de lui.

!!! note "Les crochets dans les callbacks — toujours vides"
    `[](const char* opt) { ... }` — c'est une lambda, une fonction sans nom ; nous l'avons décomposée en détail dans [l'encadré du chapitre 5](05-sensor-and-telemetry.md). Rappel de la règle du noyau : les crochets de capture sont toujours vides (`[]`), on ne prend « rien avec soi » dans la lambda, tout ce qui est nécessaire est stocké dans les variables globales — comme `g_mode` et `g_threshold` du chapitre suivant.

## 2. Mise en page automatique de la fiche

La mise en page n'a pas besoin d'être définie. Le portail rassemblera lui-même la fiche à partir des entités déclarées — et l'assemblera proprement : les capteurs (cellules) se groupent en rangées (jusqu'à trois par rangée, puis retour), les organes de commande sont dessous, chacun sur sa propre rangée, tout en style du portail. Pour la plupart des appareils, ça suffit — l'interface est impeccable sans penser à la mise en page.

L'ordre des entités sur la fiche — l'ordre de leur déclaration dans `setup()`.

## 3. Mise en page personnalisée de la fiche (optionnel)

D'abord — comment s'organise la fiche. La fiche est une pile verticale de **rangées**. Une rangée de cellules de capteurs partage la largeur à parts égales : une cellule prend toute la largeur, deux — la moitié chacune, trois — un tiers chacune. Les commandes (liste, champ, bouton) d'une rangée se placent l'une sous l'autre sur toute la largeur.

La mise en page automatique du paragraphe précédent arrange les entités dans ces rangées elle-même. Si vous voulez décider vous-même quoi mettre côte à côte — définissez les rangées manuellement par les appels `layoutRow`. Un appel = une rangée, l'ordre des appels = l'ordre des rangées de haut en bas :

```cpp
// Rangée 1 : deux cellules — indice VOC et ventilateur, chacun par moitié.
s_link.card().layoutRow("voc", "fan");

// Rangée 2 : deux commandes — mode et seuil, l'une sous l'autre.
s_link.card().layoutRow("mode", "threshold");
```

Dans `layoutRow` on passe les **id** des entités — les noms internes que vous leur avez donnés lors de la déclaration (voilà pourquoi l'id était nécessaire). `"fan"` — l'id de l'entité du dictionnaire du ventilateur, créée par le drapeau `hasFan`.

Sur la fiche ça donnera cette composition :

```text
┌─ DIY Air Filter ────────────────┐
│  VOC index        │  Ventilateur │   ← rangée 1: voc, fan
│  103              │  Éteint      │
├───────────────────┴─────────────┤
│  Mode                           │   ← rangée 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [150                         ] │
└─────────────────────────────────┘
```

Les entités que vous n'avez mentionnées dans aucune rangée ne disparaîtront pas — le portail les dessinera dessous en liste automatique. Vous pouvez ainsi mettre en page juste « l'essentiel », et laisser le reste à l'automatique.

## 4. Ce qui part en ondes

Le noyau publiera dans le topic `idryer/{serial}/card` (retained) :

```json
{
  "v": 1,
  "entities": [
    { "id": "fan",  "type": "binary_sensor", "device_class": "fan",
      "source": "telemetry", "path": "units[0].fanStatus" },
    { "id": "voc",  "type": "sensor", "label": "VOC index",
      "source": "telemetry", "path": "units[0].vocIndex" },
    { "id": "mode", "type": "select", "label": "Mode",
      "options": ["auto", "on", "off"], "action": "card.mode", "arg": "value" },
    { "id": "threshold", "type": "number", "label": "VOC threshold",
      "min": 100, "max": 400, "step": 10, "action": "card.threshold", "arg": "value" }
  ],
  "layout": [ ["voc", "fan"], ["mode", "threshold"] ]
}
```

Comprendre ce JSON n'est pas obligatoire — le noyau le génère à partir de vos appels. Mais le savoir est utile : si vous écrivez un firmware **pas** sur `idryer-core` (Rust, MicroPython, n'importe quoi), il suffit de publier vous-même un tel JSON — le portail est omniprésent, tant que le format coïncide.

## 5. Vérification

Flashez et ouvrez le tableau de bord du portail :

- la cellule **VOC index** montre l'indice en direct (soufflez sur le capteur — le nombre croît au prochain mise à jour) ;
- la cellule **Ventilateur** — Allumé/Éteint ;
- **Mode** — liste déroulante, **VOC threshold** — champ numérique : la valeur part environ 0,6 s après la modification, sans bouton de confirmation. La liste et le champ commencent à la première option et au minimum : ils envoient des commandes et n'affichent pas le réglage actuel de l'appareil ; l'état est montré par les cellules (par exemple le ventilateur).

La sélection du mode et du seuil ne fait rien pour le moment — ce sont des callbacks vides. Nous les ferons vivre dans [le chapitre suivant](07-auto-logic.md).

!!! note "C'est exactement ce concept"
    Remarquez ce qui s'est passé : vous avez décrit l'interface en cinq lignes de firmware — et elle est apparue sur le portail et l'app. Le même truc fonctionne pour n'importe quel appareil à vous : changez juste l'id, les signatures et les callbacks.

!!! note "Opérations avec démarrage et arrêt"
    La liste, le champ et le bouton envoient une valeur tout de suite. Les opérations avec démarrage et arrêt — séchage, chauffage, éclairage — se déclarent comme **actions** de la carte avec un mode et des paramètres de lancement : la carte affiche un formulaire de démarrage et, pendant l'opération, un bloc de session avec un bouton d'arrêt. Exemple : le [chapitre 7](../09-build-a-device/07-heating-control.md) de la section sur l'armoire.
