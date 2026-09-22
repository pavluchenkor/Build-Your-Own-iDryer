---
title: "Stavíme své zařízení na jádru iDryer: koncepce"
description: "Komplexní příklad: jak od nuly postavit vytápěnou skříň pro skladování filamentu na ESP32 a knihovně idryer-core s připojením k portálu iDryer."
---

# Stavíme své zařízení: koncepce

Tato kapitola je komplexní příklad. Předchozí kapitoly vysvětlovaly jednotlivé součásti: napájení, řadiče, senzory, ohřívače, bezpečnost. Zde z těchto součástí stavíte jedno hotové zařízení a přivádíte jej do pracovního stavu s připojením k [portálu iDryer](https://portal.idryer.org/).

Příklad je postaven na knihovně `idryer-core`. Knihovna si vezme veškerou síťovou práci: připojení k Wi-Fi, vazbu na účet, zabezpečenou MQTT-relaci, periodické zveřejňování telemetrie. Vy píšete pouze to, co je specifické pro vaše zařízení: čtení senzorů, řízení ohřívače a ventilátoru, logiku udržování teploty.

## Co přesně stavíme

Stavíme **vytápěnou skříň pro skladování filamentu**. To je uzavřená skříň na 10–40 kotouček, v níž se udržuje teplota kolem `40–45 °C`.

Je důležité od začátku jasně vymezit hranice úkolu.

!!! note "Toto není vysokoteplotní sušička"
    Nechceme předstírat, že jde o rychlé sušení při vysoké teplotě. Cílem zařízení je udržovat v skříni jemné teplo, které drží filament suchý při skladování.

Teplota `40–45 °C` je dostatečná pro skladování většiny nenáročných plastů — od PLA po ABS — v suchém stavu. Pro aktivní sušení náročných materiálů (nylon, polykarbonát, PA-CF) jsou potřeba vyšší teploty a jiná konstrukce — takové sušičky se staví zvlášť, podle principů z ostatních kapitol.

## Proč to dělat sami

Hotový regulátor iDryer už umí všechno, co je popsáno níže. Tento příklad není jeho náhrada, ale ukazuje **jak je zařízení uvnitř uspořádáno** a poskytuje základ pro vaše vlastní moduly.

Vlastní stavba má smysl, když:

- potřebujete skříň nestandardní velikosti nebo tvaru;
- chcete pochopit, jak regulátor řídí ohřev a komunikuje s portálem;
- plánujete vytvořit svůj modul ekosystému a berete tento příklad jako výchozí bod.

## Jak se to liší od regulátoru V2

Sériový regulátor iDryer V2 má dvouprocesorovou architekturu: hlavní logika běží na samostatném mikrokontroléru, zatímco modul ESP32 funguje pouze jako most k Wi-Fi a portálu. To má smysl pro sériový výrobek s displejem, váhami, RFID a několika kamerami.

Pro vlastní skříň není taková složitost potřebná. Zjednodušujeme architekturu na **jeden ESP32**, který dělá všechno sám:

- čte senzory;
- řídí ohřívač a ventilátor;
- připojuje se k Wi-Fi a portálu přes `idryer-core`.

Funkčně opakujeme chování jedné kamery regulátoru V2 (senzor klimatu, ohřívač se zpětnou vazbou z termistoru, ventilátor), ale v čestné DIY-realizaci na jedné desce.

!!! note "Servo není použito"
    V regulátoru V2 servo řídí vzduchovou klapku komory. Pro skladovací skříň s rovnoměrným jemným ohřevem klapka není potřebná, takže v tomto příkladu servo není.

## Co přináší připojení k jádru

Když je zařízení postaveno na `idryer-core` a vázáno na účet, získáte bez dodatečného kódu:

- řízení a monitorování přes [portál](https://portal.idryer.org/) a mobilní aplikaci;
- graf teploty a vlhkosti ve skříni;
- spuštění a zastavení režimu udržování tepla na dálku;
- konfiguraci parametrů (cílová teplota, hystereze) přes menu zařízení.

!!! note "Vlastní senzory a tlačítka na kartě zařízení"
    Karta zařízení není omezena na slovníkové schopnosti (`has*`). Prostřednictvím card-manifestu může firmware deklarovat libovolné vlastní senzory a ovládací prvky — automaticky se zobrazí na portálu i v aplikaci. Jak to udělat ukazuje průřezový příklad [„Chytrý vzduchový filtr"](../10-build-a-filter/01-concept.md), zejména [kapitola o kartě](../10-build-a-filter/06-card.md). Spuštění a zastavení udržování tepla se deklarují jako akce karty v [kapitole 7](07-heating-control.md).

## Co tato kapitola obsahuje

Dále následuje postupná cesta od prázdné desky k pracující skříni:

1. [Složení systému](02-bom.md) — které součásti bereme a dvě verze silové části (nízkonapěťová a síťová).
2. [Schéma zapojení](03-wiring.md) — mapa pinů ESP32, oddělení slaboproudé a silové části, bezpečnost.
3. [Spuštění firmware na jádru](04-firmware-start.md) — projekt PlatformIO, první spuštění, vazba na portál.
4. [Senzory](05-sensors.md) — připojujeme SHT31 a termistor, získáváme od nich data.
5. [Menu z YAML](06-menu.md) — popisujeme nastavení zařízení, padá do NVS a na portál.
6. [Řízení ohřevu](07-heating-control.md) — logika udržování teploty, ventilátor, spuštění a zastavení z karty zařízení.
7. [Montáž a kontrola](08-assembly-and-check.md) — konečná montáž, první nahřátí, bezpečnostní checklist.

!!! tip "Hotový příklad"
    Pokud chcete hned vidět výsledek — hotový projekt leží ve složce `example/09-cabinet/` repozitáře a je postaven příkazem `pio run -e cabinet`. Kapitoly níže probírají stejný kód po krocích.

## Viz také

- [Kde začít](../00-start-here/01-introduction.md) — obecný postup čtení kapitoly.
- [Regulátor ESP32](../02-controllers/01-esp32-controller.md) — proč je ESP32 pohodlný pro zařízení s Wi-Fi.
- [Běžné součásti](../03-common-components/01-overview.md) — mapa dílů zařízení.
