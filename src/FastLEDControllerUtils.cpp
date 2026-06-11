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
#include "FastLEDControllerUtils.h"

#include <FastLED.h>
#include <math.h>

#include "CLPUtils.h"

void CLP::transformLLFanToStrip(FastLEDController* controller, uint8_t channelIndex) {
	auto& channel = controller->getChannel(channelIndex);
	if (channel.mode == ChannelMode::SoftwarePlayback) {
		auto leds = controller->getLEDs(channelIndex);
		auto count = controller->getLEDCount(channelIndex);
		if (leds == nullptr || count < LL_FAN_LEDS) {
			return;
		}
		for (uint8_t fanIndex = 0; fanIndex < count / LL_FAN_LEDS; fanIndex++) {
			CLP::reverse(&leds[fanIndex * LL_FAN_LEDS], &leds[fanIndex * LL_FAN_LEDS + LL_FAN_LEDS - 1] + 1);
		}
	}
}

void CLP::transformLC100ToStrip(FastLEDController* controller, uint8_t channelIndex) {
	auto& channel = controller->getChannel(channelIndex);
	if (channel.mode == ChannelMode::SoftwarePlayback) {
		auto leds = controller->getLEDs(channelIndex);
		auto count = controller->getLEDCount(channelIndex);
		if (leds == nullptr || count < LC100_LEDS) {
			return;
		}
		for (uint8_t lc100Index = 0; lc100Index < count / LC100_LEDS; lc100Index++) {
			CLP::rotate(&leds[lc100Index * LC100_LEDS], &leds[lc100Index * LC100_LEDS + LC100_FIRST_LED_OFFSET],
						&leds[lc100Index * LC100_LEDS + LC100_LEDS - 1] + 1);  //  End address after the last element
		}
	}
}

/**
 * @param index the index which should be scaled
 * @param scaleFactor the factor for the scaling
 * @return the scaled index
 */
inline int scaleIndex(const int index, const float scaleFactor) { return (int)(index * scaleFactor + 0.5f); }

float scaleFactorOf(const int numberLEDsBefore, const int numberLEDsAfter) {
	if (numberLEDsAfter <= 1 || numberLEDsBefore <= 1) {
		return 0.0f;
	}
	return (float)(numberLEDsBefore - 1) / (numberLEDsAfter - 1);
}

void CLP::scale(FastLEDController* controller, uint8_t channelIndex, int scaleToSize) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr || count == 0 || scaleToSize <= 0) {
		return;
	}
	if (count == 1) {
		for (int ledIndex = 0; ledIndex < scaleToSize; ledIndex++) {
			leds[ledIndex] = leds[0];
		}
		return;
	}
	if (scaleToSize == 1) {
		leds[0] = leds[0];
		return;
	}
	const float scaleFactor = scaleFactorOf(count, scaleToSize);
	if (scaleFactor < 1.0f) {
		for (int ledIndex = scaleToSize - 1; ledIndex >= 0; ledIndex--) {
			leds[ledIndex] = leds[scaleIndex(ledIndex, scaleFactor)];
		}
	} else {
		for (int ledIndex = 0; ledIndex < scaleToSize; ledIndex++) {
			leds[ledIndex] = leds[scaleIndex(ledIndex, scaleFactor)];
		}
	}
}

void CLP::repeat(FastLEDController* controller, uint8_t channelIndex, uint8_t times) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr || count == 0 || times <= 1) {
		return;
	}
	// skip first iteration, because LEDs already contains the data at the first position
	for (int i = 1; i < times; i++) {
		memcpy(leds + (count * i), leds, sizeof(CRGB) * count);
	}
}

void CLP::scaleSegments(FastLEDController* controller, uint8_t channelIndex, const SegmentScaling* const segments,
						int segmentsCount) {
	auto leds = controller->getLEDs(channelIndex);
	int ledStripIndexAfterScaling = 0;
	int ledStripIndexBeforeScaling = 0;
	int totalLengthAfterScaling = 0;
	SegmentScaling downScaledSegments[segmentsCount];
	// scale down segments and move all segments together so there is space for upscaling
	for (int i = 0; i < segmentsCount; i++) {
		const int segmentLength = segments[i].segmentLength;
		const int scaleToSize = min(segments[i].scaleToSize, segmentLength);
		const float scaleFactor = scaleFactorOf(segmentLength, scaleToSize);

		for (int ledIndex = 0; ledIndex < scaleToSize; ledIndex++) {
			leds[ledStripIndexAfterScaling + ledIndex] =
				leds[ledStripIndexBeforeScaling + scaleIndex(ledIndex, scaleFactor)];
		}
		ledStripIndexAfterScaling += scaleToSize;
		ledStripIndexBeforeScaling += segmentLength;
		downScaledSegments[i].segmentLength = scaleToSize;
		downScaledSegments[i].scaleToSize = segments[i].scaleToSize;
		totalLengthAfterScaling += segments[i].scaleToSize;
	}

	ledStripIndexBeforeScaling = ledStripIndexAfterScaling;
	ledStripIndexAfterScaling = totalLengthAfterScaling;
	// scale up segments beginning with the last segment to not override other segments
	for (int i = segmentsCount - 1; i >= 0; i--) {
		const float scaleFactor = scaleFactorOf(downScaledSegments[i].segmentLength, downScaledSegments[i].scaleToSize);
		ledStripIndexAfterScaling -= downScaledSegments[i].scaleToSize;
		ledStripIndexBeforeScaling -= downScaledSegments[i].segmentLength;
		for (int ledIndex = downScaledSegments[i].scaleToSize - 1; ledIndex >= 0; ledIndex--) {
			leds[ledStripIndexAfterScaling + ledIndex] =
				leds[ledStripIndexBeforeScaling + scaleIndex(ledIndex, scaleFactor)];
		}
	}
}

void CLP::reverse(FastLEDController* controller, uint8_t channelIndex) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr || count <= 1) {
		return;
	}
	int maxIndex = count - 1;
	for (int ledIndex = 0; ledIndex < maxIndex - ledIndex; ledIndex++) {
		CRGB temp = leds[ledIndex];
		leds[ledIndex] = leds[maxIndex - ledIndex];
		leds[maxIndex - ledIndex] = temp;
	}
}

void CLP::gammaCorrection(FastLEDController* controller, uint8_t channelIndex) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr) {
		return;
	}
	for (int ledIndex = 0; ledIndex < count; ledIndex++) {
		leds[ledIndex].r = dim8_video(leds[ledIndex].r);
		leds[ledIndex].g = dim8_video(leds[ledIndex].g);
		leds[ledIndex].b = dim8_video(leds[ledIndex].b);
	}
}

void CLP::fixIcueBrightness(FastLEDController* controller, uint8_t channelIndex) {
	auto& channel = controller->getChannel(channelIndex);
	if (channel.mode == ChannelMode::SoftwarePlayback) {
		auto leds = controller->getLEDs(channelIndex);
		auto count = controller->getLEDCount(channelIndex);
		if (leds == nullptr) {
			return;
		}
		for (int ledIndex = 0; ledIndex < count; ledIndex++) {
			uint16_t r = leds[ledIndex].r * 2;
			uint16_t g = leds[ledIndex].g * 2;
			uint16_t b = leds[ledIndex].b * 2;
			leds[ledIndex].r = r > 255 ? 255 : r;
			leds[ledIndex].g = g > 255 ? 255 : g;
			leds[ledIndex].b = b > 255 ? 255 : b;
		}
	}
}

void CLP::applyColorCorrection(FastLEDController* controller, uint8_t channelIndex, const CRGB& correction) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr) {
		return;
	}
	for (int ledIndex = 0; ledIndex < count; ledIndex++) {
		leds[ledIndex].r = (leds[ledIndex].r * correction.r) >> 8;
		leds[ledIndex].g = (leds[ledIndex].g * correction.g) >> 8;
		leds[ledIndex].b = (leds[ledIndex].b * correction.b) >> 8;
	}
}

void CLP::mapToMatrix(FastLEDController* controller, uint8_t channelIndex, uint8_t width, uint8_t height, bool zigzag) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr || count == 0 || width == 0 || height == 0) {
		return;
	}

	const int maxLeds = 256;
	int total = width * height;
	if (total > count) {
		total = count;
	}
	if (total > maxLeds) {
		total = maxLeds;
	}
	CRGB tempLeds[maxLeds];
	memcpy(tempLeds, leds, sizeof(CRGB) * total);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int inputIndex = y * width + x;
			if (inputIndex >= total) break;

			int outputIndex;
			if (zigzag && (y % 2 != 0)) {
				outputIndex = y * width + (width - 1 - x);
			} else {
				outputIndex = y * width + x;
			}

			if (outputIndex < count) {
				leds[outputIndex] = tempLeds[inputIndex];
			}
		}
	}
}

void CLP::enableStandbyAnimation(FastLEDController* controller, uint8_t channelIndex, clp_standby_mode_t mode,
								 const CRGB& color, uint8_t speed) {
#ifndef LED_CONTROLLER_TIMEOUT
#define LED_CONTROLLER_TIMEOUT 30000
#endif

	if (millis() - controller->getLastCommand() > LED_CONTROLLER_TIMEOUT) {
		auto leds = controller->getLEDs(channelIndex);
		auto count = controller->getLEDCount(channelIndex);
		if (leds == nullptr || count == 0) {
			return;
		}

		static uint8_t hue = 0;
		static unsigned long lastTick = 0;
		if (millis() - lastTick >= speed) {
			hue++;
			lastTick = millis();
		}

		switch (mode) {
			case CLP_STANDBY_RAINBOW_WAVE:
				fill_rainbow(leds, count, hue, 256 / count);
				break;
			case CLP_STANDBY_BREATHING: {
				uint8_t brightness = quadwave8(hue);
				for (int i = 0; i < count; i++) {
					leds[i].r = (color.r * brightness) >> 8;
					leds[i].g = (color.g * brightness) >> 8;
					leds[i].b = (color.b * brightness) >> 8;
				}
				break;
			}
			case CLP_STANDBY_SOLID:
				fill_solid(leds, count, color);
				break;
			case CLP_STANDBY_OFF:
			default:
				fill_solid(leds, count, CRGB::Black);
				break;
		}
	}
}

void CLP::applyTemporalSmoothing(FastLEDController* controller, uint8_t channelIndex, uint8_t alpha) {
	auto leds = controller->getLEDs(channelIndex);
	auto count = controller->getLEDCount(channelIndex);
	if (leds == nullptr || count == 0) {
		return;
	}

	const int maxLeds = 96;
	const int maxChannels = 2;
	static CRGB prevLeds[maxChannels][maxLeds];
	static bool initialized[maxChannels] = {false};

	if (channelIndex >= maxChannels) return;

	if (!initialized[channelIndex]) {
		int copyCount = min((int)count, maxLeds);
		memcpy(prevLeds[channelIndex], leds, sizeof(CRGB) * copyCount);
		initialized[channelIndex] = true;
		return;
	}

	for (int ledIndex = 0; ledIndex < count && ledIndex < maxLeds; ledIndex++) {
		uint16_t r = ((uint16_t)alpha * leds[ledIndex].r + (256 - alpha) * prevLeds[channelIndex][ledIndex].r) >> 8;
		uint16_t g = ((uint16_t)alpha * leds[ledIndex].g + (256 - alpha) * prevLeds[channelIndex][ledIndex].g) >> 8;
		uint16_t b = ((uint16_t)alpha * leds[ledIndex].b + (256 - alpha) * prevLeds[channelIndex][ledIndex].b) >> 8;

		leds[ledIndex].r = r;
		leds[ledIndex].g = g;
		leds[ledIndex].b = b;

		prevLeds[channelIndex][ledIndex] = leds[ledIndex];
	}
}
