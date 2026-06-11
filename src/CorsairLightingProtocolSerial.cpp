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
#include "CorsairLightingProtocolSerial.h"

CorsairLightingProtocolSerial::CorsairLightingProtocolSerial(CorsairLightingProtocolController* controller)
	: controller(controller) {}

void CorsairLightingProtocolSerial::setup() {
	Serial.begin(SERIAL_BAUD);
	Serial.setTimeout(SERIAL_TIMEOUT);
}

void CorsairLightingProtocolSerial::update() {
	bool available = handleSerial();
	if (available) {
		Command command;
		memcpy(command.raw, rawCommand, sizeof(command.raw));
		controller->handleCommand(command, this);
	}
}

bool CorsairLightingProtocolSerial::handleSerial() {
	if (Serial.available()) {
		size_t read = Serial.readBytes((char*)rawCommand, sizeof(rawCommand));
		if (read == sizeof(rawCommand)) {
			return true;
		} else {
			while (Serial.available()) {
				Serial.read();
			}
			byte data[] = {PROTOCOL_RESPONSE_ERROR, (byte)read};
			sendX(data, sizeof(data));
		}
	}

	static unsigned long lastSignal = 0;
	unsigned long now = millis();
	if (now - lastSignal >= 2) {
		Serial.write(42);
		Serial.flush();
		lastSignal = now;
	}
	return false;
}

void CorsairLightingProtocolSerial::sendX(const uint8_t* data, const size_t x) const { Serial.write(data, x); }
