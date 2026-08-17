#pragma once
#include "AppConfig.h"
namespace VinylChroma { class LightController { public: void begin(AppConfig&); void apply(bool measurement=false,bool calibration=false); void applySingle(size_t sensorIndex); void off(); void setConfig(AppConfig&c){config_=&c;apply();} bool active(size_t i)const{return active_[i];} private: AppConfig*config_{nullptr};std::array<bool,MaxSensors>active_{};void write(size_t,bool);}; }
