#pragma once

#include "AppConfig.h"
#include "Logger.h"
#include "NetworkManager.h"

namespace VinylChroma {
class PlatformioOta {
 public:
  PlatformioOta(AppConfig& config,NetworkManager& network,Logger& logger):config_(config),network_(network),logger_(logger){}
  void begin();
  void loop();
  bool active()const{return active_;}

 private:
  static constexpr uint16_t Port=3232;
  AppConfig& config_;
  NetworkManager& network_;
  Logger& logger_;
  bool configured_{false};
  bool active_{false};
  bool allowed()const;
  bool networkReady()const;
  void start();
  void stop();
};
}
