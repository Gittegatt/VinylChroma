#pragma once
#include <WebServer.h>
#include <Update.h>
#include "ConfigStore.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "ColorEngine.h"
#include "WledClient.h"
#include "LightController.h"
#include "Logger.h"
namespace VinylChroma {
class WebInterface {
 public:
  WebInterface(ConfigStore&,NetworkManager&,SensorManager&,ColorEngine&,WledClient&,LightController&,Logger&,DebugOverrides&);
  void begin(); void loop(){server_.handleClient();} uint32_t requestCount()const{return requests_;}
  void setPerformance(float cpu,uint32_t loopUs){cpuLoad_=cpu;loopUs_=loopUs;}
 private:
  WebServer server_{80}; ConfigStore&store_; NetworkManager&network_; SensorManager&sensors_; ColorEngine&engine_; WledClient&wled_; LightController&lights_; Logger&logger_; DebugOverrides&overrides_;
  uint32_t requests_{0}; float cpuLoad_{0}; uint32_t loopUs_{0};
  bool otaStarted_{false},otaCompleted_{false},otaDisabledForRequest_{false};
  void count(){requests_++;}
  void commonHeaders();
  bool authorize(bool respond=true);
  bool authorizeMutation(bool respond=true);
  void sendJson(const String&,int=200);
  void syncWled(bool force=false);
  void handleConfigPost(bool replace);
  String statusJson();
  String diagnosticsJson();
  String overridesJson();
  bool applyOverrides(JsonVariantConst);
  static bool parseHex(const String&,RgbColor&);
  static String hex(RgbColor);
  static const char* page();
};
}
