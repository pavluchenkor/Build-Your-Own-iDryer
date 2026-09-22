---
title: "Smart filter: assembly and final verification"
description: "Assembling the filter into a case, filter layer order, and complete checklist: sensor, telemetry, card, commands, automation."
---

# Assembly and verification

## Assembly

1. **Case.** A box with two openings: air inlet and outlet. Print for your filter cartridge or modify a ready-made one.
2. **Layer order by air flow:** inlet → HEPA → carbon → fan (as exhaust) → outlet. Joints without gaps: air is lazy and will bypass the filter if it can.
3. **Sensor** — on the inlet flow, before the filters: it should smell the room's dirty air, not the cleaned one.
4. **Electronics** — in a separate compartment or on the wall, away from the dust flow. Board — on spacers, not "loose."
5. Secure the wires: fan vibration will loosen anything not secured over time.

## Complete checklist

Check in order — each point depends on the previous ones.

| # | Check | How |
|---|---|---|
| 1 | Power | 12V on fan line, 5V after buck, 3.3V on sensor |
| 2 | Sensor alive | in Serial log index ~100 in clean air, grows from breath |
| 3 | Device Online | status on the portal after pairing in the app |
| 4 | Telemetry | `vocIndex` and `fanStatus` in device stream |
| 5 | Card | VOC and Fan cells, Mode list, Threshold field |
| 6 | Command from portal | Mode → `on`: fan turned on, card shows "On" |
| 7 | Automation | Mode → `auto`, breathe: turned on at threshold, turned off below |
| 8 | Reboot | mode and threshold saved, card came alive by itself |

## What's next

Filter is ready. Next — your choice:

- **More entities**: button "purge 5 minutes" (`card().button(...)`), second sensor, fan hours counter with replacement reminder;
- **Nice layout**: factory `layoutRow` you already saw;
- **Your devices**: this whole section is a template. Replace the sensor, actuator, and logic — and by the same scheme assemble a humidifier, exhaust, controller of anything. The manifest will build the interface itself.

If something doesn't start — [Common Mistakes](../08-common-mistakes/01-power-mistakes.md).
