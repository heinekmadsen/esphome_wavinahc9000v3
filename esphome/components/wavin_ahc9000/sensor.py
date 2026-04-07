import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_ICON,
    UNIT_PERCENT,
    DEVICE_CLASS_BATTERY,
    ICON_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    STATE_CLASS_MEASUREMENT,
)

from . import WavinAHC9000

CONF_PARENT_ID = "wavin_ahc9000_id"
CONF_CHANNEL = "channel"
CONF_TYPE = "type"

# Per-type sensor field defaults (plain dicts, strings only — will be
# validated and converted to proper ESPHome types by sensor.sensor_schema())
_SENSOR_DEFAULTS = {
    "battery": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_PERCENT,
        CONF_ICON: ICON_BATTERY,
        CONF_ACCURACY_DECIMALS: 0,
        CONF_DEVICE_CLASS: DEVICE_CLASS_BATTERY,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    },
    "temperature": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    },
    "comfort_setpoint": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    },
    "floor_temperature": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    },
    "floor_min_temperature": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    },
    "floor_max_temperature": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    },
}

# Extra wavin-specific fields shared across both schema passes
_WAVIN_FIELDS = {
    cv.GenerateID(CONF_PARENT_ID): cv.use_id(WavinAHC9000),
    cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=16),
    cv.Required(CONF_TYPE): cv.one_of(*_SENSOR_DEFAULTS.keys(), lower=True),
}


def _apply_sensor_defaults(config):
    """Fill in per-type defaults BEFORE sensor.sensor_schema() validates them.

    This ensures string values like STATE_CLASS_MEASUREMENT are converted to
    proper ESPHome enum types by the schema validator that follows.
    """
    defaults = _SENSOR_DEFAULTS[config[CONF_TYPE]]
    config = dict(config)
    for key, value in defaults.items():
        if key not in config:
            config[key] = value
    return config


CONFIG_SCHEMA = cv.All(
    # Pass 1: validate wavin-specific fields; allow all other keys through
    # so that sensor fields (name, id, …) are not rejected.
    cv.Schema(_WAVIN_FIELDS, extra=cv.ALLOW_EXTRA),
    # Fill in per-type defaults while values are still plain strings.
    _apply_sensor_defaults,
    # Pass 2: full sensor schema validation — converts strings to typed
    # ESPHome values (e.g. "measurement" → StateClass enum) exactly once.
    sensor.sensor_schema().extend(_WAVIN_FIELDS),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    sens = await sensor.new_sensor(config)

    if config[CONF_TYPE] == "battery":
        cg.add(hub.add_channel_battery_sensor(config[CONF_CHANNEL], sens))
    elif config[CONF_TYPE] == "comfort_setpoint":
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
