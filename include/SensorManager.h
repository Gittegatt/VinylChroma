#pragma once
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include "AppConfig.h"
#include "LightController.h"
#include "Logger.h"
namespace VinylChroma {
class SensorManager {
 public:
  SensorManager(AppConfig&,LightController&,Logger&);
  void begin();
  bool sample(const DebugOverrides&);
  const std::array<SensorState,MaxSensors>& states()const{return states_;}
  RgbColor weightedColor()const;
  bool hasContributingColorOverride()const;
  bool vinylPresent()const;
  bool readingFresh(size_t)const;
  bool sensorPresent(size_t)const;
  uint32_t normalizedClear(size_t)const;
  bool relativeBrightness(size_t,float&)const;
  bool weightedBrightness(float&)const;
  uint32_t readingAgeMs(size_t)const;
  bool multiplexerDetected()const{return multiplexer_;}
  bool calibrate(String&message);
  void rescan();

 private:
  AppConfig&config_;
  LightController&lights_;
  Logger&logger_;
  std::array<Adafruit_TCS34725,MaxSensors>sensors_;
  std::array<SensorState,MaxSensors>states_{};
  std::array<bool,MaxSensors>driverCreated_{};
  std::array<bool,MaxSensors>initialized_{};
  std::array<bool,MaxSensors>fresh_{};
  std::array<uint32_t,MaxSensors>normalizedClear_{};
  std::array<uint32_t,MaxSensors>lastReadMs_{};
  bool multiplexer_{false};
  uint32_t lastSample_{0};

  bool select(size_t);
  bool ping(uint8_t);
  bool initialize(size_t);
  void invalidateReading(size_t,bool detected=false);
  bool readRaw(size_t,RawColor&);
  SensorExposure desired(size_t,const DebugOverrides&)const;
  bool applyExposure(size_t,const SensorExposure&);
  void autorange(size_t);
  RgbColor process(size_t,const RawColor&,RgbColor& calibratedColor)const;
  uint32_t normalizeClear(uint16_t,const SensorExposure&)const;
  float estimateLux(const RawColor&,const SensorExposure&)const;
  static uint16_t saturationLimit(IntegrationPreset);
  static uint8_t gainMultiplier(SensorGain);
  static uint8_t integrationCode(IntegrationPreset);
  static uint16_t integrationMs(IntegrationPreset);
  static tcs34725Gain_t gainCode(SensorGain);
 };
}
