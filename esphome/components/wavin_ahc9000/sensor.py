import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_PERCENT,
    DEVICE_CLASS_BATTERY,
    ICON_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
)

from . import WavinAHC9000

CONF_PARENT_ID = "wavin_ahc9000_id"
CONF_CHANNEL = "channel"
CONF_TYPE = "type"

CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_PARENT_ID): cv.use_id(WavinAHC9000),
        cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=16),
        cv.Required(CONF_TYPE): cv.one_of(
            "battery", 
            "temperature", 
            "comfort_setpoint", 
            "floor_temperature", 
            "floor_min_temperature", 
            "floor_max_temperature",
            "output",          # NEW: valve position 0-100%
            "comm_health",     # NEW: Modbus communication health %
            lower=True
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    sens = await sensor.new_sensor(config)
    sensor_type = config[CONF_TYPE]
    
    # NEW: Comm health sensor (hub-level, no channel required)
    if sensor_type == "comm_health":
        cg.add(sens.set_device_class(""))  # Generic sensor
        cg.add(sens.set_unit_of_measurement(UNIT_PERCENT))
        cg.add(sens.set_accuracy_decimals(1))
        cg.add(sens.set_icon("mdi:progress-check"))
        cg.add(hub.set_comm_health_sensor(sens))
        return
    
    # All channel-based sensors require CONF_CHANNEL
    if CONF_CHANNEL not in config:
        raise cv.RequiredFieldMissing(CONF_CHANNEL)
    
    ch = config[CONF_CHANNEL]
    
    # Battery sensor
    if sensor_type == "battery":
        cg.add(sens.set_device_class(DEVICE_CLASS_BATTERY))
        cg.add(sens.set_unit_of_measurement(UNIT_PERCENT))
        cg.add(sens.set_icon(ICON_BATTERY))
        cg.add(sens.set_accuracy_decimals(0))
        cg.add(hub.add_channel_battery_sensor(ch, sens))
    
    # NEW: Output/valve position sensor
    elif sensor_type == "output":
        cg.add(sens.set_unit_of_measurement(UNIT_PERCENT))
        cg.add(sens.set_accuracy_decimals(0))
        cg.add(sens.set_icon("mdi:valve"))
        cg.add(hub.add_channel_output_sensor(ch, sens))
    
    # Temperature-based sensors
    elif sensor_type in ("temperature", "comfort_setpoint", "floor_temperature", 
                         "floor_min_temperature", "floor_max_temperature"):
        cg.add(sens.set_device_class(DEVICE_CLASS_TEMPERATURE))
        cg.add(sens.set_unit_of_measurement(UNIT_CELSIUS))
        cg.add(sens.set_accuracy_decimals(1))
        
        if sensor_type == "comfort_setpoint":
            cg.add(hub.add_channel_comfort_setpoint_sensor(ch, sens))
        elif sensor_type == "floor_temperature":
            cg.add(hub.add_channel_floor_temperature_sensor(ch, sens))
        elif sensor_type == "floor_min_temperature":
            cg.add(hub.add_channel_floor_min_temperature_sensor(ch, sens))
        elif sensor_type == "floor_max_temperature":
            cg.add(hub.add_channel_floor_max_temperature_sensor(ch, sens))
        else:  # temperature
            cg.add(hub.add_channel_temperature_sensor(ch, sens))
    
    # Mark channel as active for polling
    cg.add(hub.add_active_channel(ch))