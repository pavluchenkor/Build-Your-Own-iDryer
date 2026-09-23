---
title: "Montáž vyhřívané skříně a kontrola před spuštěním"
description: "Finální montáž vlastnoručně vyrobené skříně na ESP32: instalace do pouzdra, první zahřívání, kalibrace teploty a bezpečnostní seznam před trvalým provozem."
---

# Montáž a kontrola

Na této stránce montujete zařízení do pouzdra, provádíte první kontrolované zahřívání a ověřujete, že skříň pracuje bezpečně. Provádějte kontroly postupně a zařízení nenechávejte bez dozoru při prvním zapnutí.

## Postup montáže

1. Upevněte ESP32 a výkonovou část v pouzdře tak, aby byla nízkovoltážová a výkonová zóna odděleny.
2. Umístěte senzor SHT31 v skříni mimo přímý proud od topidla — jinak ukáže teplotu proudu vzduchu namísto teploty vzduchu v objemu.
3. Upevněte termistor v tepelném kontaktu s topidlem.
4. Zkontrolujte, že vodiče se nedotýkají topidla a nepadají do ventilátoru.
5. Ve verzi B (`220V`) se ujistěte, že jsou síťové vodiče upevněny v svorkách, izolace je neporušená a pouzdro je uzemneno.

Požadavky na pouzdro a umístění prvků — [Návrh pouzdra](../07-3d-printing/05-enclosure-design.md).

!!! warning "Tištěné díly v blízkosti tepla"
    PLA změkne při teplotě, která se snadno vyskytuje v blízkosti topidla. Díly blízko tepla tisknout z tepelně odolného materiálu: ABS, ASA nebo PA (nylon). Viz [Tepelně odolné materiály](../07-3d-printing/04-heat-resistant-materials.md) a [Proč je PLA riskantní volba](../07-3d-printing/06-why-pla-is-risky.md).

## Kontrola před připojením napájecího proudu

Proveďte kontrolu multimetrem před prvním zapnutím:

- mezi napájením a zemí není zkrat;
- napájení senzorů je `3.3V`, nikoli `5V`;
- společná zem regulátoru a výkonového zdroje;
- termistor a rezistor děliče jsou zapojeny správně;
- ve verzi B — uzemnění pouzdra a pojistka na místě.

Jak používat multimetr — [Multimetr](../05-tools/02-multimeter.md).

## První spuštění

1. Připojte napájení pouze na regulátor a senzory. Pokud je zdroj jeden pro všechno, napájejte regulátor zvlášť: z USB nebo přes snižující měnič, a vodič topidla zatím nepřipojujte.
2. Ověřte, že je zařízení Online na portálu a zobrazuje teplotu a vlhkost.
3. Připojte topidlo a ventilátor.
4. Spusťte režim udržování tepla z portálu a sledujte.

!!! danger "Nenechávejte první zahřívání bez dozoru"
    Při prvním zapnutí sledujte zařízení. Ověřte, že se topidlo vypíná po dosažení cíle a ochranou termistoru, nikoli aby topilo nepřetržitě.

Co sledovat v prvních minutách:

- teplota vzduchu roste a stabilizuje se kolem cíle;
- teplota topidla nepřekračuje stanovenou mez;
- topení se vypíná po dosažení cíle a znovu se zapíná po vychladnutí o hodnotu hystereze;
- ventilátor funguje a nedotýká se vodičů;
- regulátor se nerestartuje při připojení zátěže.

## Kalibrace

Po prvním zahřívání porovnejte údaje se samostatným teploměrem v skříni:

- pokud se teplota vzduchu v skříni liší od cíle — zkontrolujte umístění SHT31 (neměl by stát v proudu nebo u stěny);
- pokud se teplota topidla zdá neuvěřitelná — zkontrolujte typ termistoru a jmenovitou hodnotu rezistoru děliče;
- v případě potřeby upravte cílovou teplotu a hysterezi v [menu](06-menu.md).

## Pokud něco nefunguje

| Příznak | Kde hledat |
|---------|-----------|
| Regulátor se restartuje při zatížení | [Chyby napájení](../08-common-mistakes/02-power-mistakes.md) |
| Senzor zobrazuje nesmysly | [Chyby propojení](../08-common-mistakes/03-wiring-mistakes.md), [Kontrola termistoru](../06-practical-guides/02-checking-thermistor.md) |
| Zařízení se nepřipojuje k Wi-Fi | [Chyby regulátorů](../08-common-mistakes/04-controller-mistakes.md) |
| Topidlo/SSR se velmi zahřívá | [Chyby topidel a SSR](../08-common-mistakes/05-heater-ssr-mistakes.md) |

Obecný postup diagnostiky — [Kontrolní seznam diagnostiky](../08-common-mistakes/06-diagnostic-checklist.md).

## Kontrolní seznam před trvalým provozem

- [ ] Zařízení udržuje cílovou teplotu a netopí nepřetržitě.
- [ ] Ochrana topidla termistorem funguje.
- [ ] Vodiče se nedotýkají topidla a ventilátoru.
- [ ] Tištěné díly blízko tepla jsou tepelně odolné.
- [ ] Ve verzi B: pouzdro je uzemneno, pojistka je instalována, izolace je neporušená.
- [ ] Data na portálu odpovídají skutečné teplotě v skříni.

## Shrnutí

Montážou vyhřívané skříně pro skladování na ESP32 a `idryer-core` jste vytvořili zařízení, které čte klima a teplotu topidla, udržuje stanovenou teplotu, chrání topidlo před přehřátím a lze jej řídit z portálu. Toto je hotová základna, na níž je možné stavět vlastní moduly ekosystému.

Další komponenty — podsvícení, váhy, RFID — jádro rovněž podporuje; lze je přidat stejným způsobem: senzor nebo periférie → telemetrie nebo příkaz → zobrazení na portálu.
