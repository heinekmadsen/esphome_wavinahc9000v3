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
