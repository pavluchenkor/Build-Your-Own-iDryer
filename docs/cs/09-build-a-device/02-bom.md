---
title: "Složení systému vyhřívaného skříně: součástky a dvě verze silové části"
description: "Seznam součástek pro vlastnoručně vyrobený vyhřívací skříň na ESP32: senzor SHT31, termistor, topný prvek a ventilátor v nízkonapěťové (24V) a síťové (220V) verzi."
---

# Složení systému

Na této stránce je seznam součástek zařízení a dvě varianty silové části. Slaboproudá část (kontrolér a senzory) je v obou verzích stejná. Liší se pouze to, jak se spínají topný prvek a ventilátor.

## Slaboproudá část (stejná pro obě verze)

| Uzel | Účel | Poznámka |
|------|------|---------|
| ESP32-C3 nebo ESP32-S3 | Kontrolér: logika, Wi-Fi, portál | Vyhovuje DevKit nebo Super Mini |
| Senzor SHT31 | Teplota a vlhkost vzduchu ve skříni | Rozhraní I2C |
| Termistor NTC 100K | Kontrola teploty topného prvku | Například Generic 3950 |
| Rezistor pull-up termistoru | Dělič napětí pro ADC | Obvykle `4,7 kΩ` |
| Zdroj napájení | Napájení kontroléru a nízkonapěťové periférie | Napětí pro vybranou verzi |

C3 a S3 jsou pro tento příklad rovnocenné: kód, zapojení i postup sestavení jsou stejné. S3 je výkonnější a má větší paměť - vezměte jej, pokud budete později chtít přidat displej, adresovatelný LED pás nebo vlastní náročnou logiku.

ESP32 byl vybrán proto, že obsahuje Wi-Fi, potřebná rozhraní (I2C pro SHT31, ADC pro termistor, PWM pro řízení zátěže) a je přímo podporován `idryer-core`. Více - [Kontrolér ESP32](../02-controllers/01-esp32-controller.md).

!!! warning "Logika ESP32 - 3.3V"
    ESP32 pracuje na `3.3V`. Nepřipojujte `5V` k jeho vývodům. To platí pro senzory, moduly a adaptéry. Více - [Chyby kontrolérů](../08-common-mistakes/04-controller-mistakes.md).

## Senzory

**SHT31** měří teplotu a vlhkost vzduchu uvnitř skříně. Toto je hlavní zpětná vazba: podle ní vidíte, zda se udržuje nastavené klima. Připojuje se přes I2C (dvě linky: `SDA`, `SCL`). Více - [Termistory a klimatické senzory](../03-common-components/04-thermistors.md).

**Termistor** měří teplotu samotného topného prvku, ne vzduchu. Je potřebný, aby se topný prvek nepřehřál: vzduch se zahřívá pomalu, topný prvek - rychle. Termistor se připojuje jako dělič napětí na výstup ADC. [Kontrola termistoru](../06-practical-guides/02-checking-thermistor.md).

!!! note "Proč dva senzory tepla"
    SHT31 říká "jaká je teplota ve skříni", termistor - "není topný prvek přehřátý". První nastavuje cíl, druhý chrání před havárií.

## Silová část: zvolte verzi

Topný prvek a ventilátor jsou zátěž, kterou řídí kontrolér. ESP32 nemůže spínat takovou zátěž přímo: jeho výstup produkuje slabý signál `3.3V`. Mezi kontrolérem a zátěží je potřeba klíč.

Existují dvě zásadně odlišné verze. Zvolte jednu v závislosti na tom, jaký topný prvek a ventilátor používáte.

### Verze A - nízkonapěťová (24V nebo 12V)

Topný prvek a ventilátor jsou napájeny z `24V` (nebo `12V`) stejnosměrného proudu. Toto je jednodušší a bezpečnější cesta pro vlastnoruční montáž.

| Uzel | Součástka |
|------|-----------|
| Topný prvek | Topný prvek `12V` nebo `24V` (PTC-topný prvek) |
| Ventilátor | Ventilátor `24V` nebo `12V` (2-pin nebo 4-pin) |
| Klíč topného prvku | Modul na bázi MOSFET |
| Klíč ventilátoru | Modul na bázi MOSFET (nebo 4-pin PWM přímo) |
| Zdroj napájení | `24V DC` s rezervou výkonu |

Kontrolér řídí MOSFET modul signálem z výstupu ESP32. Modul spíná nízkonapěťovou zátěž. Toto je stejná logika jako v připraveném kontroléru. Více - [MOSFET modul](../01-electronics-basics/02-mosfet-module.md).

Výkon zdroje se vypočítá pro celkovou zátěž s rezervou - viz [Výpočet proudu zátěže 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Doporučená verze pro první zařízení"
    Pokud zařízení skládáte poprvé, začněte verzí A. Zde není síťové napětí na zátěži a chyba montáže je méně nebezpečná.

### Verze B - síťová (110-230V AC)

Topný prvek a ventilátor jsou napájeny ze sítě `110-230V`. To se dělá, když je potřebný silný síťový topný prvek - například připravený topný prvek s ventilátorem pro skříň. Zde se místo MOSFET modulu používají AC spínací moduly.

| Uzel | Součástka |
|------|-----------|
| Topný prvek | Síťový topný prvek `110-230V AC` |
| Ventilátor | Síťový ventilátor `110-230V AC` |
| Klíč topného prvku | Solid-state relé (SSR) pro AC |
| Klíč ventilátoru | SSR nebo běžné relé pro AC |
| Zdroj napájení | Samostatný `24V`/`5V DC` pro kontrolér a senzory |
| Ochrana | Pojistka, ochranné uzemnění skříně |

!!! danger "Síťové napětí je nebezpečné pro život"
    Verze B pracuje s napětím `110-230V`. Chyba montáže může vést k úrazu elektrickým proudem nebo požáru. Před montáží si bezpodmínečně přečtěte materiály o bezpečnosti: [Triák](../01-electronics-basics/03-triac.md), [Solid-state relé (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Chyby topných prvků a SSR](../08-common-mistakes/05-heater-ssr-mistakes.md). Pokud nemáte zkušenosti se síťovým napětím, zvolte verzi A.

Kontrolér a senzory jsou ve verzi B stejně napájeny ze samostatného nízkonapěťového zdroje (`5V`/`24V`). Síťová část a slaboproudá část musí být fyzicky a elektricky odděleny.

## Volitelné moduly

Tyto uzly nejsou pro skříň povinné, ale jsou podporovány jádrem a mohou být přidány později:

- adresovatelné LED osvětlení (`hasLed`);
- váhový senzor spotřeby filamentu (`hasWeight`);
- RFID tag cívky (`hasRfid`).

Základní skříň je nepoužívá - začínáme s minimem.

## Co dál

Když jsou vybrány součástky, přejděte na [Schéma zapojení](03-wiring.md): který výstup ESP32 za co odpovídá a jak rozdělit nízkonapěťovou a silovou část.
