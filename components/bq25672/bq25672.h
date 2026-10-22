#pragma once

// ESPHome integration for Marc Visser's BQ25672 library. MIT License.
#include "bq25672_driver.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace esphome {
namespace bq25672 {

enum class SensorType : uint8_t {
  SENSOR_BATTERY_VOLTAGE,
  SENSOR_SYSTEM_VOLTAGE,
  SENSOR_BUS_VOLTAGE,
  SENSOR_INPUT_1_VOLTAGE,
  SENSOR_INPUT_2_VOLTAGE,
  SENSOR_BATTERY_CURRENT,
  SENSOR_INPUT_CURRENT,
  SENSOR_DIE_TEMPERATURE,
  SENSOR_NTC_PERCENTAGE,
  SENSOR_DP_VOLTAGE,
  SENSOR_DN_VOLTAGE,
  SENSOR_INPUT_CURRENT_LIMIT,
  COUNT,
};

enum class BinarySensorType : uint8_t {
  BINARY_SENSOR_ONLINE,
  BINARY_SENSOR_CHARGING,
  BINARY_SENSOR_CHARGE_DONE,
  BINARY_SENSOR_POWER_GOOD,
  BINARY_SENSOR_BUS_PRESENT,
  BINARY_SENSOR_INPUT_1_PRESENT,
  BINARY_SENSOR_INPUT_2_PRESENT,
  BINARY_SENSOR_BATTERY_PRESENT,
  BINARY_SENSOR_INPUT_VOLTAGE_REGULATION,
  BINARY_SENSOR_INPUT_CURRENT_REGULATION,
  BINARY_SENSOR_THERMAL_REGULATION,
  BINARY_SENSOR_MIN_SYSTEM_VOLTAGE_REGULATION,
  BINARY_SENSOR_BATTERY_HOT,
  BINARY_SENSOR_BATTERY_WARM,
  BINARY_SENSOR_BATTERY_COOL,
  BINARY_SENSOR_BATTERY_COLD,
  BINARY_SENSOR_BATTERY_TOO_LOW_FOR_OTG,
  BINARY_SENSOR_WATCHDOG_EXPIRED,
  BINARY_SENSOR_FAULT,
  COUNT,
};

enum class TextSensorType : uint8_t {
  TEXT_SENSOR_CHARGE_STATUS,
  TEXT_SENSOR_VBUS_STATUS,
  TEXT_SENSOR_ICO_STATUS,
  TEXT_SENSOR_FAULT_STATUS,
  COUNT,
};

enum class SwitchType : uint8_t {
  SWITCH_CHARGING_ENABLED,
  SWITCH_TERMINATION_ENABLED,
  SWITCH_HIZ_MODE,
  SWITCH_ICO_ENABLED,
  SWITCH_MPPT_ENABLED,
  SWITCH_OTG_ENABLED,
  // ON forces both external input paths off; OFF releases that override.
  SWITCH_INPUTS_DISCONNECTED,
  COUNT,
};

enum class NumberType : uint8_t {
  NUMBER_CHARGE_VOLTAGE,
  NUMBER_CHARGE_CURRENT,
  NUMBER_INPUT_CURRENT_LIMIT,
  NUMBER_INPUT_VOLTAGE_LIMIT,
  NUMBER_MIN_SYSTEM_VOLTAGE,
  NUMBER_PRECHARGE_CURRENT,
  NUMBER_TERMINATION_CURRENT,
  NUMBER_OTG_VOLTAGE,
  NUMBER_OTG_CURRENT_LIMIT,
  NUMBER_RECHARGE_VOLTAGE_THRESHOLD,
  COUNT,
};

class BQ25672Number;
class BQ25672Switch;

/** Polling hub. Inherits the complete native register API for ESPHome lambdas.
 * YAML/entity voltages and currents use V/A; inherited methods use mV/mA.
 */
class BQ25672Component : public PollingComponent, public BQ25672 {
 public:
  BQ25672Component();
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sensor(SensorType type, sensor::Sensor *value) { this->sensors_[size_t(type)] = value; }
  void set_binary_sensor(BinarySensorType type, binary_sensor::BinarySensor *value) {
    this->binary_sensors_[size_t(type)] = value;
  }
  void set_text_sensor(TextSensorType type, text_sensor::TextSensor *value) {
    this->text_sensors_[size_t(type)] = value;
  }
  void set_number(NumberType type, BQ25672Number *value) { this->numbers_[size_t(type)] = value; }
  void set_switch(SwitchType type, BQ25672Switch *value) { this->switches_[size_t(type)] = value; }

  void set_initial_number(NumberType type, float value) { this->initial_numbers_[size_t(type)] = value; }
  void set_initial_switch(SwitchType type, bool value) { this->initial_switches_[size_t(type)] = value ? 1 : 0; }
  void set_cell_count(uint8_t value) { this->cell_count_ = value; }
  void set_adc_resolution(uint8_t value) { this->adc_resolution_ = value; }
  void set_adc_averaging(bool value) { this->adc_averaging_ = value; }
  void set_battery_current_sensing(bool value) { this->battery_current_sensing_ = value; }

  // Checked high-level methods use the same SI units as YAML.
  bool set_number_value(NumberType type, float value);
  bool set_switch_value(SwitchType type, bool value);
  bool is_initialized() const { return this->initialized_; }

 protected:
  static constexpr size_t SENSOR_COUNT = size_t(SensorType::COUNT);
  static constexpr size_t BINARY_SENSOR_COUNT = size_t(BinarySensorType::COUNT);
  static constexpr size_t TEXT_SENSOR_COUNT = size_t(TextSensorType::COUNT);
  static constexpr size_t SWITCH_COUNT = size_t(SwitchType::COUNT);
  static constexpr size_t NUMBER_COUNT = size_t(NumberType::COUNT);

  bool initialize_();
  void schedule_first_read_();
  void communication_failure_(const char *message);
  bool write_number_(NumberType type, float value);
  bool write_switch_(SwitchType type, bool value);
  float read_sensor_(SensorType type);
  float read_number_(NumberType type);
  bool read_switch_(SwitchType type);
  bool validate_charge_voltage_(int millivolts);
  void publish_status_(const std::array<uint8_t, 7> &status);
  void publish_online_(bool value);

  std::array<sensor::Sensor *, SENSOR_COUNT> sensors_{};
  std::array<binary_sensor::BinarySensor *, BINARY_SENSOR_COUNT> binary_sensors_{};
  std::array<text_sensor::TextSensor *, TEXT_SENSOR_COUNT> text_sensors_{};
  std::array<BQ25672Switch *, SWITCH_COUNT> switches_{};
  std::array<BQ25672Number *, NUMBER_COUNT> numbers_{};
  std::array<float, NUMBER_COUNT> initial_numbers_{};
  std::array<int8_t, SWITCH_COUNT> initial_switches_{};
  uint8_t cell_count_{0};  // Zero means retain the existing CELL configuration.
  uint8_t device_revision_{0};
  uint8_t adc_resolution_{0};  // Raw 0 = 15 effective bits.
  bool adc_averaging_{false};
  bool battery_current_sensing_{true};
  bool initialized_{false};
  bool sample_ready_{false};
  bool charging_restore_saved_{false};
  bool charging_restore_state_{false};
};

class BQ25672Switch : public switch_::Switch {
 public:
  void set_parent(BQ25672Component *parent) { this->parent_ = parent; }
  void set_type(SwitchType type) { this->type_ = type; }

 protected:
  void write_state(bool state) override;
  BQ25672Component *parent_{nullptr};
  SwitchType type_{SwitchType::SWITCH_CHARGING_ENABLED};
};

class BQ25672Number : public number::Number {
 public:
  void set_parent(BQ25672Component *parent) { this->parent_ = parent; }
  void set_type(NumberType type) { this->type_ = type; }

 protected:
  void control(float value) override;
  BQ25672Component *parent_{nullptr};
  NumberType type_{NumberType::NUMBER_CHARGE_CURRENT};
};

}  // namespace bq25672
}  // namespace esphome
