#pragma once
#include <Preferences.h>
#include <ArduinoJson.h>
#include "AppConfig.h"
namespace VinylChroma {
class ConfigStore {
 public:
  static constexpr uint8_t SchemaVersion=13;
  static constexpr uint8_t DowngradeSourceSchemaVersion=7;
  bool begin();
  bool healthy()const{return healthy_;}
  const String& lastError()const{return lastError_;}
  AppConfig& config(){return config_;}
  const AppConfig& config()const{return config_;}
  bool load();
  bool save();
  String exportJson(bool includePassword=true)const;
  bool importJson(const String&,bool replace=false);
  bool reset();
 private:
  Preferences preferences_;
  AppConfig config_{};
  String lastError_;
  bool opened_{false},healthy_{false};
  void toJson(JsonDocument&,bool)const;
  bool fromJson(JsonVariantConst);
  bool validatePublicRoundTrip();
};
}
