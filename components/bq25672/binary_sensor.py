"""Live charger, battery and fault status bits from the BQ25672."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import BQ25672_COMPONENT_SCHEMA, CONF_BQ25672_ID, BinarySensorType

DEPENDENCIES = ["bq25672"]

STATUS_SCHEMA = binary_sensor.binary_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC
)
PROBLEM_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

BINARY_SENSORS = {
    "online": binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "charging": binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_BATTERY_CHARGING
    ),
    "charge_done": binary_sensor.binary_sensor_schema(icon="mdi:battery-check"),
    "power_good": STATUS_SCHEMA,
    "bus_present": STATUS_SCHEMA,
    "input_1_present": STATUS_SCHEMA,
    "input_2_present": STATUS_SCHEMA,
    "battery_present": STATUS_SCHEMA,
    "input_voltage_regulation": STATUS_SCHEMA,
    "input_current_regulation": STATUS_SCHEMA,
    "thermal_regulation": STATUS_SCHEMA,
    "min_system_voltage_regulation": STATUS_SCHEMA,
    "battery_hot": PROBLEM_SCHEMA,
    "battery_warm": STATUS_SCHEMA,
    "battery_cool": STATUS_SCHEMA,
    "battery_cold": PROBLEM_SCHEMA,
    "battery_too_low_for_otg": PROBLEM_SCHEMA,
    "watchdog_expired": PROBLEM_SCHEMA,
    "fault": PROBLEM_SCHEMA,
}

CONFIG_SCHEMA = BQ25672_COMPONENT_SCHEMA.extend(
    {cv.Optional(key): schema for key, schema in BINARY_SENSORS.items()}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25672_ID])
    for key in BINARY_SENSORS:
        if key in config:
            entity = await binary_sensor.new_binary_sensor(config[key])
            cg.add(
                parent.set_binary_sensor(
                    getattr(BinarySensorType, f"BINARY_SENSOR_{key.upper()}"), entity
                )
            )
