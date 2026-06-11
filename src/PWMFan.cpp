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
#include "PWMFan.h"

PWMFan::PWMFan(uint8_t pwmPin, uint8_t tachoPin, uint16_t minRPM, uint16_t maxRPM)
	: pwmPin(pwmPin),
	  tachoPin(tachoPin),
	  minRPM(minRPM),
	  maxRPM(maxRPM),
	  currentRPM(0),
	  lastTachoState(HIGH),
	  lastTachoTime(0),
	  currentPower(0) {
	pinMode(pwmPin, OUTPUT);
	analogWrite(pwmPin, 0);
	if (tachoPin != 0) {
		pinMode(tachoPin, INPUT_PULLUP);
	}

#if defined(ARDUINO_ARCH_AVR)
	switch (digitalPinToTimer(pwmPin)) {
		case TIMER0B: /* 3 */
#ifdef DEBUG
			Serial.println(F("Pin not supported as PWM fan pin"));
			Serial.println(F("We don't want to mess up Arduino time functions"));
#endif  // DEBUG
			break;
		case TIMER3A: /* 5 */
#if defined(TCCR3B)
			TCCR3B = (TCCR3B & B11111000) | 0x01;
#endif
			break;
		case TIMER4D: /* 6 */
#if defined(TCCR4B)
			TCCR4B = (TCCR4B & B11110000) | 0x01;
#endif
			break;
		case TIMER1A: /* 9 */
#if defined(TCCR1B)
			TCCR1B = (TCCR1B & B11111000) | 0x01;
#endif
			break;
		case TIMER1B: /* 10 */
#if defined(TCCR1B)
			TCCR1B = (TCCR1B & B11111000) | 0x01;
#endif
			break;
		default:
#ifdef DEBUG
			Serial.println(F("Pin not supported as PWM fan pin"));
#endif  // DEBUG
			break;
	}
#endif
}

void PWMFan::setPower(uint8_t percentage) {
	analogWrite(pwmPin, percentage);
	currentPower = percentage;
}

uint8_t PWMFan::calculatePowerFromSpeed(uint16_t rpm) const {
	rpm = constrain(rpm, minRPM, maxRPM);
	return ((float)(rpm - minRPM) / (float)(maxRPM - minRPM)) * 255;
}

uint16_t PWMFan::calculateSpeedFromPower(uint8_t power) const { return map(power, 0, 255, minRPM, maxRPM); }

uint16_t PWMFan::getSpeed() const {
	if (tachoPin == 0) {
		return calculateSpeedFromPower(currentPower);
	}
	return currentRPM;
}

void PWMFan::updateTacho() {
	if (tachoPin == 0) return;

	uint8_t state = digitalRead(tachoPin);
	unsigned long now = micros();

	if (state != lastTachoState) {
		if (state == LOW) {  // falling edge (active low tachometer pulse)
			unsigned long period = now - lastTachoTime;
			if (period > 1000) {                   // debounce: ignore pulses corresponding to > 30,000 RPM
				currentRPM = 30000000UL / period;  // 2 pulses per revolution -> RPM = 60s * 10^6 us / (2 * period)
				lastTachoTime = now;
			}
		}
		lastTachoState = state;
	}

	// Timeout: if no pulses for 1.5 seconds, the fan has stopped
	if (now - lastTachoTime > 1500000UL) {
		currentRPM = 0;
	}
}
