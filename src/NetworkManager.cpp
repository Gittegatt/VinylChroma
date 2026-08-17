#include "NetworkManager.h"
#include <ArduinoJson.h>
using namespace VinylChroma;
void NetworkManager::begin(){
  WiFi.mode(WIFI_AP_STA);WiFi.setHostname(config_.wifi.hostname.c_str());resetDisconnectTimer();
  manualDisconnect_=false;WiFi.setAutoReconnect(true);
  if(!config_.wifi.ssid.isEmpty()){
    WiFi.begin(config_.wifi.ssid.c_str(),config_.wifi.password.c_str());
    logger_.log(LogLevel::Info,"Connecting to "+config_.wifi.ssid);
  }else{
    WiFi.setAutoReconnect(false);
    startAp(true);
    if(!config_.wifi.fallbackEnabled)logger_.log(LogLevel::Error,"No Wi-Fi SSID configured and fallback AP is disabled");
  }
}
void NetworkManager::resetDisconnectTimer(){disconnectedSince_=millis();disconnectTimerActive_=true;}
bool NetworkManager::apConfigMatches()const{return apActive_&&activeApSsid_==config_.wifi.fallbackSsid&&activeApPassword_==config_.wifi.fallbackPassword;}
void NetworkManager::stopAp(const char* reason){
  if(!apActive_)return;
  WiFi.softAPdisconnect(true);apActive_=false;activeApSsid_="";activeApPassword_="";apAttempted_=false;
  if(reason&&*reason)logger_.log(LogLevel::Info,reason);
}
void NetworkManager::startAp(bool force){
  if(!config_.wifi.fallbackEnabled)return;
  if(apConfigMatches())return;
  if(apActive_)stopAp("Restarting fallback AP with updated settings");
  uint32_t now=millis();
  if(!force&&apAttempted_&&now-lastApAttempt_<ApRetryIntervalMs)return;
  apAttempted_=true;lastApAttempt_=now;
  WiFi.mode(WIFI_AP_STA);
  apActive_=WiFi.softAP(config_.wifi.fallbackSsid.c_str(),config_.wifi.fallbackPassword.c_str());
  if(apActive_){
    activeApSsid_=config_.wifi.fallbackSsid;activeApPassword_=config_.wifi.fallbackPassword;
    logger_.log(LogLevel::Warning,"Fallback AP started: "+activeApSsid_+" at "+WiFi.softAPIP().toString());
  }else logger_.log(LogLevel::Error,"Fallback AP start failed for SSID "+config_.wifi.fallbackSsid);
}
void NetworkManager::loop(){
  bool connected=WiFi.status()==WL_CONNECTED;
  if(!config_.wifi.fallbackEnabled&&apActive_)stopAp("Fallback AP disabled");
  if(connected&&manualDisconnect_)return;
  if(connected){
    if(!stationWasConnected_)logger_.log(LogLevel::Info,"Wi-Fi connected: "+WiFi.localIP().toString());
    stationWasConnected_=true;WiFi.setAutoReconnect(true);disconnectTimerActive_=false;
    if(apActive_)stopAp("Fallback AP stopped after Wi-Fi connection");
    uint32_t now=millis();
    if(!mdnsActive_&&(!mdnsAttempted_||now-lastMdnsAttempt_>=MdnsRetryIntervalMs)){
      mdnsAttempted_=true;lastMdnsAttempt_=now;
      if(MDNS.begin(config_.wifi.hostname.c_str())){MDNS.addService("http","tcp",80);mdnsActive_=true;}
    }
    return;
  }
  if(stationWasConnected_){
    stationWasConnected_=false;
    if(mdnsActive_)MDNS.end();
    mdnsActive_=false;mdnsAttempted_=false;resetDisconnectTimer();
    logger_.log(LogLevel::Warning,"Wi-Fi connection lost; automatic reconnect active");
  }else if(!disconnectTimerActive_)resetDisconnectTimer();
  if(apActive_&&!apConfigMatches())startAp(true);
  bool noStation=config_.wifi.ssid.isEmpty()||manualDisconnect_;
  if(config_.wifi.fallbackEnabled&&(noStation||millis()-disconnectedSince_>=config_.wifi.fallbackDelaySeconds*1000UL))startAp();
}
void NetworkManager::reconnect(){
  if(mdnsActive_)MDNS.end();mdnsActive_=false;mdnsAttempted_=false;
  if(!config_.wifi.fallbackEnabled&&apActive_)stopAp("Fallback AP disabled");
  else if(apActive_&&!apConfigMatches())startAp(true);
  WiFi.setAutoReconnect(false);WiFi.disconnect(false,false);delay(100);
  WiFi.mode(WIFI_AP_STA);WiFi.setHostname(config_.wifi.hostname.c_str());
  stationWasConnected_=false;manualDisconnect_=false;resetDisconnectTimer();
  if(!config_.wifi.ssid.isEmpty()){
    WiFi.setAutoReconnect(true);WiFi.begin(config_.wifi.ssid.c_str(),config_.wifi.password.c_str());
    logger_.log(LogLevel::Info,"Connecting to "+config_.wifi.ssid);
  }else{
    WiFi.setAutoReconnect(false);startAp(true);
    if(!config_.wifi.fallbackEnabled)logger_.log(LogLevel::Error,"No Wi-Fi SSID configured and fallback AP is disabled");
  }
}
void NetworkManager::disconnect(bool erase){
  if(mdnsActive_)MDNS.end();mdnsActive_=false;mdnsAttempted_=false;
  WiFi.setAutoReconnect(false);
  bool accepted=WiFi.disconnect(false,erase);
  if(!accepted)logger_.log(LogLevel::Warning,"Wi-Fi disconnect request failed");
  if(erase){config_.wifi.ssid="";config_.wifi.password="";}
  manualDisconnect_=accepted||WiFi.status()!=WL_CONNECTED;
  stationWasConnected_=WiFi.status()==WL_CONNECTED&&!manualDisconnect_;
  resetDisconnectTimer();
  if(manualDisconnect_)startAp(true);
}
String NetworkManager::ip()const{return WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString();}
String NetworkManager::scanJson(){
 JsonDocument d;
 int16_t result=WiFi.scanComplete();
 if(result==WIFI_SCAN_RUNNING){
  d["running"]=true;
 }else if(result<0){
  WiFi.scanDelete();
  int16_t started=WiFi.scanNetworks(true,true);
  d["running"]=started==WIFI_SCAN_RUNNING;
  if(started!=WIFI_SCAN_RUNNING)d["error"]="scan could not be started";
 }else{
  d["running"]=false;
  auto networks=d["networks"].to<JsonArray>();
  for(int i=0;i<result;i++){
   auto item=networks.add<JsonObject>();
   item["ssid"]=WiFi.SSID(i);
   item["rssi"]=WiFi.RSSI(i);
   item["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
 }
 String json;serializeJson(d,json);return json;
}
