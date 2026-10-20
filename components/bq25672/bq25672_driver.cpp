// Native ESPHome transport port; original register helpers by Marc Visser.
// Copyright (c) 2023-2024 Marc Visser. MIT License; see LICENSE.

#include "bq25672_driver.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bq25672 {

static const char *const TAG = "bq25672.driver";

namespace {
int32_t decode_signed_word_(uint16_t value) {
  return (value & 0x8000U) != 0 ? static_cast<int32_t>(value) - 65536 : static_cast<int32_t>(value);
}

bool valid_field_(uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end) {
  return (byte_cnt == 1 || byte_cnt == 2) && bit_start <= bit_end && bit_end < byte_cnt * 8;
}
}  // namespace

bool BQ25672::validate_range_(int value, int minimum, int maximum) {
  if (value < minimum || value > maximum) {
    ESP_LOGW(TAG, "Value %d outside valid range %d..%d", value, minimum, maximum);
    return this->record_result_(false);
  }
  return true;
}

bool BQ25672::read_bytes(uint8_t reg, uint16_t *data, uint8_t byte_cnt) {
  if (data == nullptr || (byte_cnt != 1 && byte_cnt != 2) || this->bus_ == nullptr)
    return this->record_result_(false);
  uint8_t bytes[2] = {0, 0};
  if (this->read_register(reg, bytes, byte_cnt) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "I2C read failed at register 0x%02X", reg);
    return this->record_result_(false);
  }
  // TI stores the most significant byte at the lower register address.
  *data = byte_cnt == 1 ? bytes[0] : static_cast<uint16_t>((uint16_t(bytes[0]) << 8) | bytes[1]);
  return this->record_result_(true);
}

bool BQ25672::write_bytes(uint8_t reg, uint16_t data, uint8_t byte_cnt) {
  if ((byte_cnt != 1 && byte_cnt != 2) || this->bus_ == nullptr || (byte_cnt == 1 && data > 0xFFU))
    return this->record_result_(false);
  uint8_t bytes[2] = {static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data)};
  const uint8_t *payload = byte_cnt == 1 ? &bytes[1] : bytes;
  const bool success = this->write_register(reg, payload, byte_cnt) == i2c::ERROR_OK;
  if (!success) {
    ESP_LOGD(TAG, "I2C write failed at register 0x%02X", reg);
  }
  return this->record_result_(success);
}

uint16_t BQ25672::read_var(uint8_t reg, uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end) {
  if (!valid_field_(byte_cnt, bit_start, bit_end)) {
    this->record_result_(false);
    return 0;
  }
  uint16_t data = 0;
  if (!this->read_bytes(reg, &data, byte_cnt)) return 0;
  // uint32_t avoids shifting a 16-bit value by its full width.
  const uint32_t mask = (uint32_t(1) << (bit_end - bit_start + 1)) - 1U;
  return static_cast<uint16_t>((data >> bit_start) & mask);
}

bool BQ25672::write_var(uint8_t reg, uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end,
                        uint16_t new_data) {
  if (!valid_field_(byte_cnt, bit_start, bit_end)) return this->record_result_(false);
  const uint32_t unshifted_mask = (uint32_t(1) << (bit_end - bit_start + 1)) - 1U;
  if (new_data > unshifted_mask) return this->record_result_(false);
  uint16_t old_data = 0;
  // Do not attempt any write if the read part of the transaction failed.
  if (!this->read_bytes(reg, &old_data, byte_cnt)) return false;
  const uint16_t mask = static_cast<uint16_t>(unshifted_mask << bit_start);
  const uint16_t result = static_cast<uint16_t>((old_data & ~mask) | (new_data << bit_start));
  return this->write_bytes(reg, result, byte_cnt);
}

int BQ25672::getMinSystemVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x0;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 5;
  uint16_t offset = 2500;
  uint8_t lsb = 250;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setMinSystemVoltage(int new_value) {
  if (!this->validate_range_(new_value, 2500, 16000)) return false;

  // Set value in: mV

  uint8_t reg = 0x0;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 5;
  uint16_t offset = 2500;
  uint8_t lsb = 250;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getChargeVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x1;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 10;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setChargeVoltage(int new_value) {
  if (!this->validate_range_(new_value, 3000, 18800)) return false;

  // Set value in: mV

  uint8_t reg = 0x1;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 10;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getChargeCurrent() {
  // Returns value in: mA

  uint8_t reg = 0x3;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 8;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setChargeCurrent(int new_value) {
  if (!this->validate_range_(new_value, 50, 3000)) return false;

  // Set value in: mA

  uint8_t reg = 0x3;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 8;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getVindpmThreshold() {
  // Returns value in: mV

  uint8_t reg = 0x5;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 7;
  uint16_t offset = 0;
  uint8_t lsb = 100;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setVindpmThreshold(int new_value) {
  if (!this->validate_range_(new_value, 3600, 22000)) return false;

  // Set value in: mV

  uint8_t reg = 0x5;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 7;
  uint16_t offset = 0;
  uint8_t lsb = 100;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getInputCurrentLimitRegister() {
  // Returns value in: mA

  uint8_t reg = 0x6;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 8;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setInputCurrentLimitRegister(int new_value) {
  if (!this->validate_range_(new_value, 100, 3300)) return false;

  // Set value in: mA

  uint8_t reg = 0x6;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 8;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getPreChargeCurrent() {
  // Returns value in: mA

  uint8_t reg = 0x8;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 5;
  uint16_t offset = 0;
  uint8_t lsb = 40;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setPreChargeCurrent(int new_value) {
  if (!this->validate_range_(new_value, 40, 2000)) return false;

  // Set value in: mA

  uint8_t reg = 0x8;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 5;
  uint16_t offset = 0;
  uint8_t lsb = 40;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getPrechrgFastchrgThreshold() {
  // Return value:
  // 0 = 15%*VREG
  // 1 = 62.2%*VREG
  // 2 = 66.7%*VREG
  // 3 = 71.4%*VREG

  uint8_t reg = 0x8;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setPrechrgFastchrgThreshold(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 15%*VREG
  // 1 = 62.2%*VREG
  // 2 = 66.7%*VREG
  // 3 = 71.4%*VREG

  uint8_t reg = 0x8;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getTerminationCurrent() {
  // Returns value in: mA

  uint8_t reg = 0x9;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 4;
  uint16_t offset = 0;
  uint8_t lsb = 40;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setTerminationCurrent(int new_value) {
  if (!this->validate_range_(new_value, 40, 1000)) return false;

  // Set value in: mA

  uint8_t reg = 0x9;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 4;
  uint16_t offset = 0;
  uint8_t lsb = 40;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getWatchdogTimerDisablesCharging() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x9;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setWatchdogTimerDisablesCharging(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x9;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getBatteryRechargeThreshold() {
  // Returns value in: mV

  uint8_t reg = 0xa;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 3;
  uint16_t offset = 50;
  uint8_t lsb = 50;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setBatteryRechargeThreshold(int new_value) {
  if (!this->validate_range_(new_value, 50, 800)) return false;

  // Set value in: mV

  uint8_t reg = 0xa;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 3;
  uint16_t offset = 50;
  uint8_t lsb = 50;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getBatteryRechargeDeglitchTime() {
  // Return value:
  // 0 = 64ms
  // 1 = 256ms
  // 2 = 1024ms
  // 3 = 2048ms

  uint8_t reg = 0xa;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryRechargeDeglitchTime(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 64ms
  // 1 = 256ms
  // 2 = 1024ms
  // 3 = 2048ms

  uint8_t reg = 0xa;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getBatterySeriesCount() {
  // Returns value in: 

  uint8_t reg = 0xa;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;
  uint16_t offset = 1;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setBatterySeriesCount(int new_value) {
  if (!this->validate_range_(new_value, 1, 4)) return false;

  // Set value in: 

  uint8_t reg = 0xa;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;
  uint16_t offset = 1;
  uint8_t lsb = 1;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getOtgVoltage() {
  // Returns value in: mv

  uint8_t reg = 0xb;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 10;
  uint16_t offset = 2800;
  uint8_t lsb = 10;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setOtgVoltage(int new_value) {
  if (!this->validate_range_(new_value, 2800, 22000)) return false;

  // Set value in: mv

  uint8_t reg = 0xb;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 10;
  uint16_t offset = 2800;
  uint8_t lsb = 10;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getOtgCurrentLimit() {
  // Returns value in: mA

  uint8_t reg = 0xd;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 6;
  uint16_t offset = 0;
  uint8_t lsb = 40;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::setOtgCurrentLimit(int new_value) {
  if (!this->validate_range_(new_value, 160, 3360)) return false;

  // Set value in: mA

  uint8_t reg = 0xd;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 6;
  uint16_t offset = 0;
  uint8_t lsb = 40;

  uint16_t new_data = (uint16_t)((new_value - offset) / lsb);

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getPreChargeTimer() {
  // Return value:
  // 0 = 2
  // 1 = 0.5

  uint8_t reg = 0xd;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setPreChargeTimer(int new_value) {
  if (!this->validate_range_(new_value, 0, 1)) return false;

  // Set value:
  // 0 = 2
  // 1 = 0.5

  uint8_t reg = 0xd;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getSlowPreAndTrickleChargeDuringThermalReg() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSlowPreAndTrickleChargeDuringThermalReg(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getFastChargeTimer() {
  // Return value:
  // 0 = 5h
  // 1 = 8h
  // 2 = 12h
  // 3 = 24h

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setFastChargeTimer(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 5h
  // 1 = 8h
  // 2 = 12h
  // 3 = 24h

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getFastChargeTimerEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setFastChargeTimerEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getPreChargeTimerEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setPreChargeTimerEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getTrickleChargeTimerEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setTrickleChargeTimerEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getTopOffTimer() {
  // Return value:
  // 0 = -1
  // 1 = 15
  // 2 = 30
  // 3 = 45

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setTopOffTimer(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = -1
  // 1 = 15
  // 2 = 30
  // 3 = 45

  uint8_t reg = 0xe;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getTerminationEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setTerminationEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getHizModeEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setHizModeEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getIcoEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setIcoEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getChargingEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setChargingEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatteryDischargeCurrentForced() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryDischargeCurrentForced(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatteryDischargeDuringOvpEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryDischargeDuringOvpEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0xf;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getWatchdogTimerTime() {
  // Return value:
  // 0 = -1
  // 1 = 0.5
  // 2 = 1
  // 3 = 2
  // 4 = 20
  // 5 = 40
  // 6 = 80
  // 7 = 160

  uint8_t reg = 0x10;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setWatchdogTimerTime(int new_value) {
  if (!this->validate_range_(new_value, 0, 7)) return false;

  // Set value:
  // 0 = -1
  // 1 = 0.5
  // 2 = 1
  // 3 = 2
  // 4 = 20
  // 5 = 40
  // 6 = 80
  // 7 = 160

  uint8_t reg = 0x10;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getInputOverVoltageThreshold() {
  // Return value:
  // 0 = 26
  // 1 = 22
  // 2 = 12
  // 3 = 7

  uint8_t reg = 0x10;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInputOverVoltageThreshold(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 26
  // 1 = 22
  // 2 = 12
  // 3 = 7

  uint8_t reg = 0x10;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getSfet10sDelayRemoved() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSfet10sDelayRemoved(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getSfetControl() {
  // Return value:
  // 0 = IDLE
  // 1 = Shutdown mode
  // 2 = Ship mode
  // 3 = System power reset

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSfetControl(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = IDLE
  // 1 = Shutdown mode
  // 2 = Ship mode
  // 3 = System power reset

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getHighVoltageDcpHandshakeEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setHighVoltageDcpHandshakeEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getHvdc9vEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setHvdc9vEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getHvdc12vEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setHvdc12vEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getAutoDpdnDetectionEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setAutoDpdnDetectionEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x11;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getOoaInForwardModeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setOoaInForwardModeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getOoaInOtgModeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setOoaInOtgModeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatfetLdoModeDuringPreChargeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatfetLdoModeDuringPreChargeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getQonWakeUpTimer() {
  // Return value:
  // 0 = 1000
  // 1 = 15

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setQonWakeUpTimer(int new_value) {
  if (!this->validate_range_(new_value, 0, 1)) return false;

  // Set value:
  // 0 = 1000
  // 1 = 15

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getPfmInForwardModeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setPfmInForwardModeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getPfmInOtgModeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setPfmInOtgModeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getOtgControlEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setOtgControlEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput1And2Disconnected() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput1And2Disconnected(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x12;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBusCurrentOcpInForwardModeEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBusCurrentOcpInForwardModeEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getVindpmDetectionForced() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setVindpmDetectionForced(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getUvpHiccupProtectionOtgModeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setUvpHiccupProtectionOtgModeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getSystemVoltageShortProtectionForwardModeDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSystemVoltageShortProtectionForwardModeDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getStatPinDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setStatPinDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getSwitchingFrequency() {
  // Return value:
  // 0 = 1.5MHz
  // 1 = 750kHz

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSwitchingFrequency(int new_value) {
  if (!this->validate_range_(new_value, 0, 1)) return false;

  // Set value:
  // 0 = 1.5MHz
  // 1 = 750kHz

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput1Enabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput1Enabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput2Enabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput2Enabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x13;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatteryDischargeOcpEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryDischargeOcpEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getExternalInputCurrentLimitEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setExternalInputCurrentLimitEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getSoftwareInputCurrentLimitEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSoftwareInputCurrentLimitEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getBatteryDischargeOcpInOtgMode() {
  // Return value:
  // 0 = 3
  // 1 = 4
  // 2 = 5
  // 3 = -1

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryDischargeOcpInOtgMode(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 3
  // 1 = 4
  // 2 = 5
  // 3 = -1

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatteryCurrentSensingEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryCurrentSensingEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getSfetPresent() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSfetPresent(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x14;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getMpptEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setMpptEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getMpptOcvMeasurementInterval() {
  // Return value:
  // 0 = 30s
  // 1 = 2min
  // 2 = 10min
  // 3 = 30min

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setMpptOcvMeasurementInterval(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 30s
  // 1 = 2min
  // 2 = 10min
  // 3 = 30min

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getWaitTimeBeforeOcvMeasurement() {
  // Return value:
  // 0 = 50
  // 1 = 300
  // 2 = 2000
  // 3 = 5000

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setWaitTimeBeforeOcvMeasurement(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 50
  // 1 = 300
  // 2 = 2000
  // 3 = 5000

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getMpptPercentageOfOcv() {
  // Return value:
  // 0 = 0.5625
  // 1 = 0.625
  // 2 = 0.6875
  // 3 = 0.75
  // 4 = 0.8125
  // 5 = 0.875
  // 6 = 0.9375
  // 7 = 1

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setMpptPercentageOfOcv(int new_value) {
  if (!this->validate_range_(new_value, 0, 7)) return false;

  // Set value:
  // 0 = 0.5625
  // 1 = 0.625
  // 2 = 0.6875
  // 3 = 0.75
  // 4 = 0.8125
  // 5 = 0.875
  // 6 = 0.9375
  // 7 = 1

  uint8_t reg = 0x15;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput2PulldownResistorEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput2PulldownResistorEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput1PulldownResistorEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput1PulldownResistorEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBusVoltagePulldownResistorEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBusVoltagePulldownResistorEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getThermalShutdownThreshold() {
  // Return value:
  // 0 = 150
  // 1 = 130
  // 2 = 120
  // 3 = 85

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setThermalShutdownThreshold(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 150
  // 1 = 130
  // 2 = 120
  // 3 = 85

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getThermalRegulationThreshold() {
  // Return value:
  // 0 = 60
  // 1 = 80
  // 2 = 100
  // 3 = 120

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setThermalRegulationThreshold(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 60
  // 1 = 80
  // 2 = 100
  // 3 = 120

  uint8_t reg = 0x16;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getJeitaLowTemperatureChargeCurrentMultiplier() {
  // Return value:
  // 0 = 0.0
  // 1 = 0.2
  // 2 = 0.4
  // 3 = 1.0

  uint8_t reg = 0x17;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setJeitaLowTemperatureChargeCurrentMultiplier(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 0.0
  // 1 = 0.2
  // 2 = 0.4
  // 3 = 1.0

  uint8_t reg = 0x17;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getJeitaHighTemperatureChargeCurrentMultiplier() {
  // Return value:
  // 0 = 0.0
  // 1 = 0.2
  // 2 = 0.4
  // 3 = 1.0

  uint8_t reg = 0x17;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setJeitaHighTemperatureChargeCurrentMultiplier(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 0.0
  // 1 = 0.2
  // 2 = 0.4
  // 3 = 1.0

  uint8_t reg = 0x17;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getJeitaHighTempChargeVoltageOffset() {
  // Return value:
  // 0 = -1
  // 1 = 800
  // 2 = 600
  // 3 = 400
  // 4 = 300
  // 5 = 200
  // 6 = 100
  // 7 = 0

  uint8_t reg = 0x17;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setJeitaHighTempChargeVoltageOffset(int new_value) {
  if (!this->validate_range_(new_value, 0, 7)) return false;

  // Set value:
  // 0 = -1
  // 1 = 800
  // 2 = 600
  // 3 = 400
  // 4 = 300
  // 5 = 200
  // 6 = 100
  // 7 = 0

  uint8_t reg = 0x17;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getNtcFeedbackDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setNtcFeedbackDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getColdTempThresholdPercentageOtgMode() {
  // Return value:
  // 0 = 75.8 (-10)
  // 1 = 88.7 (-20)

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setColdTempThresholdPercentageOtgMode(int new_value) {
  if (!this->validate_range_(new_value, 0, 1)) return false;

  // Set value:
  // 0 = 75.8 (-10)
  // 1 = 88.7 (-20)

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getHotTempThresholdPercentageOtgMode() {
  // Return value:
  // 0 = 37.7 (55)
  // 1 = 34.4 (60)
  // 2 = 31.3 (65)
  // 3 = -1

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setHotTempThresholdPercentageOtgMode(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 37.7 (55)
  // 1 = 34.4 (60)
  // 2 = 31.3 (65)
  // 3 = -1

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getJeitaVt3Threshold() {
  // Return value:
  // 0 = 48.4 (40)
  // 1 = 44.8 (45)
  // 2 = 41.2 (50)
  // 3 = 37.7 (55)

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setJeitaVt3Threshold(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 48.4 (40)
  // 1 = 44.8 (45)
  // 2 = 41.2 (50)
  // 3 = 37.7 (55)

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getJeitaVt2Threshold() {
  // Return value:
  // 0 = 71.2 (5)
  // 1 = 68.4 (10)
  // 2 = 65.5 (15)
  // 3 = 62.4 (20)

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setJeitaVt2Threshold(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 71.2 (5)
  // 1 = 68.4 (10)
  // 2 = 65.5 (15)
  // 3 = 62.4 (20)

  uint8_t reg = 0x18;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getInputCurrentLimit() {
  // Returns value in: mA

  uint8_t reg = 0x19;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 8;
  uint16_t offset = 0;
  uint8_t lsb = 10;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

bool BQ25672::getBusVoltagePresent() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInput1Present() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInput2Present() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getPowerGood() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getWatchdogTimerExpired() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInVindpmOrVotgRegulation() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInIindpmOrIotgRegulation() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1b;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getUsbBc12DetectComplete() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1c;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

int BQ25672::getBusVoltageStatus() {
  // Return value:
  // 0 = No input or BHOT/BCOLD
  // 1 = USB 0.5A
  // 2 = USB 1.5A
  // 3 = USB 3.25A
  // 4 = Adj HV 1.5A
  // 5 = Unknown 3A
  // 6 = Non standard
  // 7 = In OTG mode
  // 8 = Unqualified
  // 9 = -
  // 10 = -
  // 11 = Direct from VBUS
  // 12 = Backup mode
  // 13 = -
  // 14 = -
  // 15 = -

  uint8_t reg = 0x1c;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

int BQ25672::getChargeStatus() {
  // Return value:
  // 0 = Not charging
  // 1 = Trickle charge
  // 2 = Pre-charge
  // 3 = Fast charge (CC)
  // 4 = Taper charge (CV)
  // 5 = -
  // 6 = Top-off timer active
  // 7 = Charge done

  uint8_t reg = 0x1c;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryPresent() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1d;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getDpdnDetectionBusy() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1d;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInThermalRegulation() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1d;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

int BQ25672::getIcoStatus() {
  // Return value:
  // 0 = Disabled
  // 1 = In progress
  // 2 = Max. input current detected
  // 3 = -

  uint8_t reg = 0x1d;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getPreChargeTimerExpired() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getTrickleChargeTimerExpired() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getFastChargeTimerExpired() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInMinSystemVoltageRegulation() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getAdcConversionDone() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInputFets1Placed() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInputFets2Placed() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryHot() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryWarm() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryCool() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryCold() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryUvloForOtg() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x1f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInput1Ovp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 0;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInput2Ovp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getConverterOcp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryCurrentOcp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBusCurrentOcp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBatteryVoltageOvp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getBusVoltageOvp() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInBatteryDischargeCurrentRegulation() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x20;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInThermalShutdownProtection() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x21;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInOtgUnderVoltage() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x21;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInOtgOverVoltage() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x21;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInSystemOverVoltageProtection() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x21;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::getInSystemShortCircuitProtection() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x21;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

uint8_t BQ25672::getChargerFlag0() {
  // Return value:
  // bit 0 = Bus voltage present changed
  // bit 1 = Input 1 present changed
  // bit 2 = Input 2 present changed
  // bit 3 = Power good changed
  // bit 4 = Poor source detected
  // bit 5 = Watchdog timer passed
  // bit 6 = VINDPM-VOTG regulation signal detected
  // bit 7 = IINDPM-IOTG signal detected

  return flag_readout[0];
}

uint8_t BQ25672::getChargerFlag1() {
  // Return value:
  // bit 0 = BC12 detection status changed
  // bit 1 = Battery present status changed
  // bit 2 = Thermal regulation status changed
  // bit 3 = Reserved
  // bit 4 = Bus voltage status changed
  // bit 5 = Reserved
  // bit 6 = ICO status changed
  // bit 7 = Charge status changed

  return flag_readout[1];
}

uint8_t BQ25672::getChargerFlag2() {
  // Return value:
  // bit 0 = Top off timer expired
  // bit 1 = Pre-charge timer expired
  // bit 2 = Trickle charger timer expired
  // bit 3 = Fast charge timer expired
  // bit 4 = Entered or existed VSYSMIN regulation
  // bit 5 = ADC Conversion completed
  // bit 6 = D+/D- detection is completed
  // bit 7 = Reserved

  return flag_readout[2];
}

uint8_t BQ25672::getChargerFlag3() {
  // Return value:
  // bit 0 = TS across hot temperature (T5) is detected
  // bit 1 = TS across warm temperature (T3) is detected
  // bit 2 = TS across cool temperature (T2) is detected
  // bit 3 = TS across cold temperature (T1) is detected
  // bit 4 = VBAT falls below the threshold to enable the OTG mode
  // bit 5 = Reserved
  // bit 6 = Reserved
  // bit 7 = Reserved

  return flag_readout[3];
}

uint8_t BQ25672::getFaultFlag0() {
  // Return value:
  // bit 0 = Enter VAC1 OVP
  // bit 1 = Enter VAC2 OVP
  // bit 2 = Enter converter OCP
  // bit 3 = Enter discharged OCP
  // bit 4 = Enter IBUS OCP
  // bit 5 = Enter VBAT OVP
  // bit 6 = Enter VBUS OVP
  // bit 7 = Enter or exit IBAT regulation

  return flag_readout[4];
}

uint8_t BQ25672::getFaultFlag() {
  // Return value:
  // bit 0 = Reserved
  // bit 1 = Reserved
  // bit 2 = TS shutdown signal rising threshold detected
  // bit 3 = Reserved
  // bit 4 = Stop OTG due to VBUS under-voltage
  // bit 5 = Stop OTG due to VBUS over voltage
  // bit 6 = Stop switching due to system over-voltage
  // bit 7 = Stop switching due to system short

  return flag_readout[5];
}

bool BQ25672::getStartAverageWithNewAdcConversion() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setStartAverageWithNewAdcConversion(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getRunningAverageEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setRunningAverageEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getAdcResolution() {
  // Return value:
  // 0 = 15
  // 1 = 14
  // 2 = 13
  // 3 = 12

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setAdcResolution(int new_value) {
  if (!this->validate_range_(new_value, 0, 3)) return false;

  // Set value:
  // 0 = 15
  // 1 = 14
  // 2 = 13
  // 3 = 12

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getAdcConversion() {
  // Return value:
  // 0 = Continuous
  // 1 = One shot

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setAdcConversion(int new_value) {
  if (!this->validate_range_(new_value, 0, 1)) return false;

  // Set value:
  // 0 = Continuous
  // 1 = One shot

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getAdcEnabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setAdcEnabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2e;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getDieTemperatureAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setDieTemperatureAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 1;
  uint8_t bit_end = 1;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getNtcAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setNtcAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 2;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getSystemVoltageAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setSystemVoltageAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 3;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatteryVoltageAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryVoltageAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBusVoltageAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBusVoltageAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBatteryCurrentAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBatteryCurrentAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getBusCurrentAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setBusCurrentAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x2f;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput1AdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput1AdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 4;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getInput2AdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setInput2AdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 5;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getDnAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setDnAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 6;
  uint8_t bit_end = 6;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

bool BQ25672::getDpAdcControlDisabled() {
  // Return value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setDpAdcControlDisabled(bool new_value) {
  // Set value:
  // 0 = NO
  // 1 = YES

  uint8_t reg = 0x30;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 7;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getInputCurrent() {
  // Returns value in: mA

  uint8_t reg = 0x31;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return decode_signed_word_(val) * lsb + offset;  // Signed two's-complement ADC result.
}

int BQ25672::getBatteryCurrent() {
  // Returns value in: mA

  uint8_t reg = 0x33;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return decode_signed_word_(val) * lsb + offset;  // Signed two's-complement ADC result.
}

int BQ25672::getBusVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x35;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getInput1Voltage() {
  // Returns value in: mV

  uint8_t reg = 0x37;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getInput2Voltage() {
  // Returns value in: mV

  uint8_t reg = 0x39;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getBatteryVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x3b;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getSystemVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x3d;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

float BQ25672::getNtcReading() {
  // Returns value in: %

  uint8_t reg = 0x3f;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  float offset = 0;
  float lsb = 100.0f / 1024.0f;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

float BQ25672::getDieTemperature() {
  // Returns value in: C

  uint8_t reg = 0x41;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  float offset = 0;
  float lsb = 0.5f;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return decode_signed_word_(val) * lsb + offset;  // Signed two's-complement ADC result.
}

int BQ25672::getDpVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x43;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getDnVoltage() {
  // Returns value in: mV

  uint8_t reg = 0x45;
  uint8_t byte_cnt = 2;
  uint8_t bit_start = 0;
  uint8_t bit_end = 15;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getDnOutput() {
  // Return value:
  // 0 = HIZ
  // 1 = 0V
  // 2 = 0.6V
  // 3 = 1.2V
  // 4 = 2.0V
  // 5 = 2.7V
  // 6 = 3.3
  // 7 = -

  uint8_t reg = 0x47;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 4;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setDnOutput(int new_value) {
  if (!this->validate_range_(new_value, 0, 7)) return false;

  // Set value:
  // 0 = HIZ
  // 1 = 0V
  // 2 = 0.6V
  // 3 = 1.2V
  // 4 = 2.0V
  // 5 = 2.7V
  // 6 = 3.3
  // 7 = -

  uint8_t reg = 0x47;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 2;
  uint8_t bit_end = 4;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getDpOutput() {
  // Return value:
  // 0 = HIZ
  // 1 = 0V
  // 2 = 0.6V
  // 3 = 1.2V
  // 4 = 2.0V
  // 5 = 2.7V
  // 6 = 3.3
  // 7 = DPDN short

  uint8_t reg = 0x47;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val;
}

bool BQ25672::setDpOutput(int new_value) {
  if (!this->validate_range_(new_value, 0, 7)) return false;

  // Set value:
  // 0 = HIZ
  // 1 = 0V
  // 2 = 0.6V
  // 3 = 1.2V
  // 4 = 2.0V
  // 5 = 2.7V
  // 6 = 3.3
  // 7 = DPDN short

  uint8_t reg = 0x47;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 5;
  uint8_t bit_end = 7;

  uint16_t new_data = (uint16_t) new_value;

  bool success  = write_var(reg, byte_cnt, bit_start, bit_end, new_data);
  return success;
}

int BQ25672::getDeviceRevision() {
  // Returns value in: 

  uint8_t reg = 0x48;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 0;
  uint8_t bit_end = 2;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

int BQ25672::getDevicePartNr() {
  // Returns value in: 

  uint8_t reg = 0x48;
  uint8_t byte_cnt = 1;
  uint8_t bit_start = 3;
  uint8_t bit_end = 5;
  uint16_t offset = 0;
  uint8_t lsb = 1;

  uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
  return val * lsb + offset;
}

// The following helpers were declared, but not defined, in upstream 0.0.1.
bool BQ25672::getResetRegistersToDefault() { return this->read_var(0x09, 1, 6, 6); }
bool BQ25672::ResetRegistersToDefault(bool new_value) { return this->write_var(0x09, 1, 6, 6, new_value); }
bool BQ25672::getForceIcoStart() { return this->read_var(0x0F, 1, 3, 3); }
bool BQ25672::ForceIcoStart(bool new_value) { return this->write_var(0x0F, 1, 3, 3, new_value); }
bool BQ25672::getResetWatchdogTimer() { return this->read_var(0x10, 1, 3, 3); }
bool BQ25672::ResetWatchdogTimer(bool new_value) { return this->write_var(0x10, 1, 3, 3, new_value); }
bool BQ25672::getForceDpdnDetection() { return this->read_var(0x11, 1, 7, 7); }
bool BQ25672::ForceDpdnDetection(bool new_value) { return this->write_var(0x11, 1, 7, 7, new_value); }

bool BQ25672::readFlags() {
  if (this->bus_ == nullptr) return this->record_result_(false);
  uint8_t next_flags[6] = {0};
  // These registers clear on read. Publish the new cache only after the whole
  // transaction succeeds, so a failed read never exposes a partly new cache.
  if (this->read_register(0x22, next_flags, sizeof(next_flags)) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "I2C flag read failed");
    return this->record_result_(false);
  }
  for (uint8_t i = 0; i < sizeof(next_flags); i++) this->flag_readout[i] = next_flags[i];
  ESP_LOGD(TAG, "Charger flags: %02X %02X %02X %02X; fault flags: %02X %02X",
           next_flags[0], next_flags[1], next_flags[2], next_flags[3], next_flags[4], next_flags[5]);
  return this->record_result_(true);
}

}  // namespace bq25672
}  // namespace esphome
