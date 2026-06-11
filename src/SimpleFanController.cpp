/*
   Copyright 2019 Raghav Kathuria

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "SimpleFanController.h"

#if defined(ARDUINO_ARCH_AVR)
#include <EEPROM.h>
#endif

SimpleFanController::SimpleFanController(TemperatureController* temperatureController, uint16_t updateRate,
										 uint16_t eEPROMAdress)
	: temperatureController(temperatureController), updateRate(updateRate), eEPROMAdress(eEPROMAdress) {
	load();
}

void SimpleFanController::handleFanControl(const Command& command, const CorsairLightingProtocolResponse* response) {
	lastFanCommand = millis();
	FanController::handleFanControl(command, response);
}

void SimpleFanController::setStandbyFanCurve(uint8_t fan, FanCurve& fanCurve, uint8_t group) {
	if (fan >= FAN_NUM) {
		return;
	}
	standbyFanCurves[fan] = fanCurve;
	standbyTempGroups[fan] = group;
	standbyEnabled[fan] = true;
}

void SimpleFanController::addFan(uint8_t index, PWMFan* fan) {
	if (index >= FAN_NUM) {
		return;
	}
	fans[index] = fan;
	switch (fanData[index].mode) {
		case FAN_CONTROL_MODE_FIXED_POWER:
			fanData[index].speed = fan->calculateSpeedFromPower(fanData[index].power);
			break;
		case FAN_CONTROL_MODE_FIXED_RPM:
			fanData[index].power = fan->calculatePowerFromSpeed(fanData[index].speed);
			break;
	}
}

bool SimpleFanController::updateFans() {
	unsigned long currentUpdate = millis();
	unsigned long lastUpdateNumber = lastUpdate / updateRate;
	unsigned long currentUpdateNumber = currentUpdate / updateRate;
	lastUpdate = currentUpdate;
	if (lastUpdateNumber < currentUpdateNumber) {
		if (triggerSave) {
			triggerSave = false;
			save();
		}

		bool isOffline = (millis() - lastFanCommand) > 30000;

		for (uint8_t i = 0; i < FAN_NUM; i++) {
			if (fans[i] == nullptr) {
				continue;
			}

			uint8_t currentMode = fanData[i].mode;
			uint8_t currentGroup = fanData[i].tempGroup;
			const FanCurve* currentFanCurve = &fanData[i].fanCurve;

			if (isOffline && standbyEnabled[i]) {
				currentMode = FAN_CONTROL_MODE_CURVE;
				currentGroup = standbyTempGroups[i];
				currentFanCurve = &standbyFanCurves[i];
			}

			if (currentMode == FAN_CONTROL_MODE_FIXED_RPM || currentMode == FAN_CONTROL_MODE_FIXED_POWER) {
				fans[i]->setPower(fanData[i].power);
				continue;
			}

			uint16_t temp = 0;
			if (currentGroup == FAN_CURVE_TEMP_GROUP_EXTERNAL) {
				temp = externalTemp[i];
			} else if (currentGroup < TEMPERATURE_NUM) {
				temp = temperatureController->getTemperature(currentGroup);
			}

			uint16_t speed;

			if (temp <= currentFanCurve->temperatures[0]) {
				speed = currentFanCurve->rpms[0];
			} else if (temp > currentFanCurve->temperatures[FAN_CURVE_POINTS_NUM - 1]) {
				speed = currentFanCurve->rpms[FAN_CURVE_POINTS_NUM - 1];
			} else {
				for (uint8_t p = 0; p < FAN_CURVE_POINTS_NUM - 1; p++) {
					if (temp > currentFanCurve->temperatures[p + 1]) {
						continue;
					}
					speed = map(temp, currentFanCurve->temperatures[p], currentFanCurve->temperatures[p + 1],
								currentFanCurve->rpms[p], currentFanCurve->rpms[p + 1]);
					break;
				}
			}

			fanData[i].speed = speed;
			fanData[i].power = fans[i]->calculatePowerFromSpeed(speed);
			fans[i]->setPower(fanData[i].power);
		}
		return true;
	}
	return false;
}

uint16_t SimpleFanController::getFanSpeed(uint8_t fan) {
	if (fans[fan] != nullptr) {
		return fans[fan]->getSpeed();
	}
	return fanData[fan].speed;
}

void SimpleFanController::update() {
	for (uint8_t i = 0; i < FAN_NUM; i++) {
		if (fans[i] != nullptr) {
			fans[i]->updateTacho();
		}
	}
}

void SimpleFanController::setFanSpeed(uint8_t fan, uint16_t speed) {
	fanData[fan].speed = speed;
	fanData[fan].mode = FAN_CONTROL_MODE_FIXED_RPM;
	fanData[fan].power = fans[fan] != nullptr ? fans[fan]->calculatePowerFromSpeed(speed) : 0;
	triggerSave = true;
}

uint8_t SimpleFanController::getFanPower(uint8_t fan) { return fanData[fan].power; }

void SimpleFanController::setFanPower(uint8_t fan, uint8_t percentage) {
	fanData[fan].power = percentage;
	fanData[fan].mode = FAN_CONTROL_MODE_FIXED_POWER;
	fanData[fan].speed = fans[fan] != nullptr ? fans[fan]->calculateSpeedFromPower(percentage) : 0;
	triggerSave = true;
}

void SimpleFanController::setFanCurve(uint8_t fan, uint8_t group, FanCurve& fanCurve) {
	fanData[fan].fanCurve = fanCurve;
	fanData[fan].tempGroup = group;
	fanData[fan].mode = FAN_CONTROL_MODE_CURVE;
	triggerSave = true;
}

void SimpleFanController::setFanExternalTemperature(uint8_t fan, uint16_t temp) { externalTemp[fan] = temp; }

void SimpleFanController::setFanForce3PinMode(bool flag) { force3PinMode = flag; }

FanDetectionType SimpleFanController::getFanDetectionType(uint8_t fan) { return fanData[fan].detectionType; }

void SimpleFanController::setFanDetectionType(uint8_t fan, FanDetectionType type) {
	if (fanData[fan].detectionType != type) {
		fanData[fan].detectionType = type;
		triggerSave = true;
	}
}

bool SimpleFanController::load() {
#if defined(ARDUINO_ARCH_AVR)
	EEPROM.get(eEPROMAdress, fanData);
#endif
	return true;
}

bool SimpleFanController::save() {
#ifdef DEBUG
	Serial.println(F("Save fan data to EEPROM."));
#endif
#if defined(ARDUINO_ARCH_AVR)
	EEPROM.put(eEPROMAdress, fanData);
#endif
	return true;
}
