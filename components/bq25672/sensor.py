"""Voltage, current and ADC measurements from the BQ25672."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from . import BQ25672_COMPONENT_SCHEMA, CONF_BQ25672_ID, SensorType

DEPENDENCIES = ["bq25672"]

VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSORS = {
    "battery_voltage": VOLTAGE_SCHEMA,
    "system_voltage": VOLTAGE_SCHEMA,
    "bus_voltage": VOLTAGE_SCHEMA,
    "input_1_voltage": VOLTAGE_SCHEMA,
    "input_2_voltage": VOLTAGE_SCHEMA,
    "battery_current": CURRENT_SCHEMA,
    "input_current": CURRENT_SCHEMA,
    "die_temperature": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "ntc_percentage": sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        icon="mdi:thermometer",
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "dp_voltage": VOLTAGE_SCHEMA,
    "dn_voltage": VOLTAGE_SCHEMA,
    "input_current_limit": CURRENT_SCHEMA,
}

CONFIG_SCHEMA = BQ25672_COMPONENT_SCHEMA.extend(
    {cv.Optional(key): schema for key, schema in SENSORS.items()}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25672_ID])
    for key in SENSORS:
        if key in config:
            entity = await sensor.new_sensor(config[key])
            cg.add(
                parent.set_sensor(getattr(SensorType, f"SENSOR_{key.upper()}"), entity)
            )
