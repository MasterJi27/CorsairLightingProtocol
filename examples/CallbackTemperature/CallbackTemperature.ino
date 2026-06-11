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
#include <CorsairLightingProtocol.h>
#include <FastLED.h>

#define DATA_PIN_CHANNEL_1 2
#define DATA_PIN_CHANNEL_2 3
#define CHANNEL_LED_COUNT 60

CorsairLightingFirmwareStorageEEPROM firmwareStorage;
// Emulate Commander PRO to enable Temperature and Fan tabs in iCUE
CorsairLightingFirmware firmware(CORSAIR_COMMANDER_PRO, &firmwareStorage);

CallbackTemperatureController temperatureController;
FastLEDControllerStorageEEPROM storage;
FastLEDController ledController(&storage);

// Connect everything to the main protocol controller
CorsairLightingProtocolController cLP(&ledController, &temperatureController, nullptr, &firmware);
CorsairLightingProtocolHID cHID(&cLP);

CRGB ledsChannel1[CHANNEL_LED_COUNT];
CRGB ledsChannel2[CHANNEL_LED_COUNT];

void setup() {
	CLP::disableBuildInLEDs();

	FastLED.addLeds<WS2812B, DATA_PIN_CHANNEL_1, GRB>(ledsChannel1, CHANNEL_LED_COUNT);
	FastLED.addLeds<WS2812B, DATA_PIN_CHANNEL_2, GRB>(ledsChannel2, CHANNEL_LED_COUNT);
	ledController.addLEDs(0, ledsChannel1, CHANNEL_LED_COUNT);
	ledController.addLEDs(1, ledsChannel2, CHANNEL_LED_COUNT);

	// Register custom temperature callback
	temperatureController.setTemperatureCallback([](uint8_t sensorIndex) -> uint16_t {
		if (sensorIndex == 0) {
			// Sensor 0: Read internal CPU/Chip Temperature
			return CallbackTemperatureController::readInternalChipTemperature();
		} else if (sensorIndex == 1) {
			// Sensor 1: Read a mock digital sensor (e.g. DS18B20 returning 28.5C)
			// Return temperature in hundredths of a degree Celsius (28.5 * 100 = 2850)
			return 2850;
		}
		return 0;
	});

	// Register custom connection status callback
	temperatureController.setSensorConnectedCallback([](uint8_t sensorIndex) -> bool {
		// Report that Sensor 0 and Sensor 1 are connected, and others are disconnected
		return (sensorIndex == 0 || sensorIndex == 1);
	});

	// Register custom voltage rail callback
	temperatureController.setVoltageCallback([](uint8_t railIndex) -> uint16_t {
		if (railIndex == VOLTAGE_RAIL_5V) {
			// Read the real live VCC voltage of the board (in mV)
			return CallbackTemperatureController::readInternalVcc();
		} else if (railIndex == VOLTAGE_RAIL_12V) {
			return 12000;
		} else if (railIndex == VOLTAGE_RAIL_3V3) {
			return 3300;
		}
		return 0;
	});
}

void loop() {
	cHID.update();

	if (ledController.updateLEDs()) {
		FastLED.show();
	}
}
