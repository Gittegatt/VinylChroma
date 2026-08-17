#pragma once
#include "Types.h"
#include "Version.h"
#include "Hardware.h"
namespace VinylChroma {
struct HardwareConfig { uint8_t i2cSdaPin{Hardware::I2cSdaPin},i2cSclPin{Hardware::I2cSclPin}; std::array<uint8_t,MaxSensors> sensorLedPins{Hardware::SensorLedPins}; std::array<uint8_t,MaxSensors> tcaChannels{0,1,2,3}; };
struct WifiConfig { String ssid,password,hostname{DefaultHostname}; bool fallbackEnabled{true}; uint16_t fallbackDelaySeconds{30}; String fallbackSsid{"VinylChroma"},fallbackPassword{"vinyl!1234"}; };
struct LightConfig { bool enabled{true}; uint8_t brightness{100}; bool activeHigh{true}; bool onlyDuringMeasurement{false}; };
struct MeasurementConfig { uint16_t sampleIntervalMs{250}; float rollingSeconds{10.0F}; AveragingMode averagingMode{AveragingMode::RollingAverage}; float rpm{33.333F}; float revolutions{1.0F}; SensitivityMode sensitivityMode{SensitivityMode::Automatic}; SensorExposure sharedExposure{}; uint16_t autoLowClear{3000}; uint16_t autoHighClear{52000}; };
struct VinylConfig { bool presenceDetection{true}; uint16_t clearThreshold{350}; uint8_t requiredSensors{1}; uint16_t colorChangeThreshold{12}; float colorHoldSeconds{3.0F}; uint8_t outputNormalizationStrength{100}; bool darknessCutoffEnabled{false}; float darknessCutoffPercent{5.0F}; bool defaultEnabled{true}; uint16_t defaultAfterSeconds{20}; RgbColor defaultColor{255,255,255}; bool offEnabled{true}; uint16_t offAfterSeconds{60}; };
struct WledConfig { bool enabled{false}; String host; uint16_t port{80}; uint16_t updateIntervalMs{500}; uint8_t segment{0}; uint8_t brightness{255}; bool sendBrightness{true}; bool keepSelectedEffect{false}; };
struct SystemConfig { bool developerMode{false}; LogLevel logLevel{LogLevel::Info}; bool otaEnabled{true}; bool authenticationEnabled{false}; String authenticationUser{"admin"},authenticationPassword; };
struct AppConfig { HardwareConfig hardware{}; WifiConfig wifi{}; LightConfig light{}; MeasurementConfig measurement{}; VinylConfig vinyl{}; WledConfig wled{}; SystemConfig system{}; std::array<SensorConfig,MaxSensors> sensors{}; };
bool isUsableGpio(uint8_t pin);
bool validateHardwareConfig(const HardwareConfig& c);
void setFactoryDefaults(AppConfig& c);
}
