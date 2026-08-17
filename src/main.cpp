#include <Arduino.h>
#include "ConfigStore.h"
#include "Logger.h"
#include "NetworkManager.h"
#include "LightController.h"
#include "SensorManager.h"
#include "ColorEngine.h"
#include "WledClient.h"
#include "WebInterface.h"
using namespace VinylChroma;
ConfigStore store; Logger logger; DebugOverrides overrides;
LightController lights; NetworkManager* network=nullptr; SensorManager* sensors=nullptr; ColorEngine* engine=nullptr; WledClient* wled=nullptr; WebInterface* web=nullptr;
uint32_t busyWindowStart=0,busyMicros=0;
void syncWled(){
 constexpr uint16_t MarbleUpdateIntervalMs=100;
 if(engine->outputOn())wled->update(
  engine->output(),
  true,
  false,
  false,
  overrides.simulation==SimulationMode::Marble?MarbleUpdateIntervalMs:0
 );
 else if(engine->automaticOffActive()||engine->darknessCutoffActive())wled->update(engine->output(),false,false,true);
 else wled->cancelRelease();
}
void setup(){
 Serial.begin(115200); delay(250); bool configReady=store.begin(); logger.setLevel(store.config().system.logLevel); logger.log(LogLevel::Info,String(FirmwareName)+" "+FirmwareVersion);
 if(!configReady)logger.log(LogLevel::Error,"Configuration storage is unavailable or invalid; safe factory defaults are active until a valid save or reset");
 lights.begin(store.config()); network=new NetworkManager(store.config(),logger); sensors=new SensorManager(store.config(),lights,logger); engine=new ColorEngine(store.config()); wled=new WledClient(store.config(),logger); web=new WebInterface(store,*network,*sensors,*engine,*wled,lights,logger,overrides);
 sensors->begin(); network->begin(); web->begin(); busyWindowStart=millis();
}
void loop(){
 uint32_t start=micros(); network->loop(); web->loop();
 static uint32_t lastDebugUpdate=0;
 bool directDebugOutput=overrides.simulation!=SimulationMode::Off||
  (overrides.colorEnabled&&overrides.averageOnly);
 if(directDebugOutput){
  if(millis()-lastDebugUpdate>=25){lastDebugUpdate=millis();engine->update({},true,overrides,false);syncWled();}
 }else if(sensors->sample(overrides)){
  bool colorOverridden=sensors->hasContributingColorOverride();
  float brightnessPercent=0;
  bool brightnessAvailable=!colorOverridden&&sensors->weightedBrightness(brightnessPercent);
  engine->update(
   sensors->weightedColor(),
   sensors->vinylPresent(),
   overrides,
   !colorOverridden,
   brightnessAvailable,
   brightnessPercent,
   colorOverridden
  );
  syncWled();
 }
 uint32_t used=micros()-start; busyMicros+=used;
 uint32_t elapsedMs=millis()-busyWindowStart;
 if(elapsedMs>=1000){float cpu=min(100.0F,busyMicros/(elapsedMs*10.0F));web->setPerformance(cpu,used);busyMicros=0;busyWindowStart=millis();}
 delay(1);
}
