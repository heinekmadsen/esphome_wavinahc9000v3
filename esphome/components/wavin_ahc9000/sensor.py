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

def _apply_defaults(config):
    t = config[CONF_TYPE]

    if t == "battery":
        config.setdefault("device_class", DEVICE_CLASS_BATTERY)
        config.setdefault("unit_of_measurement", UNIT_PERCENT)
        config.setdefault("icon", ICON_BATTERY)
        config.setdefault("accuracy_decimals", 0)
    else:
        config.setdefault("device_class", DEVICE_CLASS_TEMPERATURE)
        config.setdefault("unit_of_measurement", UNIT_CELSIUS)
        config.setdefault("accuracy_decimals", 1)

    return config

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema().extend(
        {
            cv.GenerateID(CONF_PARENT_ID): cv.use_id(WavinAHC9000),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=16),
            cv.Required(CONF_TYPE): cv.one_of(
                "battery",
                "temperature",
                "comfort_setpoint",
                "floor_temperature",
                "floor_min_temperature",
                "floor_max_temperature",
                lower=True,
            ),
        }
    ),
    _apply_defaults,
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    sens = await sensor.new_sensor(config)

    if config[CONF_TYPE] == "battery":
        cg.add(hub.add_channel_battery_sensor(config[CONF_CHANNEL], sens))
    else:
        if config[CONF_TYPE] == "comfort_setpoint":
            cg.add(hub.add_channel_comfort_setpoint_sensor(config[CONF_CHANNEL], sens))
        elif config[CONF_TYPE] == "floor_temperature":
            cg.add(hub.add_channel_floor_temperature_sensor(config[CONF_CHANNEL], sens))
        elif config[CONF_TYPE] == "floor_min_temperature":
            cg.add(hub.add_channel_floor_min_temperature_sensor(config[CONF_CHANNEL], sens))
        elif config[CONF_TYPE] == "floor_max_temperature":
            cg.add(hub.add_channel_floor_max_temperature_sensor(config[CONF_CHANNEL], sens))
        else:
            cg.add(hub.add_channel_temperature_sensor(config[CONF_CHANNEL], sens))

    cg.add(hub.add_active_channel(config[CONF_CHANNEL]))
