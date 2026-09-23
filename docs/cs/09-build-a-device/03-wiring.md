---
title: "Schéma zapojení vyhřívané skříně na ESP32"
description: "Mapování pinů ESP32 pro domácí skříň: SHT31 přes I2C, termistor na ADC, ohřívač a ventilátor přes spínač. Oddělení slaboproudé a silnoproudé části."
---

# Schéma zapojení

Na této stránce — jak připojit součástky kolem ESP32. Nejdříve obecná mapování pinů, poté připojení jednotlivých uzlů a pravidla vedení silnoproudé části.

!!! warning "Nejdříve zkontrolujte pinout vaší desky"
    Čísla pinů níže — příklad. Různé desky ESP32-C3 a ESP32-S3 mají různé číslování a rozmístění pinů. Před montáží zkontrolujte pinout přesně vaší vývojové desky: u DevKit a Super Mini jej zveřejňuje výrobce a najdete jej v popisu desky. Ne všechny piny lze používat volně: některé jsou obsazeny bootloadem, flash nebo USB.

## Mapování pinů (příklad)

| Uzel | Vedení | Pin ESP32 (příklad) |
|------|-------|----------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Termistor | signál ADC | GPIO2 |
| Ohřívač (spínač) | řízení | GPIO4 |
| Ventilátor (spínač/PWM) | řízení | GPIO5 |

Napájení snímačů — `3.3V` a `GND` z desky. Silnoproudá část je napájena zvlášť.

## SHT31 přes I2C

SHT31 se připojuje čtyřmi vodiči:

1. `VCC` snímače — na `3.3V` desky.
2. `GND` snímače — na `GND` desky.
3. `SDA` snímače — na pin `SDA` (příklad: GPIO8).
4. `SCL` snímače — na pin `SCL` (příklad: GPIO9).

Vedení I2C jsou krátká. Pokud je snímač daleko od desky, držte vodiče co nejkratší a zkroucené. Většina modulů SHT31 má přitahovací rezistory již osazeny na desce modulu.

!!! note "Adresa SHT31"
    SHT31 má obvykle adresu `0x44` (někdy `0x45`). Pokud snímač neodpovídá, zkontrolujte adresu a vedení `SDA`/`SCL`.

## Termistor na ADC

Termistor se zapojuje do děliče napětí spolu s tahacím rezistorem:

1. Jeden vývod termistoru — na `3.3V`.
2. Druhý vývod termistoru — na styčný bod s rezistorem `4.7 kΩ` a na pin ADC (příklad: GPIO2).
3. Druhý vývod rezistoru `4.7 kΩ` — na `GND`.

Řadič měří napětí ve středním bodě děliče a z něj vypočítá odpor termistoru a poté teplotu. Typ termistoru je určen v firmwaru (viz [Řízení ohřevu](07-heating-control.md)).

Podrobně o kontrole a montáži — [Kontrola termistoru](../06-practical-guides/02-checking-thermistor.md).

## Ohřívač a ventilátor přes spínač

ESP32 neovládá zátěž přímo, ale přes spínač. Jaký spínač — závisí na verzi z [Seznamu součástek](02-bom.md).

### Verze A (24V/12V) — MOSFET modul

1. Signálový vstup modulu (`PWM`/`SIG`) — na pin řízení ESP32 (příklad: GPIO4 pro ohřívač, GPIO5 pro ventilátor).
2. `GND` modulu — na společný `GND` s ESP32.
3. Vstup napájení modulu a zátěž — na napájení `24V`.

!!! warning "Společná masa"
    `GND` řadiče a `GND` silnoproudého zdroje napájení musí být připojeny. Bez společné masy signál řízení nemá referenční úroveň a spínač pracuje nepředvídatelně.

Připojení ventilátoru s řízením je podrobně vysvětleno v [Připojení ventilátoru](../06-practical-guides/01-connecting-fan.md). Logika spínače — [MOSFET modul](../01-electronics-basics/02-mosfet-module.md).

### Verze B (220V) — SSR/relé

!!! danger "Před montáží části sítě"
    Všechna připojení k síti proveďte se zcela odpojeným zařízením. Skříň se síťovou částí musí mít ochranné uzemění a pojistku. Síťové vodiče používejte dostatečného průřezu a bezpečně je upevněte v svorkách.

SSR má dvě strany. **Řídící** — nízkonapěťový vstup, kterým řídí řadič. **Silnoproudá** — vývody, kterými prochází síťové napětí zátěže. Strany jsou navzájem izolovány optočlenem uvnitř SSR, takže je možné síť řídit slabým signálem `3.3V`.

1. Řídící vstup je obvykle označen `DC+` a `DC-` (někdy `+` a `-`) a je navržen na `3–32V` stejnosměrného proudu. Připojte `DC+` na pin řízení ESP32 (příklad: GPIO4) a `DC-` na `GND` řadiče. Napětí `3.3V` z pinu ESP32 je dostatečné k otevření SSR.
2. Silnoproudé vývody (často značeny jako síť/`AC` a zátěž/`LOAD`) se zapojují do přerušení jednoho ze síťových vodičů ohřívače — stejně jako vypínač ve vodiči.
3. Ventilátor se ovládá samostatným SSR nebo relé stejným způsobem.

!!! note "Proč SSR chladič"
    Při spínání SSR se mírně zahřívá a čím větší proud zátěže, tím větší ohřev. Proto se SSR přišroubuje na chladič (kovový plát na odvod tepla) a SSR se volí s rezervou proudu — výrazně vyšší než proud zátěže. Jakou rezervu a chladič potřebujete pro váš proud — [Polovodičové relé (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Vedení: slaboproudá a silová část

- Udržujte signálové vodiče (snímače, řízení) oddělené od silnoproudých.
- Nepouštějte vodiče termistoru a I2C podél silnoproudých vodičů ohřívače — je to zdroj rušení.
- Ve verzi B fyzicky oddělte zónu sítě a nízkonapětí uvnitř skříně.
- Všechny masy slaboproudé části sveďte do jednoho bodu.

Rušení od ventilátoru a špatná masa — častá příčina „plovoucích" hodnot a restartů. Viz [Chyby vedení](../08-common-mistakes/03-wiring-mistakes.md).

## Co zkontrolovat před připojením napájení

- Napájení snímačů `3.3V`, ne `5V`.
- Termistor a rezistor děliče jsou správně zapojeny, ADC pin je ve středním bodě.
- Společná masa řadiče a silnoproudého zdroje napájení.
- Ve verzi B — uzemění skříně, pojistka, bezpečné svorky, izolace.
- Žádné zkraty mezi napájením a zemí (zkontrolujte multimetrem).

Kontrola multimetrem — [Multimetr](../05-tools/02-multimeter.md).

## Co dál

Hardwarová část je sestavena. Pokračujte [Spuštění firmwaru na jádru](04-firmware-start.md): vytvořte projekt a přiveďte zařízení do stavu Online na portálu.
