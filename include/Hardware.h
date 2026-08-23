#pragma once
#include <Arduino.h>
#include <array>
#include "BoardProfile.h"
namespace VinylChroma::Hardware {
inline constexpr uint8_t I2cSdaPin = BoardProfile::I2cSdaPin;
inline constexpr uint8_t I2cSclPin = BoardProfile::I2cSclPin;
inline constexpr uint8_t TcaAddress = 0x70;
inline constexpr uint8_t TcsAddress = 0x29;
inline constexpr std::array<uint8_t,4> SensorLedPins{BoardProfile::SensorLedPins};
inline constexpr uint8_t LedPwmResolution = 8;
inline constexpr uint32_t LedPwmFrequency = 5000;
}
