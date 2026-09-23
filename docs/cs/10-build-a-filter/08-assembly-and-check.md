---
title: "Chytrý filtr: montáž a finální ověření"
description: "Montáž filtru do pouzdra, pořadí filtrační vrstev a komplexní kontrolní seznam: senzor, telemetrické údaje, karta, příkazy, automatika."
---

# Montáž a ověření

## Montáž

1. **Kryt.** Krabice se dvěma otvory: vstup vzduchu a výstup. Vytisknete si pro svůj filtrační element nebo upravíte hotovou krabici.
2. **Vrstvy po toku vzduchu:** vstup → HEPA → uhlí → ventilátor (na výfuk) → výstup. Spoje bez mezer: vzduch je líný a projde kolem filtru, kde se dá.
3. **Senzor** — na přívodu vzduchu, před filtry: musí snímat špinavý vzduch z místnosti, nikoli již vyčištěný.
4. **Elektronika** — v oddělené přihrádce nebo na stěně, dál od toku prachu. Desku na distanční sloupky, ne „naházeno".
5. Vodiče zajistěte: vibrace ventilátoru postupem času uvolní vše, co není pevně uchyceno.

## Kontrolní seznam

Kontrolujte v pořadí — každý bod staví na předchozích.

| # | Ověření | Jak |
|---|---|---|
| 1 | Napájení | 12 V na větvi ventilátoru, 5 V za buck, 3,3 V na senzoru |
| 2 | Senzor funguje | v Serial logu index ~100 v čistém vzduchu, roste po výdechu |
| 3 | Zařízení Online | stav v portálu po spárování v aplikaci |
| 4 | Telemetrie | `vocIndex` a `fanStatus` ve streamu zařízení |
| 5 | Karta | dlaždice VOC a Ventilátor, seznam Mode, pole Threshold |
| 6 | Příkaz z portálu | Mode → `on`: ventilátor se zapnul, karta zobrazila „Zap" |
| 7 | Automatika | Mode → `auto`, vydechnout: zapnul na prahu, vypnul pod ním |
| 8 | Restart | režim a práh se zachovaly, karta se obnovila sama |

## Co dál

Filtr je hotov. Dál — dle libosti:

- **Více entit**: tlačítko „proplach 5 minut" (`card().button(...)`), druhý senzor, čítač provozních hodin filtru s upozorněním na výměnu;
- **Vlastní rozvržení**: `layoutRow` jste již viděli;
- **Vlastní zařízení**: celý tento oddíl je šablona. Vyměňte senzor, akční člen a logiku — a podle stejného schématu postavíte zvlhčovač, odsávání, regulátor čehokoli. Manifest rozhraní sestaví sám.

Pokud se něco nespustí — [Typické chyby](../08-common-mistakes/01-overview.md).
