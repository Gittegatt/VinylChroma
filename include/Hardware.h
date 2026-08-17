#pragma once
#include <Arduino.h>
#include <array>
namespace VinylChroma::Hardware {
inline constexpr uint8_t I2cSdaPin = 12;
inline constexpr uint8_t I2cSclPin = 13;
inline constexpr uint8_t TcaAddress = 0x70;
inline constexpr uint8_t TcsAddress = 0x29;
inline constexpr std::array<uint8_t,4> SensorLedPins{11,10,9,8};
inline constexpr uint8_t LedPwmResolution = 8;
inline constexpr uint32_t LedPwmFrequency = 5000;
}
