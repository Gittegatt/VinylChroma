#include "PlatformioOta.h"
#include <ArduinoOTA.h>

using namespace VinylChroma;

bool PlatformioOta::allowed()const{
 return config_.system.otaEnabled&&config_.system.platformioOtaEnabled;
}

bool PlatformioOta::networkReady()const{
 bool stationReady=WiFi.status()==WL_CONNECTED&&static_cast<uint32_t>(WiFi.localIP())!=0;
 bool accessPointReady=network_.apActive()&&static_cast<uint32_t>(WiFi.softAPIP())!=0;
 return stationReady||accessPointReady;
}

void PlatformioOta::begin(){
 if(!configured_){
  ArduinoOTA.setPort(Port);
  // NetworkManager owns the shared mDNS responder. Direct-IP PlatformIO uploads
  // do not need ArduinoOTA to start or stop a second mDNS instance.
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([this](){logger_.log(LogLevel::Warning,"PlatformIO OTA firmware upload started");});
  ArduinoOTA.onEnd([this](){logger_.log(LogLevel::Info,"PlatformIO OTA firmware upload completed; rebooting");});
  ArduinoOTA.onError([this](ota_error_t error){
   logger_.log(LogLevel::Error,"PlatformIO OTA firmware upload failed (error "+String((int)error)+")");
  });
  configured_=true;
 }
 if(allowed()&&networkReady())start();
}

void PlatformioOta::start(){
 if(active_)return;
 ArduinoOTA.begin();
 active_=true;
 logger_.log(LogLevel::Warning,"PlatformIO OTA enabled on port "+String(Port)+" without protocol authentication; use only on a trusted network");
}

void PlatformioOta::stop(){
 if(!active_)return;
 ArduinoOTA.end();
 active_=false;
 logger_.log(LogLevel::Info,"PlatformIO OTA disabled");
}

void PlatformioOta::loop(){
 if(allowed()&&networkReady()){
  if(!active_)start();
  ArduinoOTA.handle();
 }else stop();
}
