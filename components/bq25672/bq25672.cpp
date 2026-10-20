// ESPHome integration for Marc Visser's BQ25672 library. MIT License.
#include "bq25672.h"
#include "esphome/core/log.h"

#include <string>

namespace esphome {
namespace bq25672 {

static const char *const TAG = "bq25672";

namespace {
struct NumberSetting {
  const char *name;
  int minimum;
  int maximum;
  int step;
  int offset;
  int (BQ25672::*get)();
  bool (BQ25672::*set)(int);
};

// Physical integer units are mV/mA. Order matches NumberType in bq25672.h.
const NumberSetting NUMBER_SETTINGS[] = {
    {"Charge voltage", 3000, 18800, 10, 0, &BQ25672::getChargeVoltage, &BQ25672::setChargeVoltage},
    {"Charge current", 50, 3000, 10, 0, &BQ25672::getChargeCurrent, &BQ25672::setChargeCurrent},
    {"Input current limit", 100, 3300, 10, 0, &BQ25672::getInputCurrentLimitRegister,
     &BQ25672::setInputCurrentLimitRegister},
    {"Input voltage limit", 3600, 22000, 100, 0, &BQ25672::getVindpmThreshold, &BQ25672::setVindpmThreshold},
    {"Minimum system voltage", 2500, 16000, 250, 2500, &BQ25672::getMinSystemVoltage,
     &BQ25672::setMinSystemVoltage},
    {"Precharge current", 40, 2000, 40, 0, &BQ25672::getPreChargeCurrent, &BQ25672::setPreChargeCurrent},
    {"Termination current", 40, 1000, 40, 0, &BQ25672::getTerminationCurrent, &BQ25672::setTerminationCurrent},
    {"OTG voltage", 5000, 12000, 10, 2800, &BQ25672::getOtgVoltage, &BQ25672::setOtgVoltage},
    {"OTG current limit", 160, 2000, 40, 0, &BQ25672::getOtgCurrentLimit, &BQ25672::setOtgCurrentLimit},
    {"Recharge voltage threshold", 50, 800, 50, 50, &BQ25672::getBatteryRechargeThreshold,
     &BQ25672::setBatteryRechargeThreshold},
};

struct SwitchSetting {
  const char *name;
  bool (BQ25672::*get)();
  bool (BQ25672::*set)(bool);
};
const SwitchSetting SWITCH_SETTINGS[] = {
    {"Charging", &BQ25672::getChargingEnabled, &BQ25672::setChargingEnabled},
    {"Termination", &BQ25672::getTerminationEnabled, &BQ25672::setTerminationEnabled},
    {"Hi-Z", &BQ25672::getHizModeEnabled, &BQ25672::setHizModeEnabled},
    {"ICO", &BQ25672::getIcoEnabled, &BQ25672::setIcoEnabled},
    {"MPPT", &BQ25672::getMpptEnabled, &BQ25672::setMpptEnabled},
    {"OTG", &BQ25672::getOtgControlEnabled, &BQ25672::setOtgControlEnabled},
};

const char *const CHARGE_STATUS[] = {"Not charging", "Trickle charge", "Precharge", "Fast charge (CC)",
                                    "Taper charge (CV)", "Reserved", "Top-off charging", "Charge done"};
const char *const VBUS_STATUS[] = {
    "No input or OTG suspended", "USB SDP", "USB CDP", "USB DCP", "HVDCP", "Unknown adapter",
    "Non-standard adapter", "OTG", "Unqualified adapter", "Reserved", "Reserved", "Direct VBUS power",
    "Backup mode", "Reserved", "Reserved", "Reserved"};
const char *const ICO_STATUS[] = {"Disabled", "Optimizing", "Maximum input current detected", "Reserved"};

void append_fault_(std::string &result, bool active, const char *name) {
  if (!active) return;
  if (!result.empty()) result += "; ";
  result += name;
}
}  // namespace

BQ25672Component::BQ25672Component() {
  this->initial_numbers_.fill(NAN);
  this->initial_switches_.fill(-1);
}

void BQ25672Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BQ25672...");
  this->publish_online_(false);
  if (this->initialize_()) this->schedule_first_read_();
}

bool BQ25672Component::initialize_() {
  this->clear_io_error();
  this->initialized_ = false;
  this->sample_ready_ = false;
  uint16_t part = 0;
  if (!this->read_bytes(0x48, &part, 1)) {
    this->communication_failure_("BQ25672 not responding; will retry at the next update");
    return false;
  }
  // The part-number field identifies BQ25672; the revision is allowed to vary.
  if (((part >> 3) & 0x07) != 0x04) {
    ESP_LOGE(TAG, "Unexpected part number at 0x%02X: REG48=0x%02X", this->address_, unsigned(part));
    this->status_set_error();
    this->mark_failed();
    return false;
  }
  this->device_revision_ = part & 0x07;

  // Writes enter host mode. Stop the chip watchdog before configuring anything.
  if (!this->setWatchdogTimerTime(0) || this->getWatchdogTimerTime() != 0 || !this->io_ok()) {
    this->communication_failure_("Could not disable the charger watchdog");
    return false;
  }

  bool have_initial_numbers = false;
  for (float value : this->initial_numbers_) have_initial_numbers |= std::isfinite(value);
  const bool configure_charge = have_initial_numbers || this->cell_count_ != 0;
  const int8_t requested_charging = this->initial_switches_[size_t(SwitchType::SWITCH_CHARGING_ENABLED)];
  if (configure_charge || requested_charging == 0) {
    // Preserve this across a failed initialization attempt; a retry must not
    // mistake our temporary disable for the user's original charging state.
    if (!this->charging_restore_saved_) {
      this->charging_restore_state_ = this->getChargingEnabled();
      if (!this->io_ok()) {
        this->communication_failure_("Could not read the charging state");
        return false;
      }
      this->charging_restore_saved_ = true;
    }
    if (!this->write_switch_(SwitchType::SWITCH_CHARGING_ENABLED, false)) {
      this->communication_failure_("Could not pause charging before configuration");
      return false;
    }
  }

  if (this->cell_count_ != 0) {
    const int old_count = this->getBatterySeriesCount();
    if (!this->io_ok()) {
      this->communication_failure_("Could not read the cell count");
      return false;
    }
    // Changing CELL resets VREG, ICHG and VSYSMIN. Apply it before all limits,
    // and avoid a gratuitous CELL write when the existing value already matches.
    if (old_count != this->cell_count_ &&
        (!this->setBatterySeriesCount(this->cell_count_) || this->getBatterySeriesCount() != this->cell_count_ ||
         !this->io_ok())) {
      this->communication_failure_("Cell count configuration failed; charging remains paused");
      return false;
    }
  }

  for (size_t i = 0; i < NUMBER_COUNT; i++) {
    if (std::isfinite(this->initial_numbers_[i]) &&
        !this->write_number_(static_cast<NumberType>(i), this->initial_numbers_[i])) {
      this->communication_failure_("A startup setting was rejected; charging remains paused");
      return false;
    }
  }

  // Own the ADC configuration and enable all eleven channels. Preserve reserved
  // bits in the channel-disable registers. The first conversion is asynchronous.
  if (!this->setAdcEnabled(false) || !this->setAdcConversion(0) ||
      !this->setAdcResolution(this->adc_resolution_) || !this->setRunningAverageEnabled(this->adc_averaging_) ||
      !this->write_var(0x2F, 1, 1, 7, 0) || !this->write_var(0x30, 1, 4, 7, 0) ||
      !this->setBatteryCurrentSensingEnabled(this->battery_current_sensing_) || !this->setAdcEnabled(true)) {
    this->communication_failure_("Could not configure ADC measurements");
    return false;
  }
  const uint16_t expected_adc = 0x80 | (this->adc_resolution_ << 4) | (this->adc_averaging_ ? 0x08 : 0);
  uint16_t adc_control = 0;
  if (!this->read_bytes(0x2E, &adc_control, 1) || (adc_control & 0xF8) != expected_adc ||
      this->getBatteryCurrentSensingEnabled() != this->battery_current_sensing_ || !this->io_ok()) {
    this->communication_failure_("ADC configuration did not read back correctly");
    return false;
  }

  // Charging is the final switch applied, after all other setup has succeeded.
  for (size_t i = 1; i < SWITCH_COUNT; i++) {
    if (this->initial_switches_[i] >= 0 &&
        !this->write_switch_(static_cast<SwitchType>(i), this->initial_switches_[i] != 0)) {
      this->communication_failure_("A startup switch setting was rejected");
      return false;
    }
  }
  if (requested_charging >= 0 || this->charging_restore_saved_) {
    const bool enable = requested_charging >= 0 ? requested_charging != 0 : this->charging_restore_state_;
    if (!this->write_switch_(SwitchType::SWITCH_CHARGING_ENABLED, enable)) {
      this->communication_failure_("Could not apply the final charging state");
      return false;
    }
  }
  this->charging_restore_saved_ = false;
  this->initialized_ = true;
  this->status_clear_warning();
  ESP_LOGI(TAG, "BQ25672 initialized (revision %u)", unsigned(this->device_revision_));
  return true;
}

void BQ25672Component::schedule_first_read_() {
  // 15-bit conversion is nominally 24 ms per enabled channel. Do not delay()
  // and do not wait for ADC_DONE_STAT, which stays zero in continuous mode.
  this->set_timeout("first_read", 350, [this]() {
    this->sample_ready_ = true;
    this->update();
  });
}

void BQ25672Component::update() {
  if (this->is_failed()) return;
  if (!this->initialized_) {
    if (this->initialize_()) this->schedule_first_read_();
    return;
  }
  if (!this->sample_ready_) return;

  this->clear_io_error();
  uint16_t watchdog = 0;
  uint16_t adc_control = 0;
  if (!this->read_bytes(0x10, &watchdog, 1) || !this->read_bytes(0x2E, &adc_control, 1)) {
    this->communication_failure_("I2C communication failed");
    return;
  }
  const uint16_t expected_adc = 0x80 | (this->adc_resolution_ << 4) | (this->adc_averaging_ ? 0x08 : 0);
  if ((watchdog & 0x07) != 0 || (adc_control & 0xF8) != expected_adc) {
    ESP_LOGW(TAG, "Managed registers changed or charger reset; restoring startup configuration");
    this->publish_online_(false);
    if (this->initialize_()) this->schedule_first_read_();
    return;
  }

  std::array<uint8_t, 7> status{};
  std::array<float, SENSOR_COUNT> samples{};
  std::array<float, NUMBER_COUNT> number_states{};
  std::array<bool, SWITCH_COUNT> switch_states{};
  for (size_t i = 0; i < status.size(); i++) {
    uint16_t value = 0;
    if (!this->read_bytes(0x1B + i, &value, 1)) {
      this->communication_failure_("Could not read charger status");
      return;
    }
    status[i] = value;
  }
  for (size_t i = 0; i < SENSOR_COUNT; i++) {
    if (this->sensors_[i] != nullptr) samples[i] = this->read_sensor_(static_cast<SensorType>(i));
  }
  for (size_t i = 0; i < NUMBER_COUNT; i++) {
    if (this->numbers_[i] != nullptr) number_states[i] = this->read_number_(static_cast<NumberType>(i));
  }
  for (size_t i = 0; i < SWITCH_COUNT; i++) {
    if (this->switches_[i] != nullptr) switch_states[i] = this->read_switch_(static_cast<SwitchType>(i));
  }
  if (!this->io_ok()) {
    this->communication_failure_("A charger measurement or setting read failed");
    return;
  }

  // Publish only after all reads succeeded. Failed reads never become false
  // statuses, zero currents, or plausible offset values in Home Assistant.
  this->status_clear_warning();
  this->publish_online_(true);
  this->publish_status_(status);
  for (size_t i = 0; i < SENSOR_COUNT; i++) {
    if (this->sensors_[i] != nullptr) this->sensors_[i]->publish_state(samples[i]);
  }
  for (size_t i = 0; i < NUMBER_COUNT; i++) {
    if (this->numbers_[i] != nullptr) this->numbers_[i]->publish_state(number_states[i]);
  }
  for (size_t i = 0; i < SWITCH_COUNT; i++) {
    if (this->switches_[i] != nullptr) this->switches_[i]->publish_state(switch_states[i]);
  }
}

float BQ25672Component::read_sensor_(SensorType type) {
  switch (type) {
    case SensorType::SENSOR_BATTERY_VOLTAGE: return this->getBatteryVoltage() / 1000.0f;
    case SensorType::SENSOR_SYSTEM_VOLTAGE: return this->getSystemVoltage() / 1000.0f;
    case SensorType::SENSOR_BUS_VOLTAGE: return this->getBusVoltage() / 1000.0f;
    case SensorType::SENSOR_INPUT_1_VOLTAGE: return this->getInput1Voltage() / 1000.0f;
    case SensorType::SENSOR_INPUT_2_VOLTAGE: return this->getInput2Voltage() / 1000.0f;
    case SensorType::SENSOR_BATTERY_CURRENT: return this->getBatteryCurrent() / 1000.0f;
    case SensorType::SENSOR_INPUT_CURRENT: return this->getInputCurrent() / 1000.0f;
    case SensorType::SENSOR_DIE_TEMPERATURE: return this->getDieTemperature();
    case SensorType::SENSOR_NTC_PERCENTAGE: return this->getNtcReading();
    case SensorType::SENSOR_DP_VOLTAGE: return this->getDpVoltage() / 1000.0f;
    case SensorType::SENSOR_DN_VOLTAGE: return this->getDnVoltage() / 1000.0f;
    case SensorType::SENSOR_INPUT_CURRENT_LIMIT: return this->getInputCurrentLimit() / 1000.0f;
    default: return NAN;
  }
}

float BQ25672Component::read_number_(NumberType type) {
  return (this->*NUMBER_SETTINGS[size_t(type)].get)() / 1000.0f;
}

bool BQ25672Component::read_switch_(SwitchType type) { return (this->*SWITCH_SETTINGS[size_t(type)].get)(); }

bool BQ25672Component::validate_charge_voltage_(int millivolts) {
  const int cells = this->getBatterySeriesCount();
  if (!this->io_ok() || cells < 1 || cells > 4) return false;
  const int minimum[] = {3000, 5000, 10000, 14000};
  const int maximum[] = {4990, 9990, 13990, 18800};
  if (millivolts < minimum[cells - 1] || millivolts > maximum[cells - 1]) {
    ESP_LOGW(TAG, "Charge voltage %d mV is outside the device's %d-cell register band (%d..%d mV)",
             millivolts, cells, minimum[cells - 1], maximum[cells - 1]);
    return false;
  }
  return true;
}

bool BQ25672Component::write_number_(NumberType type, float value) {
  const size_t index = size_t(type);
  if (index >= NUMBER_COUNT || !std::isfinite(value)) return false;
  const auto &setting = NUMBER_SETTINGS[index];
  // Check the float range before integer conversion, including hostile runtime
  // values from lambdas. Exact hardware step alignment prevents silent rounding.
  const double scaled = static_cast<double>(value) * 1000.0;
  if (scaled < setting.minimum - 0.001 || scaled > setting.maximum + 0.001) {
    ESP_LOGW(TAG, "%s is outside its valid range", setting.name);
    return false;
  }
  const int target = static_cast<int>(std::lround(scaled));
  if (std::fabs(scaled - target) > 0.001 || (target - setting.offset) % setting.step != 0) {
    ESP_LOGW(TAG, "%s must use %d mV/mA steps", setting.name, setting.step);
    return false;
  }
  if (type == NumberType::NUMBER_CHARGE_VOLTAGE && !this->validate_charge_voltage_(target)) return false;
  if (!(this->*setting.set)(target)) return false;
  const int actual = (this->*setting.get)();
  if (!this->io_ok()) return false;
  if (actual != target) {
    ESP_LOGW(TAG, "%s rejected or overridden: requested %d, read back %d mV/mA", setting.name, target, actual);
    return false;
  }
  return true;
}

bool BQ25672Component::write_switch_(SwitchType type, bool value) {
  if (size_t(type) >= SWITCH_COUNT) return false;
  const auto &setting = SWITCH_SETTINGS[size_t(type)];
  if (!(this->*setting.set)(value)) return false;
  const bool actual = (this->*setting.get)();
  if (!this->io_ok()) return false;
  if (actual != value) {
    ESP_LOGW(TAG, "%s setting did not read back correctly", setting.name);
    return false;
  }
  return true;
}

bool BQ25672Component::set_number_value(NumberType type, float value) {
  if (!this->initialized_ || this->is_failed() || size_t(type) >= NUMBER_COUNT) return false;
  this->clear_io_error();
  const bool applied = this->write_number_(type, value);
  const bool write_io_ok = this->io_ok();
  if (write_io_ok && this->numbers_[size_t(type)] != nullptr) {
    const float actual = this->read_number_(type);
    if (this->io_ok()) this->numbers_[size_t(type)]->publish_state(actual);
  }
  if (!this->io_ok()) this->communication_failure_("I2C setting write/readback failed");
  else if (!applied) this->status_set_warning("Number setting rejected");
  return applied && this->io_ok();
}

bool BQ25672Component::set_switch_value(SwitchType type, bool value) {
  if (!this->initialized_ || this->is_failed() || size_t(type) >= SWITCH_COUNT) return false;
  this->clear_io_error();
  const bool applied = this->write_switch_(type, value);
  if (this->io_ok() && this->switches_[size_t(type)] != nullptr) {
    const bool actual = this->read_switch_(type);
    if (this->io_ok()) this->switches_[size_t(type)]->publish_state(actual);
  }
  if (!this->io_ok()) this->communication_failure_("I2C switch write/readback failed");
  else if (!applied) this->status_set_warning("Switch setting rejected");
  return applied && this->io_ok();
}

void BQ25672Component::publish_online_(bool value) {
  auto *entity = this->binary_sensors_[size_t(BinarySensorType::BINARY_SENSOR_ONLINE)];
  if (entity != nullptr) entity->publish_state(value);
}

void BQ25672Component::communication_failure_(const char *message) {
  ESP_LOGW(TAG, "%s", message);
  this->status_set_warning(message);
  this->publish_online_(false);
  for (auto *entity : this->sensors_) {
    if (entity != nullptr) entity->publish_state(NAN);
  }
  for (size_t i = 1; i < BINARY_SENSOR_COUNT; i++) {
    if (this->binary_sensors_[i] != nullptr) this->binary_sensors_[i]->invalidate_state();
  }
  for (auto *entity : this->text_sensors_) {
    if (entity != nullptr) entity->publish_state("Unavailable");
  }
  for (auto *entity : this->numbers_) {
    if (entity != nullptr) entity->publish_state(NAN);
  }
  // Switches retain their last confirmed state; never claim an off state after
  // a failed read. The online entity identifies communication loss.
}

void BQ25672Component::publish_status_(const std::array<uint8_t, 7> &status) {
  const uint8_t charge = (status[1] >> 5) & 0x07;
  const uint8_t vbus = (status[1] >> 1) & 0x0F;
  const uint8_t ico = (status[2] >> 6) & 0x03;
  const bool fault = (status[5] & 0x7F) != 0 || (status[6] & 0xF4) != 0;
  auto binary = [this](BinarySensorType type, bool value) {
    auto *entity = this->binary_sensors_[size_t(type)];
    if (entity != nullptr) entity->publish_state(value);
  };
  binary(BinarySensorType::BINARY_SENSOR_CHARGING, (charge >= 1 && charge <= 4) || charge == 6);
  binary(BinarySensorType::BINARY_SENSOR_CHARGE_DONE, charge == 7);
  binary(BinarySensorType::BINARY_SENSOR_POWER_GOOD, status[0] & 0x08);
  binary(BinarySensorType::BINARY_SENSOR_BUS_PRESENT, status[0] & 0x01);
  binary(BinarySensorType::BINARY_SENSOR_INPUT_1_PRESENT, status[0] & 0x02);
  binary(BinarySensorType::BINARY_SENSOR_INPUT_2_PRESENT, status[0] & 0x04);
  binary(BinarySensorType::BINARY_SENSOR_BATTERY_PRESENT, status[2] & 0x01);
  binary(BinarySensorType::BINARY_SENSOR_INPUT_VOLTAGE_REGULATION, status[0] & 0x40);
  binary(BinarySensorType::BINARY_SENSOR_INPUT_CURRENT_REGULATION, status[0] & 0x80);
  binary(BinarySensorType::BINARY_SENSOR_THERMAL_REGULATION, status[2] & 0x04);
  binary(BinarySensorType::BINARY_SENSOR_MIN_SYSTEM_VOLTAGE_REGULATION, status[3] & 0x10);
  binary(BinarySensorType::BINARY_SENSOR_BATTERY_HOT, status[4] & 0x01);
  binary(BinarySensorType::BINARY_SENSOR_BATTERY_WARM, status[4] & 0x02);
  binary(BinarySensorType::BINARY_SENSOR_BATTERY_COOL, status[4] & 0x04);
  binary(BinarySensorType::BINARY_SENSOR_BATTERY_COLD, status[4] & 0x08);
  binary(BinarySensorType::BINARY_SENSOR_BATTERY_TOO_LOW_FOR_OTG, status[4] & 0x10);
  binary(BinarySensorType::BINARY_SENSOR_WATCHDOG_EXPIRED, status[0] & 0x20);
  binary(BinarySensorType::BINARY_SENSOR_FAULT, fault);

  auto text = [this](TextSensorType type, const char *value) {
    auto *entity = this->text_sensors_[size_t(type)];
    if (entity != nullptr) entity->publish_state(value);
  };
  text(TextSensorType::TEXT_SENSOR_CHARGE_STATUS, CHARGE_STATUS[charge]);
  text(TextSensorType::TEXT_SENSOR_VBUS_STATUS, VBUS_STATUS[vbus]);
  text(TextSensorType::TEXT_SENSOR_ICO_STATUS, ICO_STATUS[ico]);
  if (this->text_sensors_[size_t(TextSensorType::TEXT_SENSOR_FAULT_STATUS)] != nullptr) {
    std::string faults;
    append_fault_(faults, status[5] & 0x01, "Input 1 overvoltage");
    append_fault_(faults, status[5] & 0x02, "Input 2 overvoltage");
    append_fault_(faults, status[5] & 0x04, "Converter overcurrent");
    append_fault_(faults, status[5] & 0x08, "Battery overcurrent");
    append_fault_(faults, status[5] & 0x10, "Bus overcurrent");
    append_fault_(faults, status[5] & 0x20, "Battery overvoltage");
    append_fault_(faults, status[5] & 0x40, "Bus overvoltage");
    append_fault_(faults, status[6] & 0x04, "Thermal shutdown");
    append_fault_(faults, status[6] & 0x10, "OTG undervoltage");
    append_fault_(faults, status[6] & 0x20, "OTG overvoltage");
    append_fault_(faults, status[6] & 0x40, "System overvoltage");
    append_fault_(faults, status[6] & 0x80, "System short circuit");
    text(TextSensorType::TEXT_SENSOR_FAULT_STATUS, faults.empty() ? "None" : faults.c_str());
  }
}

void BQ25672Switch::write_state(bool state) {
  if (this->parent_ != nullptr) this->parent_->set_switch_value(this->type_, state);
}

void BQ25672Number::control(float value) {
  if (!std::isfinite(value) || value < this->traits.get_min_value() || value > this->traits.get_max_value()) {
    ESP_LOGW(TAG, "Number value is outside the configured entity limits");
    return;
  }
  if (this->parent_ != nullptr) this->parent_->set_number_value(this->type_, value);
}

void BQ25672Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ25672:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  ADC: continuous, %u bits, averaging %s", unsigned(15 - this->adc_resolution_),
                YESNO(this->adc_averaging_));
  ESP_LOGCONFIG(TAG, "  Battery discharge current sensing: %s", YESNO(this->battery_current_sensing_));
  ESP_LOGCONFIG(TAG, "  Charger watchdog: disabled");
  if (this->cell_count_ != 0) ESP_LOGCONFIG(TAG, "  Configured cell count: %u", unsigned(this->cell_count_));
  for (auto *entity : this->sensors_) LOG_SENSOR("  ", "", entity);
  for (auto *entity : this->binary_sensors_) LOG_BINARY_SENSOR("  ", "", entity);
  for (auto *entity : this->text_sensors_) LOG_TEXT_SENSOR("  ", "", entity);
  for (auto *entity : this->switches_) LOG_SWITCH("  ", "", entity);
  for (auto *entity : this->numbers_) LOG_NUMBER("  ", "", entity);
}

}  // namespace bq25672
}  // namespace esphome
