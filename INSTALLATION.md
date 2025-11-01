# Installation & Onboarding Guide

This guide walks you through adding the Wavin AHC 9000 (Jablotron AC‑116) ESPHome external component, generating optimal YAML entities, and trimming the final config for daily use.

## Prerequisites
- Working ESPHome setup (1.21+ recommended) and an ESP32 board.
- RS‑485 adapter wired to the controller (A/B twisted pair) and to ESP32 UART pins (example uses GPIO16=RX, GPIO17=TX).
- (Optional) Floor probes installed if you want comfort/floor climates.
 - (Optional) RS‑485 direction control pin (DE/RE) if your adapter does not auto‑manage half‑duplex.

### Choosing Direction Control (flow_control_pin vs tx_enable_pin)
You can normally wire many small MAX3485/75176 breakout boards so that DE & /RE are tied together. The component now supports two optional GPIOs:

| Option | Behavior | Recommended Use |
|--------|----------|-----------------|
| `flow_control_pin` | Pulsed HIGH only while transmitting, LOW for receive. Unified DE/RE. | Preferred (clean bus turnaround; less risk of bus contention) |
| `tx_enable_pin` | HIGH enables driver (left HIGH between frames if you do not also supply flow control). | Legacy / only if already wired |

Provide at most one (prefer `flow_control_pin`). If both are given they will both toggle (harmless but redundant). If your transceiver auto‑handles direction you can omit both.

## Overview of the Two-Phase Flow
1. Minimal “generation” configuration (enables YAML generator package & text sensors).
2. Call service to publish YAML chunk sensors.
3. Use the provided Jinja template to stitch chunks into full YAML sections.
4. Copy the generated entity blocks into your permanent ESPHome YAML.
5. Disable (comment out) the generator package.
6. Recompile & upload clean final firmware.
7. (Optional) Iterate later by temporarily re-enabling the generator.

---
## Step 1: Minimal Generation Config
Create a temporary node config (e.g. `wavin_gen.yaml`) like below. Keep it lean—no extra sensors beyond what the generator needs.

```yaml
esphome:
  name: wavin-gen

esp32:
  board: esp32dev
  framework:
    type: esp-idf

external_components:
  - source: github://heinekmadsen/esphome_wavinahc9000v3
    components: [wavin_ahc9000]

logger:
  level: DEBUG
  logs:
    wavin_ahc9000: DEBUG

uart:
  id: uart_wavin
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 38400
  stop_bits: 1
  parity: NONE

wavin_ahc9000:
  id: wavin
  uart_id: uart_wavin
  update_interval: 5s
  # Optional half‑duplex direction control (choose one):
  # flow_control_pin: GPIO4   # Preferred (toggles only during TX)
  # tx_enable_pin: GPIO4      # Legacy enable style
  # Optional temporary speedup (uncomment briefly):
  # poll_channels_per_cycle: 8
  # update_interval: 2s
  # Optional per-channel friendly names (only include those you wish to rename)
  channel_01_friendly_name: "Bedroom"
  channel_02_friendly_name: "Living Room"
  channel_03_friendly_name: "Kitchen"

packages:
  wavin_tools:
    url: https://github.com/heinekmadsen/esphome_wavinahc9000v3
    files:
      - packages/yaml_generator.yaml
    refresh: 0s

# Optional readiness indicator (turns on when discovery stable)
binary_sensor:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    type: yaml_ready
    name: "Wavin YAML Ready"

wifi:
  ssid: "<your-ssid>"
  password: "<your-password>"

api:

ota:
```

Upload this to the ESP. Give it 30–90 seconds to discover channels (longer if you left default polling speed).

---
## Step 2: Publish YAML Suggestion Chunks
In Home Assistant:
1. Open Developer Tools → Actions.
2. Call `esphome.wavin_gen_wavin_publish_yaml_text_sensors`.
3. If you added the readiness binary sensor, wait until it is ON before calling (optional, but avoids partial data).

The following sensors will fill (examples):
- `sensor.wavin_yaml_climate_1` … `sensor.wavin_yaml_climate_8`
- `sensor.wavin_yaml_group_climate_1` … (only if grouped climates exist)
- `sensor.wavin_yaml_comfort_climate_1` … (only if floor probes detected)
- `sensor.wavin_yaml_battery_1` … `sensor.wavin_yaml_battery_8`
- `sensor.wavin_yaml_temperature_1` … `sensor.wavin_yaml_temperature_8`
- (Optionally future floor temperature chunks if enabled)

You can also inspect `sensor.wavin_yaml_suggestion` (may be truncated in HA UI for very long outputs; rely on chunks for reliability).

If a category appears empty (unknown): wait a bit longer and call the publish service again. Early reads might not yet have valid element data.

---
## Step 3: Stitch with Jinja Template
Open the provided `jinjatemplate.txt` (or `jinja_examples.j2` inside a Jinja-capable editor / HA template dev tool). Choose the “All-in-one” macro or the specific climate/sensor macros you need.

The template pulls the chunk sensor states, filters out empty / unknown entries, and emits properly indented YAML with section headers (`climate:`, `sensor:`) and commented single climates that belong to a generated group.

Workflow:
1. Copy the Jinja template content into Home Assistant Developer Tools → Templates.
2. Adjust the sensor entity IDs if your node name differs (`wavin_gen` vs something else).
3. Render the template—confirm the output looks like valid YAML.

---
## Step 4: Copy Entities to Final Node Config
Open (or create) your production node YAML (e.g. `wavin_main.yaml`). Paste the generated blocks beneath existing headers or replace placeholder sections.

Typical pasted sections include:
- Group climates (if any).
- Single channel climates (some may be commented out if grouped).
- Optional comfort climates.
- Battery sensors.
- Temperature sensors.

You can rename `name:` fields freely—functionality is unaffected.

---
## Step 5: Remove / Comment the Generator Package
In the generation config (or in your final node file if you merged them), comment out the generator include:

```yaml
packages:
#  wavin_tools:
#    url: https://github.com/heinekmadsen/esphome_wavinahc9000v3
#    files:
#      - packages/wavin_debug_services.yaml
#    refresh: 0s
```

Leave friendly name keys in place if you want consistent future regeneration.

---
## Step 6: Rebuild & Upload Final Firmware
Recompile the final config without the generator. This removes all temporary text sensors and services, keeping your device entity list lean.

If you ever need to regenerate (e.g. you add channels later):
1. Temporarily re-add the generator package line.
2. Upload.
3. Repeat Steps 2–5.

---
## Optional / Advanced Steps
| Purpose | Action |
|---------|--------|
| Force immediate discovery refresh | Call service `esphome.<node>_wavin_generate_yaml` then publish again |
| Normalize a channel's mode register | Custom service `wavin_strict_heat` with `channel: <n>` |
| Inspect raw YAML in logs | Look for bannered section “Wavin YAML SUGGESTION” in ESPHome logs |
| Add floor (comfort) variant after initial deploy | Just add another climate with `use_floor_temperature: true` for the same channel |
| Only use sensors (no climates) | Copy only sensor blocks from template output |
| Add child lock switch | See child lock section below and add a `switch:` block |

---
## Troubleshooting
| Symptom | Cause / Fix |
|---------|-------------|
| Chunk sensors stay `unknown` | Discovery not finished – wait or ensure polling speed is sufficient |
| Comfort climates missing | No valid floor probe reading yet (>1°C and <90°C) – wait or verify wiring |
| Group climate name not as expected | Some members missing friendly names – add `channel_XX_friendly_name` entries and regenerate |
| Single channel climates commented | Those channels are members of a group; uncomment if you want them active individually |
| Battery always 0% | Thermostat hasn’t reported battery yet or device lacks that metric |
| Child lock switch state reverts | Bus write failed; check DEBUG logs for masked write retry outcomes |
| Child lock switch missing | Ensure `platform: wavin_ahc9000` and a `channel:` 1..16 under `switch:` |

---
## Regeneration Checklist (Later Changes)
1. Re-enable generator package.
2. Upload & wait for readiness.
3. Publish chunks again.
4. Stitch with Jinja.
5. Diff against existing YAML; apply changes you want.
6. Disable generator again.

---
## Summary
You now have a streamlined workflow: generate → stitch → adopt → slim down. This keeps your production firmware minimal while still letting the component guide you through optimal entity selection whenever hardware changes.

Happy automating!

---

## Child Lock (Optional Feature)
Each channel’s packed configuration register exposes a bit (mask `0x0800`) that toggles when the thermostat child lock is engaged. The integration lets you surface this as a simple switch per channel.

### Adding a Child Lock Switch
Add one (or more) entries under `switch:`:
```yaml
switch:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 9
    name: "Office Lock"
```
`type: child_lock` is optional (defaults to that for now).

### Behavior
* Turning ON sets the lock bit; turning OFF clears it via a masked write (preserves other config bits).
* The switch publishes an optimistic state immediately; an urgent refresh validates shortly after.
* Safe to add/remove any time; does not influence climate operation apart from blocking manual thermostat changes locally.

### Tips
* If you lock all zones, keep at least one automation or dashboard path to adjust setpoints remotely.
* For bulk operations, create a HA script or automation targeting multiple `switch.*_lock` entities.

### Example Night Lock Automation
```yaml
automation:
  - alias: Nightly Wavin Child Lock
    trigger:
      - platform: time
        at: "23:00:00"
    action:
      - service: switch.turn_on
        target:
          entity_id:
            - switch.office_lock
            - switch.kitchen_lock
            - switch.living_room_lock
```

Unlock in the morning:
```yaml
automation:
  - alias: Morning Wavin Unlock
    trigger:
      - platform: time
        at: "06:30:00"
    action:
      - service: switch.turn_off
        target:
          entity_id:
            - switch.office_lock
            - switch.kitchen_lock
            - switch.living_room_lock
```

### Troubleshooting Child Lock
| Symptom | Remedy |
|---------|--------|
| Switch flips back | Check logs; underlying masked write failed (bus or timeout) |
| No switch | YAML indentation / schema issue – verify top-level `switch:` list item |
| State lags | Wait one poll cycle; urgent refresh resync soon after write |

---
