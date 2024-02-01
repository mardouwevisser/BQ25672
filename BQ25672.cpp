/*
  FILE:    BQ25672.cpp
  AUTHOR:  Marc Visser
  VERSION: 0.0.1
  PURPOSE: BQ25672 library for the Arduino IDE
  URL:     https://github.com/mardouwevisser/BQ25672
  LICENCE: See LICENCE file
*/

#include "BQ25672.h"

BQ25672::BQ25672():
	timeout_time(50), _Serial(NULL) {
}

BQ25672::BQ25672(HardwareSerial *serial):
	timeout_time(50) {
	_Serial = serial;
}

BQ25672::~BQ25672() {
}

int BQ25672::begin() {
	_i2caddr = 0x6B;
	_bus = &Wire;

	_bus->begin();
	_bus->beginTransmission(_i2caddr);
	int error = _bus->endTransmission();
	return error;
}


int BQ25672::begin(TwoWire *i2c_bus) {
	_i2caddr = 0x6B;
	_bus = i2c_bus;

	_bus->begin();
	_bus->beginTransmission(_i2caddr);
	int error = _bus->endTransmission();
	return error;
}

#ifdef ESP32
int BQ25672::begin(int sda, int scl, uint32_t frequency) {
	_i2caddr = 0x6B;
	_bus = &Wire;

	_bus->begin(sda, scl, frequency);
	_bus->beginTransmission(_i2caddr);
	int error = _bus->endTransmission();
	return error;
}

int BQ25672::begin(TwoWire *i2c_bus, int sda, int scl, uint32_t frequency) {
	_i2caddr = 0x6B;
	_bus = i2c_bus;

	_bus->begin(sda, scl, frequency);
	_bus->beginTransmission(_i2caddr);
	int error = _bus->endTransmission();
	return error;
}

#endif

bool BQ25672::read_bytes(uint8_t reg, uint16_t *data, uint8_t byte_cnt){
	_bus->beginTransmission(_i2caddr);
	_bus->write(reg);
	int error = _bus->endTransmission();

	if(error){
		return false;
	}

	_bus->requestFrom(_i2caddr, byte_cnt);
	unsigned long timeout_timer = millis();
	while (_bus->available() < byte_cnt){
		if(millis() - timeout_timer > timeout_time) return false;
	}

	uint8_t _data[2];
	for(int i = 0; i< byte_cnt; i++){
		_data[i] = _bus->read();
	}

	if(byte_cnt == 1){
		*data = _data[0];
	}
	else if(byte_cnt == 2){
		*data = (_data[0] << 8) | _data[1];
	}
	else{
		*data = 0;
		return false;
	}
	return true;
}

uint16_t BQ25672::read_var(uint8_t reg, uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end){
	uint16_t data;
	bool success = read_bytes(reg, &data, byte_cnt);
	if(!success) return 0;

	uint16_t bit_mask = (0xFFFF >> (16 - (bit_end - bit_start + 1))) << bit_start;

	data = data & bit_mask;
	data = data >> bit_start;
	return data;
}

bool BQ25672::write_bytes(uint8_t reg, uint16_t data, uint8_t byte_cnt){
	_bus->beginTransmission(_i2caddr);
	_bus->write(reg);

	if (byte_cnt == 1){
		uint8_t _data = data & 0x00FF;
		_bus->write(_data);
	}
    else if(byte_cnt == 2){
		uint8_t _data[2];
		_data[0] = (data & 0xFF00) >> 8;
		_data[1] = data & 0x00FF;
		_bus->write(_data, 2);
	}
	else{
		return false;
	}

	int error = _bus->endTransmission();

	if(error){
		return false;
	}
	return true;
}

bool BQ25672::write_var(uint8_t reg, uint8_t byte_cnt, uint8_t bit_start, uint8_t bit_end, uint16_t new_data){
	if(!((1 << bit_end - bit_start + 1) > new_data && new_data >= 0)){
		// Data is out of range
		return false;
	}
	uint16_t old_data;
	bool success = read_bytes(reg, &old_data, byte_cnt);

	if(!success){
		return false;
	}

	uint16_t old_data_lsb_mask = 0xFFFF >> (16 - bit_start);
	uint16_t old_data_msb_mask = (0xFFFF >> (bit_end + 1)) << (bit_end + 1);

	uint16_t old_data_lsb = old_data & old_data_lsb_mask;
	uint16_t old_data_msb = old_data & old_data_msb_mask;
	new_data = new_data << bit_start;

	new_data = new_data | old_data_lsb;
	new_data = new_data | old_data_msb;

	success = write_bytes(reg, new_data, byte_cnt);

	// TODO: Finish function with read back check

	return success;
}



int BQ25672::getMinSystemVoltage(){
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

bool BQ25672::setMinSystemVoltage(int new_value){
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

int BQ25672::getChargeVoltage(){
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

bool BQ25672::setChargeVoltage(int new_value){
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

int BQ25672::getChargeCurrent(){
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

bool BQ25672::setChargeCurrent(int new_value){
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

int BQ25672::getVindpmThreshold(){
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

bool BQ25672::setVindpmThreshold(int new_value){
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

int BQ25672::getInputCurrentLimitRegister(){
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

bool BQ25672::setInputCurrentLimitRegister(int new_value){
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

int BQ25672::getPreChargeCurrent(){
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

bool BQ25672::setPreChargeCurrent(int new_value){
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

int BQ25672::getPrechrgFastchrgThreshold(){
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

bool BQ25672::setPrechrgFastchrgThreshold(int new_value){
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

int BQ25672::getTerminationCurrent(){
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

bool BQ25672::setTerminationCurrent(int new_value){
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

bool BQ25672::getWatchdogTimerDisablesCharging(){
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

bool BQ25672::setWatchdogTimerDisablesCharging(bool new_value){
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

int BQ25672::getBatteryRechargeThreshold(){
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

bool BQ25672::setBatteryRechargeThreshold(int new_value){
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

int BQ25672::getBatteryRechargeDeglitchTime(){
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

bool BQ25672::setBatteryRechargeDeglitchTime(int new_value){
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

int BQ25672::getBatterySeriesCount(){
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

bool BQ25672::setBatterySeriesCount(int new_value){
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

int BQ25672::getOtgVoltage(){
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

bool BQ25672::setOtgVoltage(int new_value){
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

int BQ25672::getOtgCurrentLimit(){
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

bool BQ25672::setOtgCurrentLimit(int new_value){
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

int BQ25672::getPreChargeTimer(){
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

bool BQ25672::setPreChargeTimer(int new_value){
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

bool BQ25672::getSlowPreAndTrickleChargeDuringThermalReg(){
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

bool BQ25672::setSlowPreAndTrickleChargeDuringThermalReg(bool new_value){
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

int BQ25672::getFastChargeTimer(){
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

bool BQ25672::setFastChargeTimer(int new_value){
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

bool BQ25672::getFastChargeTimerEnabled(){
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

bool BQ25672::setFastChargeTimerEnabled(bool new_value){
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

bool BQ25672::getPreChargeTimerEnabled(){
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

bool BQ25672::setPreChargeTimerEnabled(bool new_value){
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

bool BQ25672::getTrickleChargeTimerEnabled(){
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

bool BQ25672::setTrickleChargeTimerEnabled(bool new_value){
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

int BQ25672::getTopOffTimer(){
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

bool BQ25672::setTopOffTimer(int new_value){
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

bool BQ25672::getTerminationEnabled(){
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

bool BQ25672::setTerminationEnabled(bool new_value){
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

bool BQ25672::getHizModeEnabled(){
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

bool BQ25672::setHizModeEnabled(bool new_value){
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

bool BQ25672::getIcoEnabled(){
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

bool BQ25672::setIcoEnabled(bool new_value){
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

bool BQ25672::getChargingEnabled(){
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

bool BQ25672::setChargingEnabled(bool new_value){
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

bool BQ25672::getBatteryDischargeCurrentForced(){
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

bool BQ25672::setBatteryDischargeCurrentForced(bool new_value){
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

bool BQ25672::getBatteryDischargeDuringOvpEnabled(){
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

bool BQ25672::setBatteryDischargeDuringOvpEnabled(bool new_value){
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

int BQ25672::getWatchdogTimerTime(){
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

bool BQ25672::setWatchdogTimerTime(int new_value){
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

int BQ25672::getInputOverVoltageThreshold(){
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

bool BQ25672::setInputOverVoltageThreshold(int new_value){
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

bool BQ25672::getSfet10sDelayRemoved(){
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

bool BQ25672::setSfet10sDelayRemoved(bool new_value){
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

int BQ25672::getSfetControl(){
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

bool BQ25672::setSfetControl(int new_value){
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

bool BQ25672::getHighVoltageDcpHandshakeEnabled(){
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

bool BQ25672::setHighVoltageDcpHandshakeEnabled(bool new_value){
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

bool BQ25672::getHvdc9vEnabled(){
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

bool BQ25672::setHvdc9vEnabled(bool new_value){
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

bool BQ25672::getHvdc12vEnabled(){
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

bool BQ25672::setHvdc12vEnabled(bool new_value){
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

bool BQ25672::getAutoDpdnDetectionEnabled(){
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

bool BQ25672::setAutoDpdnDetectionEnabled(bool new_value){
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

bool BQ25672::getOoaInForwardModeDisabled(){
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

bool BQ25672::setOoaInForwardModeDisabled(bool new_value){
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

bool BQ25672::getOoaInOtgModeDisabled(){
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

bool BQ25672::setOoaInOtgModeDisabled(bool new_value){
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

bool BQ25672::getBatfetLdoModeDuringPreChargeDisabled(){
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

bool BQ25672::setBatfetLdoModeDuringPreChargeDisabled(bool new_value){
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

int BQ25672::getQonWakeUpTimer(){
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

bool BQ25672::setQonWakeUpTimer(int new_value){
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

bool BQ25672::getPfmInForwardModeDisabled(){
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

bool BQ25672::setPfmInForwardModeDisabled(bool new_value){
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

bool BQ25672::getPfmInOtgModeDisabled(){
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

bool BQ25672::setPfmInOtgModeDisabled(bool new_value){
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

bool BQ25672::getOtgControlEnabled(){
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

bool BQ25672::setOtgControlEnabled(bool new_value){
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

bool BQ25672::getInput1And2Disconnected(){
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

bool BQ25672::setInput1And2Disconnected(bool new_value){
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

bool BQ25672::getBusCurrentOcpInForwardModeEnabled(){
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

bool BQ25672::setBusCurrentOcpInForwardModeEnabled(bool new_value){
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

bool BQ25672::getVindpmDetectionForced(){
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

bool BQ25672::setVindpmDetectionForced(bool new_value){
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

bool BQ25672::getUvpHiccupProtectionOtgModeDisabled(){
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

bool BQ25672::setUvpHiccupProtectionOtgModeDisabled(bool new_value){
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

bool BQ25672::getSystemVoltageShortProtectionForwardModeDisabled(){
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

bool BQ25672::setSystemVoltageShortProtectionForwardModeDisabled(bool new_value){
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

bool BQ25672::getStatPinDisabled(){
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

bool BQ25672::setStatPinDisabled(bool new_value){
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

int BQ25672::getSwitchingFrequency(){
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

bool BQ25672::setSwitchingFrequency(int new_value){
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

bool BQ25672::getInput1Enabled(){
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

bool BQ25672::setInput1Enabled(bool new_value){
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

bool BQ25672::getInput2Enabled(){
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

bool BQ25672::setInput2Enabled(bool new_value){
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

bool BQ25672::getBatteryDischargeOcpEnabled(){
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

bool BQ25672::setBatteryDischargeOcpEnabled(bool new_value){
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

bool BQ25672::getExternalInputCurrentLimitEnabled(){
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

bool BQ25672::setExternalInputCurrentLimitEnabled(bool new_value){
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

bool BQ25672::getSoftwareInputCurrentLimitEnabled(){
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

bool BQ25672::setSoftwareInputCurrentLimitEnabled(bool new_value){
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

int BQ25672::getBatteryDischargeOcpInOtgMode(){
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

bool BQ25672::setBatteryDischargeOcpInOtgMode(int new_value){
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

bool BQ25672::getBatteryCurrentSensingEnabled(){
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

bool BQ25672::setBatteryCurrentSensingEnabled(bool new_value){
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

bool BQ25672::getSfetPresent(){
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

bool BQ25672::setSfetPresent(bool new_value){
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

bool BQ25672::getMpptEnabled(){
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

bool BQ25672::setMpptEnabled(bool new_value){
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

int BQ25672::getMpptOcvMeasurementInterval(){
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

bool BQ25672::setMpptOcvMeasurementInterval(int new_value){
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

int BQ25672::getWaitTimeBeforeOcvMeasurement(){
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

bool BQ25672::setWaitTimeBeforeOcvMeasurement(int new_value){
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

int BQ25672::getMpptPercentageOfOcv(){
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

bool BQ25672::setMpptPercentageOfOcv(int new_value){
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

bool BQ25672::getInput2PulldownResistorEnabled(){
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

bool BQ25672::setInput2PulldownResistorEnabled(bool new_value){
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

bool BQ25672::getInput1PulldownResistorEnabled(){
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

bool BQ25672::setInput1PulldownResistorEnabled(bool new_value){
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

bool BQ25672::getBusVoltagePulldownResistorEnabled(){
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

bool BQ25672::setBusVoltagePulldownResistorEnabled(bool new_value){
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

int BQ25672::getThermalShutdownThreshold(){
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

bool BQ25672::setThermalShutdownThreshold(int new_value){
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

int BQ25672::getThermalRegulationThreshold(){
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

bool BQ25672::setThermalRegulationThreshold(int new_value){
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

int BQ25672::getJeitaLowTemperatureChargeCurrentMultiplier(){
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

bool BQ25672::setJeitaLowTemperatureChargeCurrentMultiplier(int new_value){
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

int BQ25672::getJeitaHighTemperatureChargeCurrentMultiplier(){
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

bool BQ25672::setJeitaHighTemperatureChargeCurrentMultiplier(int new_value){
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

int BQ25672::getJeitaHighTempChargeVoltageOffset(){
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

bool BQ25672::setJeitaHighTempChargeVoltageOffset(int new_value){
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

bool BQ25672::getNtcFeedbackDisabled(){
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

bool BQ25672::setNtcFeedbackDisabled(bool new_value){
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

int BQ25672::getColdTempThresholdPercentageOtgMode(){
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

bool BQ25672::setColdTempThresholdPercentageOtgMode(int new_value){
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

int BQ25672::getHotTempThresholdPercentageOtgMode(){
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

bool BQ25672::setHotTempThresholdPercentageOtgMode(int new_value){
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

int BQ25672::getJeitaVt3Threshold(){
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

bool BQ25672::setJeitaVt3Threshold(int new_value){
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

int BQ25672::getJeitaVt2Threshold(){
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

bool BQ25672::setJeitaVt2Threshold(int new_value){
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

int BQ25672::getInputCurrentLimit(){
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

bool BQ25672::getBusVoltagePresent(){
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

bool BQ25672::getInput1Present(){
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

bool BQ25672::getInput2Present(){
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

bool BQ25672::getPowerGood(){
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

bool BQ25672::getWatchdogTimerExpired(){
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

bool BQ25672::getInVindpmOrVotgRegulation(){
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

bool BQ25672::getInIindpmOrIotgRegulation(){
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

bool BQ25672::getUsbBc12DetectComplete(){
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

int BQ25672::getBusVoltageStatus(){
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

int BQ25672::getChargeStatus(){
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

bool BQ25672::getBatteryPresent(){
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

bool BQ25672::getDpdnDetectionBusy(){
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

bool BQ25672::getInThermalRegulation(){
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

int BQ25672::getIcoStatus(){
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

bool BQ25672::getPreChargeTimerExpired(){
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

bool BQ25672::getTrickleChargeTimerExpired(){
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

bool BQ25672::getFastChargeTimerExpired(){
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

bool BQ25672::getInMinSystemVoltageRegulation(){
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

bool BQ25672::getAdcConversionDone(){
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

bool BQ25672::getInputFets1Placed(){
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

bool BQ25672::getInputFets2Placed(){
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

bool BQ25672::getBatteryHot(){
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

bool BQ25672::getBatteryWarm(){
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

bool BQ25672::getBatteryCool(){
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

bool BQ25672::getBatteryCold(){
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

bool BQ25672::getBatteryUvloForOtg(){
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

bool BQ25672::getInput1Ovp(){
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

bool BQ25672::getInput2Ovp(){
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

bool BQ25672::getConverterOcp(){
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

bool BQ25672::getBatteryCurrentOcp(){
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

bool BQ25672::getBusCurrentOcp(){
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

bool BQ25672::getBatteryVoltageOvp(){
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

bool BQ25672::getBusVoltageOvp(){
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

bool BQ25672::getInBatteryDischargeCurrentRegulation(){
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

bool BQ25672::getInThermalShutdownProtection(){
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

bool BQ25672::getInOtgUnderVoltage(){
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

bool BQ25672::getInOtgOverVoltage(){
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

bool BQ25672::getInSystemOverVoltageProtection(){
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

bool BQ25672::getInSystemShortCircuitProtection(){
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

uint8_t BQ25672::getChargerFlag0(){
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

uint8_t BQ25672::getChargerFlag1(){
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

uint8_t BQ25672::getChargerFlag2(){
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

uint8_t BQ25672::getChargerFlag3(){
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

uint8_t BQ25672::getFaultFlag0(){
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

uint8_t BQ25672::getFaultFlag(){
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

bool BQ25672::getStartAverageWithNewAdcConversion(){
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

bool BQ25672::setStartAverageWithNewAdcConversion(bool new_value){
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

bool BQ25672::getRunningAverageEnabled(){
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

bool BQ25672::setRunningAverageEnabled(bool new_value){
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

int BQ25672::getAdcResolution(){
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

bool BQ25672::setAdcResolution(int new_value){
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

int BQ25672::getAdcConversion(){
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

bool BQ25672::setAdcConversion(int new_value){
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

bool BQ25672::getAdcEnabled(){
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

bool BQ25672::setAdcEnabled(bool new_value){
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

bool BQ25672::getDieTemperatureAdcControlDisabled(){
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

bool BQ25672::setDieTemperatureAdcControlDisabled(bool new_value){
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

bool BQ25672::getNtcAdcControlDisabled(){
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

bool BQ25672::setNtcAdcControlDisabled(bool new_value){
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

bool BQ25672::getSystemVoltageAdcControlDisabled(){
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

bool BQ25672::setSystemVoltageAdcControlDisabled(bool new_value){
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

bool BQ25672::getBatteryVoltageAdcControlDisabled(){
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

bool BQ25672::setBatteryVoltageAdcControlDisabled(bool new_value){
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

bool BQ25672::getBusVoltageAdcControlDisabled(){
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

bool BQ25672::setBusVoltageAdcControlDisabled(bool new_value){
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

bool BQ25672::getBatteryCurrentAdcControlDisabled(){
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

bool BQ25672::setBatteryCurrentAdcControlDisabled(bool new_value){
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

bool BQ25672::getBusCurrentAdcControlDisabled(){
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

bool BQ25672::setBusCurrentAdcControlDisabled(bool new_value){
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

bool BQ25672::getInput1AdcControlDisabled(){
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

bool BQ25672::setInput1AdcControlDisabled(bool new_value){
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

bool BQ25672::getInput2AdcControlDisabled(){
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

bool BQ25672::setInput2AdcControlDisabled(bool new_value){
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

bool BQ25672::getDnAdcControlDisabled(){
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

bool BQ25672::setDnAdcControlDisabled(bool new_value){
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

bool BQ25672::getDpAdcControlDisabled(){
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

bool BQ25672::setDpAdcControlDisabled(bool new_value){
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

int BQ25672::getInputCurrent(){
	// Returns value in: mA

	uint8_t reg = 0x31;
	uint8_t byte_cnt = 2;
	uint8_t bit_start = 0;
	uint8_t bit_end = 15;
	uint16_t offset = 0;
	uint8_t lsb = 1;

	uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
	return ((int16_t) val) * lsb + offset;  // First convert to int16_t as it is a 2'complement number
}

int BQ25672::getBatteryCurrent(){
	// Returns value in: mA

	uint8_t reg = 0x33;
	uint8_t byte_cnt = 2;
	uint8_t bit_start = 0;
	uint8_t bit_end = 15;
	uint16_t offset = 0;
	uint8_t lsb = 1;

	uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
	return ((int16_t) val) * lsb + offset;  // First convert to int16_t as it is a 2'complement number
}

int BQ25672::getBusVoltage(){
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

int BQ25672::getInput1Voltage(){
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

int BQ25672::getInput2Voltage(){
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

int BQ25672::getBatteryVoltage(){
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

int BQ25672::getSystemVoltage(){
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

float BQ25672::getNtcReading(){
	// Returns value in: %

	uint8_t reg = 0x3f;
	uint8_t byte_cnt = 2;
	uint8_t bit_start = 0;
	uint8_t bit_end = 15;
	float offset = 0;
	float lsb = 0.0976563;

	uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
	return val * lsb + offset;
}

float BQ25672::getDieTemperature(){
	// Returns value in: C

	uint8_t reg = 0x41;
	uint8_t byte_cnt = 2;
	uint8_t bit_start = 0;
	uint8_t bit_end = 15;
	float offset = 0;
	float lsb = 0.5;

	uint16_t val  = read_var(reg, byte_cnt, bit_start, bit_end);
	return ((int16_t) val) * lsb + offset;  // First convert to int16_t as it is a 2'complement number
}

int BQ25672::getDpVoltage(){
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

int BQ25672::getDnVoltage(){
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

int BQ25672::getDnOutput(){
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

bool BQ25672::setDnOutput(int new_value){
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

int BQ25672::getDpOutput(){
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

bool BQ25672::setDpOutput(int new_value){
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

int BQ25672::getDeviceRevision(){
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

int BQ25672::getDevicePartNr(){
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

bool BQ25672::readFlags(){
	bool success = true;

	for(int i = 0; i < sizeof(flag_readout)/sizeof(flag_readout[0]); i++){
		success &= read_bytes(flag_registers[i], &flag_readout[i], 1);
	}

	if(_Serial != NULL){
		if(flag_readout[0] & (1 << 0)){
			_Serial->println("BQ25672: Bus voltage present changed");
		}
		if(flag_readout[0] & (1 << 1)){
			_Serial->println("BQ25672: Input 1 present changed");
		}
		if(flag_readout[0] & (1 << 2)){
			_Serial->println("BQ25672: Input 2 present changed");
		}
		if(flag_readout[0] & (1 << 3)){
			_Serial->println("BQ25672: Power good changed");
		}
		if(flag_readout[0] & (1 << 4)){
			_Serial->println("BQ25672: Poor source detected");
		}
		if(flag_readout[0] & (1 << 5)){
			_Serial->println("BQ25672: Watchdog timer passed");
		}
		if(flag_readout[0] & (1 << 6)){
			_Serial->println("BQ25672: VINDPM-VOTG regulation signal detected");
		}
		if(flag_readout[0] & (1 << 7)){
			_Serial->println("BQ25672: IINDPM-IOTG signal detected");
		}

		if(flag_readout[1] & (1 << 0)){
			_Serial->println("BQ25672: BC12 detection status changed");
		}
		if(flag_readout[1] & (1 << 1)){
			_Serial->println("BQ25672: Battery present status changed");
		}
		if(flag_readout[1] & (1 << 2)){
			_Serial->println("BQ25672: Thermal regulation status changed");
		}
		if(flag_readout[1] & (1 << 3)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[1] & (1 << 4)){
			_Serial->println("BQ25672: Bus voltage status changed");
		}
		if(flag_readout[1] & (1 << 5)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[1] & (1 << 6)){
			_Serial->println("BQ25672: ICO status changed");
		}
		if(flag_readout[1] & (1 << 7)){
			_Serial->println("BQ25672: Charge status changed");
		}

		if(flag_readout[2] & (1 << 0)){
			_Serial->println("BQ25672: Top off timer expired");
		}
		if(flag_readout[2] & (1 << 1)){
			_Serial->println("BQ25672: Pre-charge timer expired");
		}
		if(flag_readout[2] & (1 << 2)){
			_Serial->println("BQ25672: Trickle charger timer expired");
		}
		if(flag_readout[2] & (1 << 3)){
			_Serial->println("BQ25672: Fast charge timer expired");
		}
		if(flag_readout[2] & (1 << 4)){
			_Serial->println("BQ25672: Entered or existed VSYSMIN regulation");
		}
		if(flag_readout[2] & (1 << 5)){
			_Serial->println("BQ25672: ADC Conversion completed");
		}
		if(flag_readout[2] & (1 << 6)){
			_Serial->println("BQ25672: D+/D- detection is completed");
		}
		if(flag_readout[2] & (1 << 7)){
			_Serial->println("BQ25672: Reserved");
		}

		if(flag_readout[3] & (1 << 0)){
			_Serial->println("BQ25672: TS across hot temperature (T5) is detected");
		}
		if(flag_readout[3] & (1 << 1)){
			_Serial->println("BQ25672: TS across warm temperature (T3) is detected");
		}
		if(flag_readout[3] & (1 << 2)){
			_Serial->println("BQ25672: TS across cool temperature (T2) is detected");
		}
		if(flag_readout[3] & (1 << 3)){
			_Serial->println("BQ25672: TS across cold temperature (T1) is detected");
		}
		if(flag_readout[3] & (1 << 4)){
			_Serial->println("BQ25672: VBAT falls below the threshold to enable the OTG mode");
		}
		if(flag_readout[3] & (1 << 5)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[3] & (1 << 6)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[3] & (1 << 7)){
			_Serial->println("BQ25672: Reserved");
		}

		if(flag_readout[4] & (1 << 0)){
			_Serial->println("BQ25672: Enter VAC1 OVP");
		}
		if(flag_readout[4] & (1 << 1)){
			_Serial->println("BQ25672: Enter VAC2 OVP");
		}
		if(flag_readout[4] & (1 << 2)){
			_Serial->println("BQ25672: Enter converter OCP");
		}
		if(flag_readout[4] & (1 << 3)){
			_Serial->println("BQ25672: Enter discharged OCP");
		}
		if(flag_readout[4] & (1 << 4)){
			_Serial->println("BQ25672: Enter IBUS OCP");
		}
		if(flag_readout[4] & (1 << 5)){
			_Serial->println("BQ25672: Enter VBAT OVP");
		}
		if(flag_readout[4] & (1 << 6)){
			_Serial->println("BQ25672: Enter VBUS OVP");
		}
		if(flag_readout[4] & (1 << 7)){
			_Serial->println("BQ25672: Enter or exit IBAT regulation");
		}

		if(flag_readout[5] & (1 << 0)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[5] & (1 << 1)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[5] & (1 << 2)){
			_Serial->println("BQ25672: TS shutdown signal rising threshold detected");
		}
		if(flag_readout[5] & (1 << 3)){
			_Serial->println("BQ25672: Reserved");
		}
		if(flag_readout[5] & (1 << 4)){
			_Serial->println("BQ25672: Stop OTG due to VBUS under-voltage");
		}
		if(flag_readout[5] & (1 << 5)){
			_Serial->println("BQ25672: Stop OTG due to VBUS over voltage");
		}
		if(flag_readout[5] & (1 << 6)){
			_Serial->println("BQ25672: Stop switching due to system over-voltage");
		}
		if(flag_readout[5] & (1 << 7)){
			_Serial->println("BQ25672: Stop switching due to system short");
		}

	}
	return success;
}


/* END OF FILE */