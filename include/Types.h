#pragma once
#include <Arduino.h>
#include <array>
namespace VinylChroma {
inline constexpr size_t MaxSensors=4;
struct RgbColor { uint8_t red{0},green{0},blue{0}; };
struct RawColor { uint16_t red{0},green{0},blue{0},clear{0}; };
enum class SensorGain:uint8_t { Gain1x,Gain4x,Gain16x,Gain60x };
enum class IntegrationPreset:uint8_t { Ms24,Ms50,Ms101,Ms154,Ms300,Ms614 };
enum class SensitivityMode:uint8_t { Automatic,SharedManual,PerSensorManual };
enum class AveragingMode:uint8_t { Instant,RollingAverage,Revolution,LegacyMultipleRevolutions };
enum class SimulationMode:uint8_t { Off,RandomColors,Rainbow,ColorWheel,Custom,Marble };
enum class LogLevel:uint8_t { Off,Error,Warning,Info,Debug,Verbose };
struct SensorExposure { SensorGain gain{SensorGain::Gain16x}; IntegrationPreset integration{IntegrationPreset::Ms154}; };
struct SensorCalibration {
 float red{1},green{1},blue{1};
 uint8_t lightBrightness{100};
 bool valid{false};
 float darkClear{0},whiteClear{0};
 bool brightnessValid{false};
};
struct SensorManualCorrection { float red{1},green{1},blue{1}; };
struct SensorConfig { bool enabled{false}; String name; float weight{1}; SensorExposure exposure{}; SensorCalibration calibration{}; SensorManualCorrection manualCorrection{}; float lightCorrection{1}; };
struct SensorState { bool detected{false},saturated{false},colorOverridden{false}; RawColor raw{}; RgbColor calibratedColor{},color{}; float lux{0},colorTemperature{0}; SensorExposure configuredExposure{},activeExposure{},sampledExposure{}; uint32_t readUs{0}; };
struct DebugOverrides { bool colorEnabled{false}; bool averageOnly{true}; RgbColor averageColor{}; std::array<bool,MaxSensors> sensorColorEnabled{}; std::array<RgbColor,MaxSensors> sensorColors{}; bool exposureEnabled{false}; bool exposureAll{true}; std::array<SensorExposure,MaxSensors> exposures{}; SimulationMode simulation{SimulationMode::Off}; RgbColor simulationColor{}; uint16_t marbleColors{12}; uint8_t marbleStrength{35}; };
}
