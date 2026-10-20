"""Explicit controls for BQ25672 charging and operating modes."""

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_RESTORE_MODE, ENTITY_CATEGORY_CONFIG

from . import (
    BQ25672_COMPONENT_SCHEMA,
    CONF_BQ25672_ID,
    SWITCH_KEYS,
    SwitchType,
    bq25672_ns,
)

DEPENDENCIES = ["bq25672"]

BQ25672Switch = bq25672_ns.class_("BQ25672Switch", switch.Switch)


def reject_restore_mode(config):
    if isinstance(config, dict) and CONF_RESTORE_MODE in config:
        raise cv.Invalid(
            "BQ25672 switches read their state from the charger. Configure a "
            "startup state under bq25672 instead of using restore_mode.",
            path=[CONF_RESTORE_MODE],
        )
    return config


# The base platform requires a restore-mode enum during code generation. Inject
# DISABLED as its internal default, while rejecting any user-supplied setting.
BQ25672_SWITCH_SCHEMA = cv.All(
    reject_restore_mode,
    switch.switch_schema(
        BQ25672Switch,
        block_inverted=True,
        default_restore_mode="DISABLED",
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:toggle-switch",
    ),
)

CONFIG_SCHEMA = BQ25672_COMPONENT_SCHEMA.extend(
    {cv.Optional(key): BQ25672_SWITCH_SCHEMA for key in SWITCH_KEYS}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25672_ID])
    for key in SWITCH_KEYS:
        if key in config:
            entity = await switch.new_switch(config[key])
            type_ = getattr(SwitchType, f"SWITCH_{key.upper()}")
            cg.add(entity.set_parent(parent))
            cg.add(entity.set_type(type_))
            cg.add(parent.set_switch(type_, entity))
