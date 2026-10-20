"""Human-readable charging, input detection and fault status."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import BQ25672_COMPONENT_SCHEMA, CONF_BQ25672_ID, TextSensorType

DEPENDENCIES = ["bq25672"]

TEXT_SENSORS = {
    "charge_status": text_sensor.text_sensor_schema(icon="mdi:battery-charging"),
    "vbus_status": text_sensor.text_sensor_schema(
        icon="mdi:usb-port", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    "ico_status": text_sensor.text_sensor_schema(
        icon="mdi:current-dc", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    "fault_status": text_sensor.text_sensor_schema(
        icon="mdi:alert-circle", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

CONFIG_SCHEMA = BQ25672_COMPONENT_SCHEMA.extend(
    {cv.Optional(key): schema for key, schema in TEXT_SENSORS.items()}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25672_ID])
    for key in TEXT_SENSORS:
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(
                parent.set_text_sensor(
                    getattr(TextSensorType, f"TEXT_SENSOR_{key.upper()}"), entity
                )
            )
