---
title: "Chytrý vzduchový filtr: koncepce a co vám portál dává"
description: "Komplexní příklad č. 2: vzduchový filtr pro zonu 3D tisku na ESP32 a idryer-core — vlastní senzor VOC, vlastní ovládání a automatická karta v portálu prostřednictvím card manifestu."
---

# Chytrý vzduchový filtr: koncepce

Toto je druhý komplexní příklad oddílu „postav si sám". V [prvním příkladu](../09-build-a-device/01-concept.md) jste stavěli vytápěnou skříň ze „slovníkových" stavebních prvků ekosystému: teplota, vlhkost, topidlo. Zde děláme krok dál — stavíme zařízení, **které v ekosystému iDryer vůbec neexistuje**: vzduchový filtr se senzorem těkavých organických sloučenin (VOC).

## Hlavní myšlenka: toto je koncepce, ne recept na jedno zařízení

Pozorně si přečtěte tento odstavec — je důležitější než zbytek celé kapitoly.

Filtr zde je jen příklad. Popsaný přístup funguje pro **jakékoliv zařízení, které si vymyslíte**: zvlhčovač, ofukovací stanice, řadič odsávání, monitor skladu filamentu, cokoliv. Deklarujete v programu, jaké má zařízení senzory a ovládací prvky — jedním nebo dvěma řádky kódu na každý — a zařízení se **automaticky objeví v portálu a mobilní aplikaci** s hotovou kartou: živé hodnoty, tlačítka, vstupní pole. Ani řádek kódu na straně portálu, žádné dohody se stranou iDryer, žádné pull-requesty.

Funguje to díky mechanismu **dynamických karet** (entity manifest): zařízení zveřejní strojově čitelný popis „co zobrazit a čím ovládat", a portál s aplikací si podle něj sestaví rozhraní. Jak to vypadá v kódu — [kapitola o kartě](06-card.md).

!!! note "Co to znamená v praxi"
    Vymysleli jste zařízení → sestavili na ESP32 → popsali senzory a tlačítka v programu → spárovali s účtem v aplikaci. Hotovo: zařízení má rozhraní v portálu a aplikaci. Od nápadu po „ovládám ze smartphonu" — jeden večer.

## Co přesně stavíme

**Vzduchový filtr pro zónu 3D tisku**: krabice s ventilátorem, HEPA filtrem a uhlíkovou vrstvou, která:

- měří kvalitu vzduchu senzorem VOC (SGP40);
- sama zapne ventilátor, když je vzduch špinavý, a vypne, když se vyčistil;
- zobrazuje v portálu VOC index a stav ventilátoru;
- umožňuje z portálu zvolit režim (`auto` / `on` / `off`) a nastavit práh zapnutí.

ABS a ASA při tisku páchnou styrenem, pryskyřice svým tónem. Filtr u tiskárny není luxus, je to hygiena.

## Proč je to ideální první projekt

Pokud se vám skřín z oddílu 09 zdál složitý — začněte filtrem:

- **žádný ohřívač** — tedy žádná silová část, pojistky ani rizika;
- minimální počet součástek: deska, senzor, ventilátor, tranzistor;
- rozpočet kolem `$15` bez pouzdra;
- v případě jakékoliv chyby v kódu nejhorší, co se stane, je, že se ventilátor nezapne.

## Hranice záměru

Upřímně si vymezíme, čím tento filtr **není**:

- není to odsávání: vzduch cirkuluje přes filtr v uzavřené smyčce, nevychází ven;
- není to zdravotnický přístroj: SGP40 ukazuje relativní **index** kvality vzduchu, nikoli koncentraci konkrétního plynu v ppm;
- filtr nenahradí větrání.

!!! note "VOC nebo CO2?"
    Pro výpary z tisku je správnou volbou senzor VOC: reaguje na organické sloučeniny (styren, rozpouštědla). Senzory CO2 (například NDIR senzor MH-Z19) měří oxid uhličitý — to je ukazatel vydýchaného vzduchu, nikoli znečištění z tisku. Pokud chcete obojí, ENS160 poskytuje VOC index i odhad eCO2 zároveň; přístup z tohoto oddílu se nezmění — stačí jeden řádek navíc v manifestu karty.

## Struktura oddílu

1. [Soupis komponent](02-bom.md) — co koupit.
2. [Schéma zapojení](03-wiring.md) — jak zapojit.
3. [Start firmwaru](04-firmware-start.md) — kostra na `idryer-core`, spárování s portálem.
4. [Senzor a telemetrie](05-sensor-and-telemetry.md) — čteme VOC a posíláme do cloudu.
5. [Karta zařízení](06-card.md) — deklarujeme senzory a ovládání, získáváme rozhraní.
6. [Logika automatiky](07-auto-logic.md) — práh, hystereze, ruční režim z portálu.
7. [Montáž a ověření](08-assembly-and-check.md) — finální kontrolní seznam.
