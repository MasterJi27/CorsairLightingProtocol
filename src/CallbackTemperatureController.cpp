/*
   Copyright 2026 Raghav Kathuria

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
#include "CallbackTemperatureController.h"

CallbackTemperatureController::CallbackTemperatureController()
	: m_tempCallback(nullptr), m_connectedCallback(nullptr), m_voltageCallback(nullptr) {}

void CallbackTemperatureController::setTemperatureCallback(GetTemperatureCallback callback) {
	m_tempCallback = callback;
}

void CallbackTemperatureController::setSensorConnectedCallback(IsSensorConnectedCallback callback) {
	m_connectedCallback = callback;
}

void CallbackTemperatureController::setVoltageCallback(GetVoltageCallback callback) { m_voltageCallback = callback; }

uint16_t CallbackTemperatureController::readInternalChipTemperature() {
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
	// Read temperature sensor on ATmega32U4 (channel 13, MUX5:0 = 100111)
	// Use 2.56V internal voltage reference (with external cap on AREF)
	uint8_t oldADMUX = ADMUX;
	uint8_t oldADCSRB = ADCSRB;
	uint8_t oldADCSRA = ADCSRA;

	ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0);
	ADCSRB = _BV(MUX5);
	ADCSRA |= _BV(ADEN);  // Enable ADC
	delay(2);

	ADCSRA |= _BV(ADSC);  // Start conversion
	while (bit_is_set(ADCSRA, ADSC))
		;

	uint8_t low = ADCL;
	uint8_t high = ADCH;
	int raw = (high << 8) | low;

	// Restore ADC settings
	ADMUX = oldADMUX;
	ADCSRB = oldADCSRB;
	ADCSRA = oldADCSRA;

	// Approximate formula: Temp = ADC - 273 (value in degrees C)
	// Return in 100ths of C (e.g. 2500 for 25C)
	if (raw < 273) return 0;
	return (raw - 273) * 100;
#elif defined(ARDUINO_ARCH_RP2040)
	// For RP2040 under Arduino-Pico
	float temp = analogReadTemp();
	if (temp < 0.0f || temp > 150.0f) return 3500;  // fallback if invalid
	return (uint16_t)(temp * 100.0f);
#else
	return 2500;  // default 25.00 C
#endif
}

uint16_t CallbackTemperatureController::readInternalVcc() {
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
	uint8_t oldADMUX = ADMUX;
	uint8_t oldADCSRA = ADCSRA;

	// Read 1.1V Reference against AVcc
	ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	ADCSRA |= _BV(ADEN);
	delay(2);

	ADCSRA |= _BV(ADSC);  // Start conversion
	while (bit_is_set(ADCSRA, ADSC))
		;

	uint8_t low = ADCL;
	uint8_t high = ADCH;
	int raw = (high << 8) | low;

	ADMUX = oldADMUX;
	ADCSRA = oldADCSRA;

	if (raw == 0) return 5000;
	// Calculate VCC in millivolts
	return (uint16_t)(1125300L / raw);
#elif defined(ARDUINO_ARCH_RP2040)
// VSYS measurement on Raspberry Pi Pico
#if defined(A3)
	// Pico has VSYS divided by 3 on Pin 29 (A3)
	int raw = analogRead(A3);
	// raw / 1023 * 3.3 * 3 * 1000 = raw * 9.677
	return (uint16_t)(raw * 9.677f);
#else
	return 5000;  // default to 5V rail
#endif
#else
	return 5000;  // default 5000 mV
#endif
}

uint16_t CallbackTemperatureController::getTemperatureValue(uint8_t temperatureSensor) {
	if (m_tempCallback != nullptr) {
		return m_tempCallback(temperatureSensor);
	}
	// Default to reading the chip internal temperature on sensor 0
	if (temperatureSensor == 0) {
		return readInternalChipTemperature();
	}
	return 0;
}

bool CallbackTemperatureController::isTemperatureSensorConnected(uint8_t temperatureSensor) {
	if (m_connectedCallback != nullptr) {
		return m_connectedCallback(temperatureSensor);
	}
	// Sensor 0 (internal CPU sensor) is always connected. Others are disconnected.
	return (temperatureSensor == 0);
}

uint16_t CallbackTemperatureController::getVoltageRail12V() {
	if (m_voltageCallback != nullptr) {
		return m_voltageCallback(VOLTAGE_RAIL_12V);
	}
	return 12000;  // Standard 12V
}

uint16_t CallbackTemperatureController::getVoltageRail5V() {
	if (m_voltageCallback != nullptr) {
		return m_voltageCallback(VOLTAGE_RAIL_5V);
	}
	// Measure actual 5V supply voltage using internal bandgap reference
	return readInternalVcc();
}

uint16_t CallbackTemperatureController::getVoltageRail3V3() {
	if (m_voltageCallback != nullptr) {
		return m_voltageCallback(VOLTAGE_RAIL_3V3);
	}
	return 3300;  // Standard 3.3V
}
