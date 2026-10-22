"""Native ESPHome integration for the Texas Instruments BQ25672 charger."""

import math
from typing import NamedTuple

import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    UNIT_AMPERE,
    UNIT_VOLT,
)

DOMAIN = "bq25672"
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "switch", "number"]
MULTI_CONF = True

CONF_BQ25672_ID = "bq25672_id"
CONF_CELL_COUNT = "cell_count"
CONF_ADC_RESOLUTION = "adc_resolution"
CONF_ADC_AVERAGING = "adc_averaging"
CONF_BATTERY_CURRENT_SENSING = "battery_current_sensing"

bq25672_ns = cg.esphome_ns.namespace(DOMAIN)
BQ25672Component = bq25672_ns.class_(
    "BQ25672Component", cg.PollingComponent, i2c.I2CDevice
)
SensorType = bq25672_ns.enum("SensorType", is_class=True)
BinarySensorType = bq25672_ns.enum("BinarySensorType", is_class=True)
TextSensorType = bq25672_ns.enum("TextSensorType", is_class=True)
SwitchType = bq25672_ns.enum("SwitchType", is_class=True)
NumberType = bq25672_ns.enum("NumberType", is_class=True)


class NumberSetting(NamedTuple):
    """A hardware field expressed in volts or amperes."""

    minimum: float
    maximum: float
    step: float
    offset: float
    unit: str
    device_class: str
    icon: str


NUMBER_SETTINGS = {
    "charge_voltage": NumberSetting(
        3.0, 18.8, 0.01, 0.0, UNIT_VOLT, DEVICE_CLASS_VOLTAGE, "mdi:battery-charging"
    ),
    "charge_current": NumberSetting(
        0.05, 3.0, 0.01, 0.0, UNIT_AMPERE, DEVICE_CLASS_CURRENT, "mdi:current-dc"
    ),
    "input_current_limit": NumberSetting(
        0.1, 3.3, 0.01, 0.0, UNIT_AMPERE, DEVICE_CLASS_CURRENT, "mdi:current-dc"
    ),
    "input_voltage_limit": NumberSetting(
        3.6, 22.0, 0.1, 0.0, UNIT_VOLT, DEVICE_CLASS_VOLTAGE, "mdi:flash"
    ),
    "min_system_voltage": NumberSetting(
        2.5, 16.0, 0.25, 2.5, UNIT_VOLT, DEVICE_CLASS_VOLTAGE, "mdi:flash"
    ),
    "precharge_current": NumberSetting(
        0.04, 2.0, 0.04, 0.0, UNIT_AMPERE, DEVICE_CLASS_CURRENT, "mdi:current-dc"
    ),
    "termination_current": NumberSetting(
        0.04, 1.0, 0.04, 0.0, UNIT_AMPERE, DEVICE_CLASS_CURRENT, "mdi:current-dc"
    ),
    "otg_voltage": NumberSetting(
        5.0, 12.0, 0.01, 2.8, UNIT_VOLT, DEVICE_CLASS_VOLTAGE, "mdi:usb-port"
    ),
    "otg_current_limit": NumberSetting(
        0.16, 2.0, 0.04, 0.0, UNIT_AMPERE, DEVICE_CLASS_CURRENT, "mdi:current-dc"
    ),
    "recharge_voltage_threshold": NumberSetting(
        0.05, 0.8, 0.05, 0.0, UNIT_VOLT, DEVICE_CLASS_VOLTAGE, "mdi:battery-sync"
    ),
}

SWITCH_KEYS = (
    "charging_enabled",
    "termination_enabled",
    "hiz_mode",
    "ico_enabled",
    "mppt_enabled",
    "otg_enabled",
    "inputs_disconnected",
)

ADC_RESOLUTIONS = {15: 0, 14: 1, 13: 2, 12: 3}
CELL_VOLTAGE_RANGES = {
    1: (3.0, 4.99),
    2: (5.0, 9.99),
    3: (10.0, 13.99),
    4: (14.0, 18.8),
}


def validate_number_value(key):
    """Accept SI units and require a value the register can represent exactly."""
    setting = NUMBER_SETTINGS[key]
    parse = cv.voltage if setting.unit == UNIT_VOLT else cv.current

    def validator(value):
        try:
            value = parse(value)
        except (TypeError, ValueError, OverflowError) as error:
            raise cv.Invalid(
                f"{key} must be a finite numeric value, optionally with "
                f"{setting.unit} units"
            ) from error
        if not math.isfinite(value):
            raise cv.Invalid(f"{key} must be finite")
        if value < setting.minimum - 1e-9 or value > setting.maximum + 1e-9:
            raise cv.Invalid(
                f"{key} must be between {setting.minimum:g} and "
                f"{setting.maximum:g} {setting.unit}"
            )
        steps = (value - setting.offset) / setting.step
        nearest = round(steps)
        if not math.isclose(steps, nearest, rel_tol=0.0, abs_tol=1e-7):
            raise cv.Invalid(
                f"{key} must align to {setting.step:g} {setting.unit} steps "
                f"from {setting.offset:g} {setting.unit}"
            )
        # Normalize harmless binary floating-point noise from unit conversion.
        return round(setting.offset + nearest * setting.step, 9)

    return validator


def validate_hub_config(config):
    if CONF_CELL_COUNT in config:
        for key in ("charge_voltage", "charge_current"):
            if key not in config:
                raise cv.Invalid(
                    f"{key} is required when cell_count is specified, because "
                    "writing the cell count resets both charging limits",
                    path=[CONF_CELL_COUNT],
                )
        minimum, maximum = CELL_VOLTAGE_RANGES[config[CONF_CELL_COUNT]]
        if not minimum <= config["charge_voltage"] <= maximum:
            raise cv.Invalid(
                f"charge_voltage must be between {minimum:g} and {maximum:g} V "
                f"for cell_count: {config[CONF_CELL_COUNT]}",
                path=["charge_voltage"],
            )
    return config


BQ25672_COMPONENT_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_BQ25672_ID): cv.use_id(BQ25672Component)}
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25672Component),
            cv.Optional(CONF_CELL_COUNT): cv.int_range(min=1, max=4),
            cv.Optional(CONF_ADC_RESOLUTION, default=15): cv.one_of(
                *ADC_RESOLUTIONS, int=True
            ),
            cv.Optional(CONF_ADC_AVERAGING, default=False): cv.boolean,
            cv.Optional(CONF_BATTERY_CURRENT_SENSING, default=True): cv.boolean,
            **{
                cv.Optional(key): validate_number_value(key)
                for key in NUMBER_SETTINGS
            },
            **{cv.Optional(key): cv.boolean for key in SWITCH_KEYS},
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x6B)),
    validate_hub_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_CELL_COUNT in config:
        cg.add(var.set_cell_count(config[CONF_CELL_COUNT]))
    cg.add(var.set_adc_resolution(ADC_RESOLUTIONS[config[CONF_ADC_RESOLUTION]]))
    cg.add(var.set_adc_averaging(config[CONF_ADC_AVERAGING]))
    cg.add(var.set_battery_current_sensing(config[CONF_BATTERY_CURRENT_SENSING]))

    for key in NUMBER_SETTINGS:
        if key in config:
            cg.add(
                var.set_initial_number(
                    getattr(NumberType, f"NUMBER_{key.upper()}"), config[key]
                )
            )
    for key in SWITCH_KEYS:
        if key in config:
            cg.add(
                var.set_initial_switch(
                    getattr(SwitchType, f"SWITCH_{key.upper()}"), config[key]
                )
            )
