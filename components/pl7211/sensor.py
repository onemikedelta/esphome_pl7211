import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_VOLTAGE, CONF_CURRENT, CONF_POWER, CONF_POWER_FACTOR,
    UNIT_VOLT, UNIT_AMPERE, UNIT_WATT, UNIT_EMPTY, UNIT_HERTZ,
    DEVICE_CLASS_VOLTAGE, DEVICE_CLASS_CURRENT, DEVICE_CLASS_POWER, DEVICE_CLASS_POWER_FACTOR, DEVICE_CLASS_FREQUENCY,
    STATE_CLASS_MEASUREMENT
)
from . import pl7211_ns, CONF_PL7211_ID, PL7211Component

CONF_FREQUENCY = "frequency"

# Define the C++ sensor objects
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_PL7211_ID): cv.use_id(PL7211Component),
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_POWER): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
})

async def to_code(config):
    hub = await cg.get_variable(config[CONF_PL7211_ID])
    
    for key in [CONF_VOLTAGE, CONF_CURRENT, CONF_POWER, CONF_POWER_FACTOR, CONF_FREQUENCY]:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(hub, f"set_{key}_sensor")(sens))