# ESPHome Component: Wavin AHC 9000 / Jablotron AC-116 (v3 restart)

Integrates the Wavin AHC 9000 (a.k.a. Jablotron AC-116) floor heating controller via its RS‑485 protocol (custom function codes 0x43 / 0x44 / 0x45). Provides auto‑discovery assisted YAML generation, single & grouped climate entities, comfort (floor‑temperature based) climates, and optional per‑channel sensors.

## Key Features

| Feature | Status | Notes |
|---------|--------|-------|
| Up to 16 channels | ✅ | Automatically discovered through staged polling |
| Single channel climates | ✅ | One climate per active channel |
| Group climates | ✅ | Channels sharing same primary element grouped; composite name logic |
| Comfort climates | ✅ | Uses floor temperature as current temp when floor probe detected |
| Battery sensors | ✅ | Per channel (0–100%) |
| Temperature sensors | ✅ | Per channel ambient temperature |
| Floor probe detection | ✅ | Auto-detects plausible floor sensor (>1°C & <90°C) |
| Friendly names per channel | ✅ | `channel_XX_friendly_name` config; used in generated YAML |
| YAML suggestion + chunk sensors | ✅ | Service driven; chunks safe for HA text sensor size limits |
| Commented single climates for grouped members | ✅ | Keeps originals (commented) for reference |
| Jinja stitching templates | ✅ | Provided (`jinjatemplate.txt` and `jinja_examples.j2`) |
| Readiness binary sensor | ✅ | Optional `yaml_ready` type |
| Robust retry & polling pacing | ✅ | 2-attempt read/write retry logic |
| Child lock (per channel) | ✅ | Exposed as switch; toggles controller LOCK bit |

## Project Status / Vibe-Coding Disclaimer
This is a "vibe-coding" / fast-iteration community project:
* Reverse-engineered protocol pieces may evolve; some registers/assumptions could change as more hubs are tested.
* YAML generation format is considered stable enough for use, but minor cosmetic tweaks (indentation, naming heuristics, commenting strategy) can still occur.
* Floor/comfort logic, group naming rules, and friendly-name composition are pragmatic rather than final standards.
* Expect occasional refactors prioritizing clarity and onboarding experience over strict backward compatibility of generated suggestions.
* If you pin this as a GitHub external component for production, review diffs before updating — especially around write behaviors.

Contributions (bug reports, captures, PRs) are welcome. Please include firmware logs (DEBUG level), channel counts, and whether floor probes are present to help refine heuristics.

## Quick Start (Final Config Example)
### TL;DR Workflow
1. Flash a minimal generation config with the `yaml_generator` package enabled.
2. Wait until (optional) `yaml_ready` binary sensor = ON (or ~60s).
3. Call service: `esphome.<node>_wavin_publish_yaml_text_sensors`.
4. Open `jinjatemplate.txt` (or `jinja_examples.j2`) in HA Template editor; render combined YAML.
5. Copy generated entity blocks into your permanent node YAML.
6. Comment out the generator package include.
7. Recompile & upload clean firmware.

Flowchart (Mermaid):
```mermaid
flowchart TD
  A[Start / Flash Generation Config] --> B{Discovery Stable?}
  B -- No --> A
  B -- Yes --> C[Call publish YAML text sensors service]
  C --> D[Chunk Sensors Populated]
  D --> E[Render Jinja Template]
  E --> F[Copy Entities to Final YAML]
  F --> G[Disable Generator Package]
  G --> H[Rebuild & Upload]
  H --> I{Need Changes Later?}
  I -- Yes --> A
  I -- No --> J[Done]
```

Static diagram (SVG):
![Workflow Diagram](docs/workflow_diagram.svg)

```yaml
external_components:
  - source: github://heinekmadsen/esphome_wavinahc9000v3
    components: [wavin_ahc9000]

uart:
  id: uart_wavin
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 38400

wavin_ahc9000:
  id: wavin
  uart_id: uart_wavin
  update_interval: 5s
  # Optional: half-duplex RS485 direction control (DE/RE). Only include ONE of these if needed.
  # flow_control_pin: GPIO4   # Preferred unified DE/RE control (driven HIGH only while transmitting)
  # tx_enable_pin: GPIO4      # Legacy always-on style driver enable (kept for compatibility)
  channel_01_friendly_name: "Bedroom"
  channel_02_friendly_name: "Living Room"
  channel_03_friendly_name: "Kitchen"

climate:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Living Room & Kitchen"
    members: [2,3]
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bedroom"
    channel: 1

sensor:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bedroom Battery"
    channel: 1
    type: battery
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bedroom Temperature"
    channel: 1
    type: temperature
```

### Example ESPHome YAML (explicit platforms)
This variant shows a single channel, a grouped climate, plus both battery and temperature sensors explicitly defined.

```yaml
external_components:
  - source: github://heinekmadsen/esphome_wavinahc9000v3
    components: [wavin_ahc9000]

uart:
  id: uart_wavin
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 38400

wavin_ahc9000:
  id: wavin
  uart_id: uart_wavin
  update_interval: 5s
  # flow_control_pin: GPIO4  # (optional) direction control
  channel_01_friendly_name: "Bedroom"
  channel_02_friendly_name: "Living Room"
  channel_03_friendly_name: "Kitchen"

climate:
  # Group climate (channels 2 & 3). Generated YAML would comment out single 2/3 climates.
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Living Room & Kitchen"
    members: [2,3]
  # Single channel climate (channel 1)
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bedroom"
    channel: 1

sensor:
  # Battery sensor for channel 1
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bedroom Battery"
    channel: 1
    type: battery
  # Ambient temperature sensor for channel 1
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bedroom Temperature"
    channel: 1
    type: temperature
```

### Comfort Climate Example
If a channel has a detected floor probe you can expose an alternative "comfort" climate that reports the floor temperature as the current temperature and also surfaces the floor min/max limits as adjustable low/high targets.

Add `use_floor_temperature: true` to a single-channel climate. You can optionally keep both the normal (air based) and comfort variant for the same physical zone (give them different names) – they will share the underlying setpoint.

```yaml
climate:
  # Standard air temperature climate
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bathroom"
    channel: 4

  # Comfort (floor-based) variant – requires a valid floor probe reading
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: "Bathroom Comfort"
    channel: 4
    use_floor_temperature: true
```

Notes:
* The comfort climate only appears in generated YAML if the floor probe was already detected (plausible >1°C and <90°C reading). If you add it manually and the probe is not yet detected it will show as unavailable until a valid value is read.
* Low / high target temperatures correspond to the controller's floor min / max limits. Adjusting them writes those limits (clamped 5–35°C, 0.5°C steps with at least 1.0°C separation enforced).
* Action (heating/idle) logic still derives from the difference between current (floor) temperature and the setpoint with a small hysteresis.

## Hardware & Wiring
* RS‑485 (A/B) from controller to a TTL↔RS‑485 adapter.
* ESP32 recommended (tested pins 16/17 for stable UART).
* Optional direction control:
  * `flow_control_pin:` supply a single GPIO tied to DE & /RE (HIGH during TX, LOW for RX). Recommended for most MAX3485/75176 style boards.
  * `tx_enable_pin:` legacy boolean driver enable (HIGH enables, left HIGH between frames). Use only if you already wired it this way; otherwise prefer `flow_control_pin`.
* If neither is specified and your transceiver auto‑enables, you can omit both.

### Choosing flow_control_pin vs tx_enable_pin
| Option | Behavior | Pros | Cons |
|--------|----------|------|------|
| `flow_control_pin` | Pulsed HIGH only while sending, LOW for receive | Minimizes bus contention; cleaner half‑duplex | Very slightly more GPIO toggling |
| `tx_enable_pin` | HIGH enables driver (often kept HIGH) | Simple if already wired | Can hold bus driver enabled longer than needed |

Prefer `flow_control_pin` for new builds. Keep `tx_enable_pin` only for backward compatibility or where hardware expects a static enable.

## Two-Phase Workflow (Recommended)

1. Generation phase (minimal config + generator package) → see `example_generate_yaml.yaml`.
2. Final phase (copied entities, generator removed) → see `example_after_generate_yaml.yaml`.

This keeps the final runtime lean while letting the component propose accurate entities.

See `INSTALLATION.md` for a detailed, step-by-step onboarding guide (generation → stitching → final deploy) plus troubleshooting tips.

Note: climates are defined explicitly under the `climate:` section using platform `wavin_ahc9000` (single channel or grouped). Optional per-channel battery sensors use `sensor: - platform: wavin_ahc9000`.

## Friendly Names
Provide any subset of:
```yaml
wavin_ahc9000:
  channel_01_friendly_name: "Bedroom"
  channel_07_friendly_name: "Office"
```
Missing entries fallback to `Zone N`.

Group climate naming (all members have friendly names):
* 2 members: `NameA & NameB`
* 3–4 members: `NameA, NameB & NameC`
* >4 members: `FirstName – LastName`
Fallback: `Zone G a&b` or `Zone G first-last`.

Single climates that belong to a generated group are still included in the full suggestion but commented out for clarity.

## Aggregation Semantics
| Property | Group Logic |
|----------|-------------|
| Current Temperature | Average of members (or floor temp for comfort variant) |
| Setpoint | Average of member setpoints |
| Action | Heating if any member heating else idle |

## Developer / Debug Tips
* Start with one known-good channel powered & paired.
* Use `logger:` level DEBUG while validating wiring.
* Temporarily raise `poll_channels_per_cycle` to accelerate discovery, then revert.
* Floor probe detection waits for plausible readings; early YAML generation may omit comfort climates—re-run later.

## Services
| Service | Purpose |
|---------|---------|
| `wavin_generate_yaml` | Build latest suggestion internally (does not push chunks) |
| `wavin_publish_yaml_text_sensors` | Generate then publish chunk sensor states (skips if no channels yet) |
| `wavin_notify_yaml_chunks` | (Reserved / minimal) triggers generation only |
| `wavin_strict_heat` | Example: force baseline config (see source) |

Chunk sensors follow naming like `sensor.wavin_yaml_climate_1` etc. See `jinjatemplate.txt` / `jinja_examples.j2` for stitching.

## Jinja Templates
Two forms are provided:
* `jinjatemplate.txt` – annotated human-readable reference.
* `jinja_examples.j2` – syntax-highlight friendly, drop into HA template editor (remove outer comments as needed).

Copy the rendered template output into your final ESPHome YAML, adjust names, then remove the generator package.

## Floor & Comfort Climates
Comfort climates appear only for channels with a detected floor probe. Names append `Comfort` to the friendly name (or `Zone N`).

## Child Lock Switches
Each thermostat channel exposes an inferred child lock bit (observed change 0x4000 → 0x4800 in the packed configuration register; mask `0x0800`). This component surfaces that as an optional per‑channel switch.

### When to Use
Lock the physical thermostat interface (prevent local user temperature changes) while still allowing Home Assistant / ESPHome to adjust setpoints programmatically. Turning the switch ON sets the lock; OFF clears it.

### YAML Example
```yaml
switch:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 9
    # type: child_lock   # optional (defaults to child_lock for now)
    name: "Office Lock"
```

Multiple channels:
```yaml
switch:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 3
    name: "Hall Lock"
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 7
    name: "Guest Lock"
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 9
    name: "Office Lock"
```

### Behavior & Notes
* Writes use a masked register update preserving unrelated bits.
* The switch publishes its optimistic state immediately; an urgent refresh confirms or corrects it within the next polling cycle.
* Safe to add/remove at any time (no reboot needed beyond normal ESPHome deployment cycle).
* If you rarely toggle locks you can omit them from the final config to keep the entity list lean – re‑add later if needed.

### Automation Samples
Lock all relevant zones at night:
```yaml
automation:
  - alias: Lock Wavin Zones At Night
    trigger:
      - platform: time
        at: "22:30:00"
    action:
      - service: switch.turn_on
        target:
          entity_id:
            - switch.office_lock
            - switch.hall_lock
            - switch.guest_lock
```

Unlock in the morning:
```yaml
automation:
  - alias: Unlock Wavin Zones Morning
    trigger:
      - platform: time
        at: "06:30:00"
    action:
      - service: switch.turn_off
        target:
          entity_id: switch.office_lock
```

### Troubleshooting
| Symptom | Suggestion |
|---------|------------|
| Switch state flips back | Underlying write failed (bus issue); check logs at DEBUG for masked write result |
| Switch always off | Channel not yet fully discovered; wait until first packed read (discovery phase) |
| No switch entity | YAML omitted switch platform or validation failed – ensure indentation & `platform: wavin_ahc9000` |

### Safety Considerations
Child lock blocks manual thermostat adjustments. Ensure you have reliable automation or a fallback (e.g., override climate setpoint) before mass‑locking all zones.

## Commented Single Climates for Group Members
When a group climate is generated, its member single-channel climates remain in the full suggestion but are commented out with an explanatory line. This keeps copy/paste flexible (just uncomment if you later decide to manage them individually).

## YAML Generator Workflow Overview

This component can generate suggested YAML for discovered channels. Because Home Assistant limits text sensor sizes, the suggestion is also exposed as chunked text sensors (each containing only entity blocks, without section headers). Use the included `jinjatemplate.txt` to stitch them back together.

### Enabling the Generator

Files involved:
- `packages/yaml_generator.on.yaml`  (full set of text sensors + services)
- `packages/yaml_generator.off.yaml` (empty stub – mostly for clarity)
- `packages/yaml_generator.yaml`     (documentation only)

In your node YAML add (uncomment while needed):

```yaml
packages:
  # Enable during onboarding to generate YAML suggestions
  yaml_generator: !include packages/yaml_generator.on.yaml
  # Optional: debug register dump service
  # wavin_debug: !include packages/wavin_debug_services.yaml
```

After you've copied the generated YAML into your permanent config, simply comment the `yaml_generator` line again to remove the helper entities and services.

### Generating & Collecting the YAML
1. Recompile & upload with the generator enabled.
2. In Home Assistant call the service: `esphome.<node_name>_wavin_publish_yaml_text_sensors` (or first `wavin_generate_yaml`).
3. Inspect sensors:
   - `sensor.wavin_yaml_climate_1` .. `sensor.wavin_yaml_climate_8`
   - `sensor.wavin_yaml_comfort_climate_1` .. (if comfort climates apply)
   - `sensor.wavin_yaml_temperature_1` .. `sensor.wavin_yaml_temperature_8`
   - `sensor.wavin_yaml_battery_1` .. `sensor.wavin_yaml_battery_8`
   - `sensor.wavin_yaml_floor_temperature_1` .. (only if floor probes detected)
4. Optionally view the single full suggestion sensor: `sensor.wavin_yaml_suggestion` (may be truncated for very large outputs in HA – use chunks when in doubt).
5. Open `jinjatemplate.txt` and use the “All-in-one” Jinja macro to stitch chunks back into a cohesive YAML block (adds section headers + indentation).

### Why Not a Substitution Toggle?
Early versions used a substitution (`YAML_GEN_STATE`) to switch between on/off files. Remote GitHub package includes cannot use substitutions in the `files:` list, leading to confusion. The project now favors a simple comment/uncomment pattern for reliability both locally and in remote includes.

### Floor Probe Driven Additions / Comfort
If at least one floor probe is plausibly detected (>1.0°C and <90°C after startup), the generator adds the relevant floor temperature sensor entries. If you trigger the service very early and they are missing, trigger again later.

### Readiness Indicator
You can optionally add a `yaml_ready` binary sensor to know when discovery has progressed enough to get stable YAML suggestions:

```yaml
binary_sensor:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    type: yaml_ready
    name: "Wavin YAML Ready"
```

Once you have migrated the suggested YAML entities you want, disable the generator include to declutter.

### Comfort Setpoint (Read-Only) Sensor

If you do not want writable number entities but still want to surface the current comfort (manual) setpoint as a sensor for dashboards or historical graphs, you can add a `comfort_setpoint` sensor type:

```yaml
sensor:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 3
    type: comfort_setpoint
    name: "Zone 3 Comfort Setpoint"
```

This sensor publishes the latest manual/comfort setpoint (register 0x00) that the integration already reads for the climate entity. It is read-only; changing it requires using the climate entity or a number entity if enabled in another branch.

### Floor Temperature Sensor

If your thermostat provides a floor probe value, you can expose it separately:

```yaml
sensor:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 3
    type: floor_temperature
    name: "Zone 3 Floor Temperature"
```

This reads element index 0x05 (same scaling as air). A plausibility filter (>1..90°C) is applied; invalid or missing values result in the sensor staying unavailable.

Automatic detection: The component now marks a channel as having a floor sensor only after at least one plausible, non-zero floor reading is observed (> 1.0°C and < 90°C). Until then, any configured `floor_temperature` sensor will remain unavailable (rather than showing 0.0). The YAML generation services (`_wavin_generate_yaml` / `_wavin_publish_yaml_text_sensors`) will include floor temperature sensor entity suggestions only for channels where a valid floor probe has already been detected. If you generate YAML immediately after boot and a floor probe wasn’t yet detected, just trigger the service again later after some polling cycles.

Chunked YAML: If at least one floor probe is detected, an additional chunk set is exposed via the `get_yaml_floor_temperature_chunk` API internally (usable in future text sensors). If you add corresponding text sensors (e.g. `wavin_yaml_floor_temperature_1..8`), you can stitch them with the same Jinja approach as other sensor chunks by adding an extra `sensor:` header and their content.

### YAML Readiness Indicator (binary_sensor)

To know when it's reasonable to generate and copy the suggested YAML, a readiness indicator is exposed as a binary sensor. It turns on once the hub has:
- Discovered at least one active channel (primary element present and no TP lost)
- Completed at least one full element block read for all discovered channels

Add this to your config if desired:

```yaml
binary_sensor:
  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    type: yaml_ready
    name: "Wavin YAML Ready"
```

# Use Ustepper ESP32 Pico unit:

### For easy start with controlling the Wavin AHC 9000 unit, this module below is a good way to start 

https://www.ustepper.com/shop/#!/products/esphome-modbus-module/variant/615309

Below is some ESPHOME inspiration for the Ustepper module (specific) together with Wavin AHC 9000. The code supports climate, temp/battery sensors and also the child locks for the room thermostats. The code have been added with a workaround for "valve-motion", so the valves dont get stuck or leaking (if they have not been used over some time). This "valve-motion" can be set on a given day and time and also how long the "valve-motion" should run. Furthermore the code have been updated, so the desired climate settings does not go away if the esp32 restarts/power-failure, and after a valve-motion has been running - it "saves" the last known temperature / climate settings.

ESPHOME is quite powerfull and there is many possibilites for enhancing the user experience !!!

```yaml
globals:
  # -----------------------------------------------------------
  # GLOBAL VARIABLES TO STORE TEMPERATURES BEFORE VALVE MOTION
  # -----------------------------------------------------------
  
  # These variables store the last set target_temperature for each channel.
  # We use 'initial_value: 21.0' as a safety default if the system starts for the first time.
  
  - id: ch1_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch2_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch3_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch4_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch5_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch6_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch7_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch8_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch9_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch10_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true
  - id: ch11_temp_before
    type: float
    initial_value: '21.5'
    restore_value: true

esphome:
  name: 'wavin'    

esp32:
  board: pico32
  framework:
    type: esp-idf     

external_components:
  - source: github://heinekmadsen/esphome_wavinahc9000v3
    components: [wavin_ahc9000]
    refresh: 0s 

uart:
  id: uart_wavin
  tx_pin: 14
  rx_pin: 13
  baud_rate: 38400
  stop_bits: 1
  parity: NONE

wavin_ahc9000:
  id: wavin
  uart_id: uart_wavin
  update_interval: 5s
  flow_control_pin: 26
  channel_01_friendly_name: "Værksted"
  channel_02_friendly_name: "Victor"
  channel_03_friendly_name: "Gæstebadeværelse"
  channel_04_friendly_name: "Bryggers"
  channel_05_friendly_name: "Kontor"
  channel_06_friendly_name: "Køkken"
  channel_07_friendly_name: "Køkken Alrum"
  channel_08_friendly_name: "Badeværelse"
  channel_09_friendly_name: "Soveværelse"
  channel_10_friendly_name: "Stue"
  channel_11_friendly_name: "Emil"

climate:

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_1
    name: ${channel_01_friendly_name} Climate
    channel: 1

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_2
    name: ${channel_02_friendly_name} Climate
    channel: 2

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_3
    name: ${channel_03_friendly_name} Climate
    channel: 3

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_4
    name: ${channel_04_friendly_name} Climate 
    channel: 4

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_5
    name: ${channel_05_friendly_name} Climate
    channel: 5

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_6
    name: ${channel_06_friendly_name} Climate
    channel: 6

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_7
    name: ${channel_07_friendly_name} Climate
    channel: 7

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_8
    name: ${channel_08_friendly_name} Climate
    channel: 8

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_9
    name: ${channel_09_friendly_name} Climate
    channel: 9

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_10
    name: ${channel_10_friendly_name} Climate
    channel: 10 

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    id: channel_11
    name: ${channel_11_friendly_name} Climate
    channel: 11                 

sensor:

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_01_friendly_name} Battery
    channel: 1
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_01_friendly_name} Temperature
    channel: 1
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_02_friendly_name} Battery
    channel: 2
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_02_friendly_name} Temperature
    channel: 2
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_03_friendly_name} Battery
    channel: 3
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_03_friendly_name} Temperature
    channel: 3
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_04_friendly_name} Battery
    channel: 4
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_04_friendly_name} Temperature
    channel: 4
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_05_friendly_name} Battery
    channel: 5
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_05_friendly_name} Temperature
    channel: 5
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_06_friendly_name} Battery
    channel: 6
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_06_friendly_name} Temperature
    channel: 6
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_07_friendly_name} Battery
    channel: 7
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_07_friendly_name} Temperature
    channel: 7
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_08_friendly_name} Battery
    channel: 8
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_08_friendly_name} Temperature
    channel: 8
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_09_friendly_name} Battery
    channel: 9
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_09_friendly_name} Temperature
    channel: 9
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_10_friendly_name} Battery
    channel: 10
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_10_friendly_name} Temperature
    channel: 10
    type: temperature
    device_class: temperature

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_11_friendly_name} Battery
    channel: 11
    type: battery
    entity_category: diagnostic

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    name: ${channel_11_friendly_name} Temperature
    channel: 11
    type: temperature
    device_class: temperature
                                 
  - platform: internal_temperature
    name: "Wavin GW Internal Temperature"

switch:

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 1
    type: child_lock
    name: ${channel_01_friendly_name} Lock

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 2
    type: child_lock
    name: ${channel_02_friendly_name} Lock 

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 3
    type: child_lock
    name: ${channel_03_friendly_name} Lock 

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 4
    type: child_lock
    name: ${channel_04_friendly_name} Lock        

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 5
    type: child_lock
    name: ${channel_05_friendly_name} Lock

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 6
    type: child_lock
    name: ${channel_06_friendly_name} Lock 

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 7
    type: child_lock
    name: ${channel_07_friendly_name} Lock

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 8
    type: child_lock
    name: ${channel_08_friendly_name} Lock

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 9
    type: child_lock
    name: ${channel_09_friendly_name} Lock

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 10
    type: child_lock
    name: ${channel_10_friendly_name} Lock 

  - platform: wavin_ahc9000
    wavin_ahc9000_id: wavin
    channel: 11
    type: child_lock
    name: ${channel_11_friendly_name} Lock              

button:
  - platform: template
    name: "Master Reset Temperatur"
    id: master_reset_temp_button
    icon: "mdi:thermometer-lines"
    on_press:
      # When the button is pressed, the script below is called
      - script.execute: set_all_to_default_temp 

time:
  - platform: homeassistant
    id: homeassistant_time
    on_time:
      - seconds: 0 
        then:
          - lambda: |-
            // 1. Get the current time and the selected time
            auto now = id(homeassistant_time).now();
            auto desired_time = id(valve_motion_datetime).state_as_esptime();
            
            // 2. Get the selected weekday as a string
            std::string selected_day_str = id(valve_motion_day_select).state;

            // 3. Get the selected weekday as INDEX by finding the index of the string (IMPORTANT!)
            auto selected_day_index = id(valve_motion_day_select).index_of(selected_day_str);

            // 4. Get the current weekday. ESPHome's day_of_week: 1=Sunday, 2=Monday, etc.
            // We subtract 1 to get 0=Sunday, 1=Monday, etc. to match the select index (0-6).
            auto current_day_index = now.day_of_week - 1;

            // Checks that .index_of() actually found a valid value (optional but good for robustness)
            if (!selected_day_index.has_value()) {
              ESP_LOGW("valve", "Selected weekday could not be found in the list!");
              return;
            }

            // 5. Check for conditions (Time & Weekday)
            if (now.hour == desired_time.hour && now.minute == desired_time.minute) {
              // Time matches. Now check the weekday.
              if (current_day_index == selected_day_index.value()) {
                // All conditions match! Run the motion routine.
                id(valve_motion_routine).execute();
              }
            }

script:
  - id: heat_up_all
    then:
      # SAVE THE CURRENT TEMPERATURES IN GLOBAL VARIABLES
      - globals.set: { id: ch1_temp_before, value: !lambda "return id(channel_1).target_temperature;" }
      - globals.set: { id: ch2_temp_before, value: !lambda "return id(channel_2).target_temperature;" }
      - globals.set: { id: ch3_temp_before, value: !lambda "return id(channel_3).target_temperature;" }
      - globals.set: { id: ch4_temp_before, value: !lambda "return id(channel_4).target_temperature;" }
      - globals.set: { id: ch5_temp_before, value: !lambda "return id(channel_5).target_temperature;" }
      - globals.set: { id: ch6_temp_before, value: !lambda "return id(channel_6).target_temperature;" }
      - globals.set: { id: ch7_temp_before, value: !lambda "return id(channel_7).target_temperature;" }
      - globals.set: { id: ch8_temp_before, value: !lambda "return id(channel_8).target_temperature;" }
      - globals.set: { id: ch9_temp_before, value: !lambda "return id(channel_9).target_temperature;" }
      - globals.set: { id: ch10_temp_before, value: !lambda "return id(channel_10).target_temperature;" }
      - globals.set: { id: ch11_temp_before, value: !lambda "return id(channel_11).target_temperature;" }

      # SET TO MOTION TEMPERATURE
      - climate.control: { id: channel_1, target_temperature: 35.0 }
      - climate.control: { id: channel_2, target_temperature: 35.0 }
      - climate.control: { id: channel_3, target_temperature: 35.0 }
      - climate.control: { id: channel_4, target_temperature: 35.0 }
      - climate.control: { id: channel_5, target_temperature: 35.0 }
      - climate.control: { id: channel_6, target_temperature: 35.0 }
      - climate.control: { id: channel_7, target_temperature: 35.0 }
      - climate.control: { id: channel_8, target_temperature: 35.0 }
      - climate.control: { id: channel_9, target_temperature: 35.0 }
      - climate.control: { id: channel_10, target_temperature: 35.0 }
      - climate.control: { id: channel_11, target_temperature: 35.0 }

  - id: restore_normal
    then:
      # RESTORE THE SAVED TEMPERATURES
      - climate.control: { id: channel_1, target_temperature: !lambda "return id(ch1_temp_before);" }
      - climate.control: { id: channel_2, target_temperature: !lambda "return id(ch2_temp_before);" }
      - climate.control: { id: channel_3, target_temperature: !lambda "return id(ch3_temp_before);" }
      - climate.control: { id: channel_4, target_temperature: !lambda "return id(ch4_temp_before);" }
      - climate.control: { id: channel_5, target_temperature: !lambda "return id(ch5_temp_before);" }
      - climate.control: { id: channel_6, target_temperature: !lambda "return id(ch6_temp_before);" }
      - climate.control: { id: channel_7, target_temperature: !lambda "return id(ch7_temp_before);" }
      - climate.control: { id: channel_8, target_temperature: !lambda "return id(ch8_temp_before);" }
      - climate.control: { id: channel_9, target_temperature: !lambda "return id(ch9_temp_before);" }
      - climate.control: { id: channel_10, target_temperature: !lambda "return id(ch10_temp_before);" }
      - climate.control: { id: channel_11, target_temperature: !lambda "return id(ch11_temp_before);" }

  - id: set_all_to_default_temp
    # "Set All Zones to 21.5°C"
    then:
      - climate.control: { id: channel_1, target_temperature: 21.5 }
      - climate.control: { id: channel_2, target_temperature: 21.5 }
      - climate.control: { id: channel_3, target_temperature: 21.5 }
      - climate.control: { id: channel_4, target_temperature: 21.5 }
      - climate.control: { id: channel_5, target_temperature: 21.5 }
      - climate.control: { id: channel_6, target_temperature: 21.5 }
      - climate.control: { id: channel_7, target_temperature: 21.5 }
      - climate.control: { id: channel_8, target_temperature: 21.5 }
      - climate.control: { id: channel_9, target_temperature: 21.5 }
      - climate.control: { id: channel_10, target_temperature: 21.5 }
      - climate.control: { id: channel_11, target_temperature: 21.5 }      

  - id: valve_motion_routine
    then:
      - script.execute: heat_up_all
      - delay: !lambda "return id(valve_motion_duration).state * 60 * 1000;" # Dynamic duration in ms
      - script.execute: restore_normal

datetime:
  - platform: template
    name: Valve Motion Time
    id: valve_motion_datetime
    type: TIME # We only need the time
    optimistic: true
    restore_value: true

select:
  - platform: template
    name: Valve Motion Day
    id: valve_motion_day_select
    # Set the available weekdays. Important: they must match ESPHome's/Home Assistant's weekday formats!
    options:
      - "Søndag"
      - "Mandag"
      - "Tirsdag"
      - "Onsdag"
      - "Torsdag"
      - "Fredag"
      - "Lørdag"
    initial_option: "Mandag" # Default choice (Monday)
    optimistic: true 
    restore_value: true 

number:
  - platform: template
    name: Valve Motion Duration
    id: valve_motion_duration
    min_value: 1
    max_value: 20
    step: 1
    unit_of_measurement: "min"
    initial_value: 10
    optimistic: true
    restore_value: true
```


## Future Ideas
* Optional toggle to suppress (instead of comment) grouped member single climates in generated YAML.
* Additional diagnostics (timing stats, packet counters) exposed via sensors.
* Entity category refinement for sensors (diagnostic vs primary).
* Bulk child lock service (lock/unlock multiple channels in one call).

## Disclaimer
Community-driven integration; use at your own risk. Not affiliated with Wavin / Jablotron.
