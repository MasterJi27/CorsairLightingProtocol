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
#pragma once

#include "Arduino.h"

class PWMFan {
public:
	/**
	 * PWM fan which maps speed to power using linear interpolation. Can also read real RPM values
	 * if a tachometer pin is provided.
	 *
	 * @param pwmPin the Arduino pwm pin for this fan. Not all PWM pins are supported.
	 * @param tachoPin the Arduino pin to read tachometer pulses (optional, 0 = disabled)
	 * @param minRPM the speed in RPM at 0% power
	 * @param maxRPM the speed in RPM at 100% power
	 */
	PWMFan(uint8_t pwmPin, uint8_t tachoPin = 0, uint16_t minRPM = 0, uint16_t maxRPM = 2000);
	virtual void setPower(uint8_t percentage);
	virtual uint8_t calculatePowerFromSpeed(uint16_t rpm) const;
	virtual uint16_t calculateSpeedFromPower(uint8_t power) const;

	uint16_t getSpeed() const;
	void updateTacho();

protected:
	const uint8_t pwmPin;
	const uint8_t tachoPin;
	const uint16_t minRPM;
	const uint16_t maxRPM;

	uint16_t currentRPM;
	uint8_t lastTachoState;
	unsigned long lastTachoTime;
	uint8_t currentPower;
};
