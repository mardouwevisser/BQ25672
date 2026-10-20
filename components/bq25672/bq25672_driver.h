#pragma once

// Native ESPHome port of Marc Visser's BQ25672 Arduino library.
// Copyright (c) 2023-2024 Marc Visser. MIT License; see LICENSE.
// Upstream: https://github.com/mardouwevisser/BQ25672
// Commit: 1e3e59f5c7b58a1191d9e37c8c34d0cc12654ce9

#include <cstdint>
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bq25672 {

/** Complete upstream register API using the ESPHome I2C bus.
 *
 * Physical values retain the upstream units (mV, mA, degrees C or TS percent).
 * Enumerated settings retain their raw register codes; for example watchdog
 * 0 disables the timer and ADC resolution 0 selects 15 effective bits.
 * Scaled integer writes round down to the next register step, as upstream did.
 * Out-of-range arguments now fail before I2C access.
 *
 * Getter return types are unchanged. A failed read returns the upstream
 * fallback value, so check last_operation_ok() before using a result, or call
 * clear_io_error() followed by a series of reads and check io_ok() afterwards.
 * A successful write means that the I2C transaction was acknowledged. Read
 * back the setting when application code needs to verify the accepted value.
 */
class BQ25672 : public i2c::I2CDevice {
 public:
  BQ25672() { this->set_i2c_address(0x6B); }

  void clear_io_error() {
    this->io_error_ = false;
    this->last_operation_ok_ = true;
  }
  bool io_ok() const { return !this->io_error_; }
  bool last_operation_ok() const { return this->last_operation_ok_; }

  bool readFlags();

  int getMinSystemVoltage();
  bool setMinSystemVoltage(int new_value);
  int getChargeVoltage();
  bool setChargeVoltage(int new_value);
  int getChargeCurrent();
  bool setChargeCurrent(int new_value);
  int getVindpmThreshold();
  bool setVindpmThreshold(int new_value);
  int getInputCurrentLimitRegister();
  bool setInputCurrentLimitRegister(int new_value);
  int getPreChargeCurrent();
  bool setPreChargeCurrent(int new_value);
  int getPrechrgFastchrgThreshold();
  bool setPrechrgFastchrgThreshold(int new_value);
  int getTerminationCurrent();
  bool setTerminationCurrent(int new_value);
  bool getWatchdogTimerDisablesCharging();
  bool setWatchdogTimerDisablesCharging(bool new_value);
  bool getResetRegistersToDefault();
  bool ResetRegistersToDefault(bool new_value = true);
  int getBatteryRechargeThreshold();
  bool setBatteryRechargeThreshold(int new_value);
  int getBatteryRechargeDeglitchTime();
  bool setBatteryRechargeDeglitchTime(int new_value);
  int getBatterySeriesCount();
  bool setBatterySeriesCount(int new_value);
  int getOtgVoltage();
  bool setOtgVoltage(int new_value);
  int getOtgCurrentLimit();
  bool setOtgCurrentLimit(int new_value);
  int getPreChargeTimer();
  bool setPreChargeTimer(int new_value);
  bool getSlowPreAndTrickleChargeDuringThermalReg();
  bool setSlowPreAndTrickleChargeDuringThermalReg(bool new_value);
  int getFastChargeTimer();
  bool setFastChargeTimer(int new_value);
  bool getFastChargeTimerEnabled();
  bool setFastChargeTimerEnabled(bool new_value);
  bool getPreChargeTimerEnabled();
  bool setPreChargeTimerEnabled(bool new_value);
  bool getTrickleChargeTimerEnabled();
  bool setTrickleChargeTimerEnabled(bool new_value);
  int getTopOffTimer();
  bool setTopOffTimer(int new_value);
  bool getTerminationEnabled();
  bool setTerminationEnabled(bool new_value);
  bool getHizModeEnabled();
  bool setHizModeEnabled(bool new_value);
  bool getForceIcoStart();
  bool ForceIcoStart(bool new_value = true);
  bool getIcoEnabled();
  bool setIcoEnabled(bool new_value);
  bool getChargingEnabled();
  bool setChargingEnabled(bool new_value);
  bool getBatteryDischargeCurrentForced();
  bool setBatteryDischargeCurrentForced(bool new_value);
  bool getBatteryDischargeDuringOvpEnabled();
  bool setBatteryDischargeDuringOvpEnabled(bool new_value);
  int getWatchdogTimerTime();
  bool setWatchdogTimerTime(int new_value);
  bool getResetWatchdogTimer();
  bool ResetWatchdogTimer(bool new_value = true);
  int getInputOverVoltageThreshold();
  bool setInputOverVoltageThreshold(int new_value);
  bool getSfet10sDelayRemoved();
  bool setSfet10sDelayRemoved(bool new_value);
  int getSfetControl();
  bool setSfetControl(int new_value);
  bool getHighVoltageDcpHandshakeEnabled();
  bool setHighVoltageDcpHandshakeEnabled(bool new_value);
  bool getHvdc9vEnabled();
  bool setHvdc9vEnabled(bool new_value);
  bool getHvdc12vEnabled();
  bool setHvdc12vEnabled(bool new_value);
  bool getAutoDpdnDetectionEnabled();
  bool setAutoDpdnDetectionEnabled(bool new_value);
  bool getForceDpdnDetection();
  bool ForceDpdnDetection(bool new_value = true);
  bool getOoaInForwardModeDisabled();
  bool setOoaInForwardModeDisabled(bool new_value);
  bool getOoaInOtgModeDisabled();
  bool setOoaInOtgModeDisabled(bool new_value);
  bool getBatfetLdoModeDuringPreChargeDisabled();
  bool setBatfetLdoModeDuringPreChargeDisabled(bool new_value);
  int getQonWakeUpTimer();
  bool setQonWakeUpTimer(int new_value);
  bool getPfmInForwardModeDisabled();
  bool setPfmInForwardModeDisabled(bool new_value);
  bool getPfmInOtgModeDisabled();
  bool setPfmInOtgModeDisabled(bool new_value);
  bool getOtgControlEnabled();
  bool setOtgControlEnabled(bool new_value);
  bool getInput1And2Disconnected();
  bool setInput1And2Disconnected(bool new_value);
  bool getBusCurrentOcpInForwardModeEnabled();
  bool setBusCurrentOcpInForwardModeEnabled(bool new_value);
  bool getVindpmDetectionForced();
  bool setVindpmDetectionForced(bool new_value);
  bool getUvpHiccupProtectionOtgModeDisabled();
  bool setUvpHiccupProtectionOtgModeDisabled(bool new_value);
  bool getSystemVoltageShortProtectionForwardModeDisabled();
  bool setSystemVoltageShortProtectionForwardModeDisabled(bool new_value);
  bool getStatPinDisabled();
  bool setStatPinDisabled(bool new_value);
  int getSwitchingFrequency();
  bool setSwitchingFrequency(int new_value);
  bool getInput1Enabled();
  bool setInput1Enabled(bool new_value);
  bool getInput2Enabled();
  bool setInput2Enabled(bool new_value);
  bool getBatteryDischargeOcpEnabled();
  bool setBatteryDischargeOcpEnabled(bool new_value);
  bool getExternalInputCurrentLimitEnabled();
  bool setExternalInputCurrentLimitEnabled(bool new_value);
  bool getSoftwareInputCurrentLimitEnabled();
  bool setSoftwareInputCurrentLimitEnabled(bool new_value);
  int getBatteryDischargeOcpInOtgMode();
  bool setBatteryDischargeOcpInOtgMode(int new_value);
  bool getBatteryCurrentSensingEnabled();
  bool setBatteryCurrentSensingEnabled(bool new_value);
  bool getSfetPresent();
  bool setSfetPresent(bool new_value);
  bool getMpptEnabled();
  bool setMpptEnabled(bool new_value);
  int getMpptOcvMeasurementInterval();
  bool setMpptOcvMeasurementInterval(int new_value);
  int getWaitTimeBeforeOcvMeasurement();
  bool setWaitTimeBeforeOcvMeasurement(int new_value);
  int getMpptPercentageOfOcv();
  bool setMpptPercentageOfOcv(int new_value);
  bool getInput2PulldownResistorEnabled();
  bool setInput2PulldownResistorEnabled(bool new_value);
  bool getInput1PulldownResistorEnabled();
  bool setInput1PulldownResistorEnabled(bool new_value);
  bool getBusVoltagePulldownResistorEnabled();
  bool setBusVoltagePulldownResistorEnabled(bool new_value);
  int getThermalShutdownThreshold();
  bool setThermalShutdownThreshold(int new_value);
  int getThermalRegulationThreshold();
  bool setThermalRegulationThreshold(int new_value);
  int getJeitaLowTemperatureChargeCurrentMultiplier();
  bool setJeitaLowTemperatureChargeCurrentMultiplier(int new_value);
  int getJeitaHighTemperatureChargeCurrentMultiplier();
  bool setJeitaHighTemperatureChargeCurrentMultiplier(int new_value);
  int getJeitaHighTempChargeVoltageOffset();
  bool setJeitaHighTempChargeVoltageOffset(int new_value);
  bool getNtcFeedbackDisabled();
  bool setNtcFeedbackDisabled(bool new_value);
  int getColdTempThresholdPercentageOtgMode();
  bool setColdTempThresholdPercentageOtgMode(int new_value);
  int getHotTempThresholdPercentageOtgMode();
  bool setHotTempThresholdPercentageOtgMode(int new_value);
  int getJeitaVt3Threshold();
  bool setJeitaVt3Threshold(int new_value);
  int getJeitaVt2Threshold();
  bool setJeitaVt2Threshold(int new_value);
  int getInputCurrentLimit();
  bool getBusVoltagePresent();
  bool getInput1Present();
  bool getInput2Present();
  bool getPowerGood();
  bool getWatchdogTimerExpired();
  bool getInVindpmOrVotgRegulation();
  bool getInIindpmOrIotgRegulation();
  bool getUsbBc12DetectComplete();
  int getBusVoltageStatus();
  int getChargeStatus();
  bool getBatteryPresent();
  bool getDpdnDetectionBusy();
  bool getInThermalRegulation();
  int getIcoStatus();
  bool getPreChargeTimerExpired();
  bool getTrickleChargeTimerExpired();
  bool getFastChargeTimerExpired();
  bool getInMinSystemVoltageRegulation();
  bool getAdcConversionDone();
  bool getInputFets1Placed();
  bool getInputFets2Placed();
  bool getBatteryHot();
  bool getBatteryWarm();
  bool getBatteryCool();
  bool getBatteryCold();
  bool getBatteryUvloForOtg();
  bool getInput1Ovp();
  bool getInput2Ovp();
  bool getConverterOcp();
  bool getBatteryCurrentOcp();
  bool getBusCurrentOcp();
  bool getBatteryVoltageOvp();
  bool getBusVoltageOvp();
  bool getInBatteryDischargeCurrentRegulation();
  bool getInThermalShutdownProtection();
  bool getInOtgUnderVoltage();
  bool getInOtgOverVoltage();
  bool getInSystemOverVoltageProtection();
  bool getInSystemShortCircuitProtection();
  uint8_t getChargerFlag0();
  uint8_t getChargerFlag1();
  uint8_t getChargerFlag2();
  uint8_t getChargerFlag3();
  uint8_t getFaultFlag0();
  uint8_t getFaultFlag();
  bool getStartAverageWithNewAdcConversion();
  bool setStartAverageWithNewAdcConversion(bool new_value);
  bool getRunningAverageEnabled();
  bool setRunningAverageEnabled(bool new_value);
  int getAdcResolution();
  bool setAdcResolution(int new_value);
  int getAdcConversion();
  bool setAdcConversion(int new_value);
  bool getAdcEnabled();
  bool setAdcEnabled(bool new_value);
  bool getDieTemperatureAdcControlDisabled();
  bool setDieTemperatureAdcControlDisabled(bool new_value);
  bool getNtcAdcControlDisabled();
  bool setNtcAdcControlDisabled(bool new_value);
  bool getSystemVoltageAdcControlDisabled();
  bool setSystemVoltageAdcControlDisabled(bool new_value);
  bool getBatteryVoltageAdcControlDisabled();
  bool setBatteryVoltageAdcControlDisabled(bool new_value);
  bool getBusVoltageAdcControlDisabled();
  bool setBusVoltageAdcControlDisabled(bool new_value);
  bool getBatteryCurrentAdcControlDisabled();
  bool setBatteryCurrentAdcControlDisabled(bool new_value);
  bool getBusCurrentAdcControlDisabled();
  bool setBusCurrentAdcControlDisabled(bool new_value);
  bool getInput1AdcControlDisabled();
  bool setInput1AdcControlDisabled(bool new_value);
  bool getInput2AdcControlDisabled();
  bool setInput2AdcControlDisabled(bool new_value);
  bool getDnAdcControlDisabled();
  bool setDnAdcControlDisabled(bool new_value);
  bool getDpAdcControlDisabled();
  bool setDpAdcControlDisabled(bool new_value);
  int getInputCurrent();
  int getBatteryCurrent();
  int getBusVoltage();
  int getInput1Voltage();
  int getInput2Voltage();
  int getBatteryVoltage();
  int getSystemVoltage();
  float getNtcReading();
  float getDieTemperature();
  int getDpVoltage();
  int getDnVoltage();
  int getDnOutput();
  bool setDnOutput(int new_value);
  int getDpOutput();
  bool setDpOutput(int new_value);
  int getDeviceRevision();
  int getDevicePartNr();

 protected:
  bool read_bytes(uint8_t reg, uint16_t *data, uint8_t byte_cnt);
  bool write_bytes(uint8_t reg, uint16_t data, uint8_t byte_cnt);
  uint16_t read_var(uint8_t reg, uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end);
  bool write_var(uint8_t reg, uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end, uint16_t new_data);

  bool record_result_(bool success) {
    this->last_operation_ok_ = success;
    this->io_error_ |= !success;
    return success;
  }
  bool validate_range_(int value, int minimum, int maximum);

 private:
  uint8_t flag_readout[6] = {0};
  bool io_error_{false};
  bool last_operation_ok_{true};
};

}  // namespace bq25672
}  // namespace esphome
