"""Configurable BQ25672 voltage and current limits in SI units."""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    ENTITY_CATEGORY_CONFIG,
)

from . import (
    BQ25672_COMPONENT_SCHEMA,
    CONF_BQ25672_ID,
    NUMBER_SETTINGS,
    NumberType,
    bq25672_ns,
    validate_number_value,
)

DEPENDENCIES = ["bq25672"]

BQ25672Number = bq25672_ns.class_("BQ25672Number", number.Number)


def validate_bounds(config):
    if config[CONF_MIN_VALUE] > config[CONF_MAX_VALUE]:
        raise cv.Invalid("min_value must not exceed max_value", path=[CONF_MIN_VALUE])
    return config


def setting_schema(key, setting):
    validator = validate_number_value(key)
    return cv.All(
        number.number_schema(
            BQ25672Number,
            unit_of_measurement=setting.unit,
            device_class=setting.device_class,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=setting.icon,
        ).extend(
            {
                # Default to input boxes; YAML can still override with mode: slider.
                cv.Optional(CONF_MODE, default="BOX"): cv.enum(
                    number.NUMBER_MODES, upper=True
                ),
                cv.Optional(CONF_MIN_VALUE, default=setting.minimum): validator,
                cv.Optional(CONF_MAX_VALUE, default=setting.maximum): validator,
            }
        ),
        validate_bounds,
    )


CONFIG_SCHEMA = BQ25672_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(key): setting_schema(key, setting)
        for key, setting in NUMBER_SETTINGS.items()
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25672_ID])
    for key, setting in NUMBER_SETTINGS.items():
        if key in config:
            conf = config[key]
            entity = await number.new_number(
                conf,
                min_value=conf[CONF_MIN_VALUE],
                max_value=conf[CONF_MAX_VALUE],
                step=setting.step,
            )
            type_ = getattr(NumberType, f"NUMBER_{key.upper()}")
            cg.add(entity.set_parent(parent))
            cg.add(entity.set_type(type_))
            cg.add(parent.set_number(type_, entity))
