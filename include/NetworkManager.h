#pragma once
#include <WiFi.h>
#include <ESPmDNS.h>
#include "AppConfig.h"
#include "Logger.h"
namespace VinylChroma {
class NetworkManager {
 public:
  NetworkManager(AppConfig&c,Logger&l):config_(c),logger_(l){}
  void begin();
  void loop();
  void reconnect();
  void disconnect(bool erase=false);
  bool apActive()const{return apActive_;}
  String ip()const;
  String scanJson();

 private:
  static constexpr uint32_t ApRetryIntervalMs=30000;
  static constexpr uint32_t MdnsRetryIntervalMs=5000;
  AppConfig&config_;
  Logger&logger_;
  uint32_t disconnectedSince_{0},lastApAttempt_{0},lastMdnsAttempt_{0};
  bool apActive_{false},mdnsActive_{false},disconnectTimerActive_{false},stationWasConnected_{false};
  bool manualDisconnect_{false},apAttempted_{false},mdnsAttempted_{false};
  String activeApSsid_,activeApPassword_;
  void startAp(bool force=false);
  void stopAp(const char* reason=nullptr);
  bool apConfigMatches()const;
  void resetDisconnectTimer();
};
}
