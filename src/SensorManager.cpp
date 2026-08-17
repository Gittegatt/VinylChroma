#include "SensorManager.h"
#include <algorithm>
using namespace VinylChroma;

namespace {
template<typename T,size_t N>
float medianOf(std::array<T,N> values){
 static_assert(N>0,"A median requires at least one sample");
 std::sort(values.begin(),values.end());
 if(N%2)return float(values[N/2]);
 return (float(values[N/2-1])+float(values[N/2]))*0.5F;
}

template<typename T,size_t N>
float medianAbsoluteDeviation(const std::array<T,N>&values,float median){
 std::array<float,N>deviations{};
 for(size_t i=0;i<N;i++)deviations[i]=fabsf(float(values[i])-median);
 return medianOf(deviations);
}
}

SensorManager::SensorManager(AppConfig&c,LightController&l,Logger&g)
 :config_(c),lights_(l),logger_(g),
  sensors_{Adafruit_TCS34725(),Adafruit_TCS34725(),Adafruit_TCS34725(),Adafruit_TCS34725()}{}

bool SensorManager::ping(uint8_t address){
 Wire.beginTransmission(address);
 return Wire.endTransmission()==0;
}

bool SensorManager::select(size_t i){
 if(!multiplexer_)return i==0;
 Wire.beginTransmission(Hardware::TcaAddress);
 Wire.write(1<<config_.hardware.tcaChannels[i]);
 return Wire.endTransmission()==0;
}

void SensorManager::begin(){
 Wire.begin(config_.hardware.i2cSdaPin,config_.hardware.i2cSclPin);
 rescan();
}

void SensorManager::invalidateReading(size_t i,bool detected){
 fresh_[i]=false;
 normalizedClear_[i]=0;
 lastReadMs_[i]=0;
 states_[i].detected=detected;
 states_[i].saturated=false;
 states_[i].colorOverridden=false;
 states_[i].raw={};
 states_[i].calibratedColor={};
 states_[i].color={};
 states_[i].lux=0;
 states_[i].colorTemperature=0;
 states_[i].readUs=0;
}

void SensorManager::rescan(){
 multiplexer_=ping(Hardware::TcaAddress);
 for(size_t i=0;i<MaxSensors;i++){
  initialized_[i]=initialize(i);
  states_[i].detected=initialized_[i];
  fresh_[i]=false;
  normalizedClear_[i]=0;
  lastReadMs_[i]=0;
 }
 logger_.log(LogLevel::Info,String("TCA9548A: ")+(multiplexer_?"detected":"not detected"));
}

bool SensorManager::initialize(size_t i){
 if(!config_.sensors[i].enabled){invalidateReading(i);return false;}
 if(!select(i)||!ping(Hardware::TcsAddress)){invalidateReading(i);return false;}

 bool ok;
 if(driverCreated_[i]){
  // Reinitialize the existing driver. Calling begin() on a newly assigned
  // Adafruit object here would lose its internally allocated I2C device.
  ok=sensors_[i].init();
 }else{
  ok=sensors_[i].begin();
  driverCreated_[i]=true;
 }
 if(!ok){invalidateReading(i);return false;}

 SensorExposure exposure=config_.measurement.sensitivityMode==SensitivityMode::SharedManual
  ?config_.measurement.sharedExposure:config_.sensors[i].exposure;
 sensors_[i].setGain(gainCode(exposure.gain));
 sensors_[i].setIntegrationTime(integrationCode(exposure.integration));
  states_[i].activeExposure=exposure;
  states_[i].configuredExposure=exposure;
  states_[i].sampledExposure=exposure;
  states_[i].detected=true;
  return true;
}

uint8_t SensorManager::integrationCode(IntegrationPreset preset){
 switch(preset){
  case IntegrationPreset::Ms24:return TCS34725_INTEGRATIONTIME_24MS;
  case IntegrationPreset::Ms50:return TCS34725_INTEGRATIONTIME_50MS;
  case IntegrationPreset::Ms101:return TCS34725_INTEGRATIONTIME_101MS;
  case IntegrationPreset::Ms154:return TCS34725_INTEGRATIONTIME_154MS;
  case IntegrationPreset::Ms300:return TCS34725_INTEGRATIONTIME_300MS;
  default:return TCS34725_INTEGRATIONTIME_614MS;
 }
}

uint16_t SensorManager::integrationMs(IntegrationPreset preset){
 static constexpr uint16_t values[]{24,51,101,154,300,615};
 return values[(int)preset];
}

uint16_t SensorManager::saturationLimit(IntegrationPreset preset){
 static constexpr uint16_t values[]{10240,21504,43008,65535,65535,65535};
 return values[(int)preset];
}

uint8_t SensorManager::gainMultiplier(SensorGain gain){
 static constexpr uint8_t values[]{1,4,16,60};
 return values[(int)gain];
}

tcs34725Gain_t SensorManager::gainCode(SensorGain gain){
 switch(gain){
  case SensorGain::Gain1x:return TCS34725_GAIN_1X;
  case SensorGain::Gain4x:return TCS34725_GAIN_4X;
  case SensorGain::Gain16x:return TCS34725_GAIN_16X;
  default:return TCS34725_GAIN_60X;
 }
}

SensorExposure SensorManager::desired(size_t i,const DebugOverrides&o)const{
 if(o.exposureEnabled)return o.exposures[o.exposureAll?0:i];
 if(config_.measurement.sensitivityMode==SensitivityMode::SharedManual)return config_.measurement.sharedExposure;
 return config_.sensors[i].exposure;
}

bool SensorManager::applyExposure(size_t i,const SensorExposure&exposure){
 if(exposure.gain==states_[i].activeExposure.gain&&
    exposure.integration==states_[i].activeExposure.integration)return true;
 if(!select(i)||!ping(Hardware::TcsAddress)){
  invalidateReading(i);
  return false;
 }
 sensors_[i].setGain(gainCode(exposure.gain));
 sensors_[i].setIntegrationTime(integrationCode(exposure.integration));
 states_[i].activeExposure=exposure;
 states_[i].detected=true;
 return true;
}

bool SensorManager::readRaw(size_t i,RawColor&raw){
 if(!select(i))return false;
 // Read C/R/G/B in one checked auto-increment transaction. This keeps every
 // channel in the same conversion and exposes I²C failures to the caller.
 Wire.beginTransmission(Hardware::TcsAddress);
 Wire.write(TCS34725_COMMAND_BIT|0x20|TCS34725_CDATAL);
 if(Wire.endTransmission(false)!=0)return false;
 constexpr uint8_t Bytes=8;
 if(Wire.requestFrom((uint8_t)Hardware::TcsAddress,Bytes)!=Bytes){
  while(Wire.available())Wire.read();
  return false;
 }
 uint8_t data[Bytes];
 for(uint8_t&value:data)value=Wire.read();
 raw.clear=uint16_t(data[0])|(uint16_t(data[1])<<8);
 raw.red=uint16_t(data[2])|(uint16_t(data[3])<<8);
 raw.green=uint16_t(data[4])|(uint16_t(data[5])<<8);
 raw.blue=uint16_t(data[6])|(uint16_t(data[7])<<8);
 return true;
}

uint32_t SensorManager::normalizeClear(uint16_t clear,const SensorExposure&exposure)const{
 // Preserve the existing threshold scale at the factory exposure
 // (16x gain, 154 ms) while making sensors with other exposures comparable.
 constexpr uint32_t ReferenceGain=16;
 constexpr uint32_t ReferenceIntegrationMs=154;
 uint32_t denominator=uint32_t(gainMultiplier(exposure.gain))*integrationMs(exposure.integration);
 if(!denominator)return 0;
 return uint32_t((uint64_t(clear)*ReferenceGain*ReferenceIntegrationMs)/denominator);
}

float SensorManager::estimateLux(const RawColor&raw,const SensorExposure&exposure)const{
 // ams DN40 TCS3472 coefficients. This remains an estimate because the
 // enclosure geometry has no calibrated glass/aperture factor, but unlike
 // Adafruit's raw-count helper it is signed, clamped, and exposure-normalized.
 float infrared=max(0.0F,(float(raw.red)+raw.green+raw.blue-raw.clear)*0.5F);
 float red=max(0.0F,float(raw.red)-infrared);
 float green=max(0.0F,float(raw.green)-infrared);
 float blue=max(0.0F,float(raw.blue)-infrared);
 float weighted=0.136F*red+green-0.444F*blue;
 float exposureScale=float(gainMultiplier(exposure.gain))*integrationMs(exposure.integration);
 if(!isfinite(weighted)||!isfinite(exposureScale)||weighted<=0.0F||exposureScale<=0.0F)return 0.0F;
 float lux=weighted*310.0F/exposureScale;
 return isfinite(lux)&&lux>0.0F?lux:0.0F;
}

void SensorManager::autorange(size_t i){
 SensorExposure exposure=states_[i].activeExposure;
 uint16_t limit=saturationLimit(exposure.integration);
 uint16_t low=uint16_t((uint32_t(config_.measurement.autoLowClear)*limit)/65535UL);
 uint16_t high=uint16_t((uint32_t(config_.measurement.autoHighClear)*limit)/65535UL);
 if(high<=low)high=low<limit?low+1:limit;

 if(states_[i].raw.clear<low&&!states_[i].saturated){
  if((int)exposure.gain<3)exposure.gain=(SensorGain)((int)exposure.gain+1);
  else if((int)exposure.integration<5)exposure.integration=(IntegrationPreset)((int)exposure.integration+1);
 }else if(states_[i].saturated||states_[i].raw.clear>high){
  if((int)exposure.integration>0)exposure.integration=(IntegrationPreset)((int)exposure.integration-1);
  else if((int)exposure.gain>0)exposure.gain=(SensorGain)((int)exposure.gain-1);
 }
 applyExposure(i,exposure);
}

RgbColor SensorManager::process(size_t i,const RawColor&raw,RgbColor&calibratedColor)const{
 if(!raw.clear){calibratedColor={};return{};}
 float scale=255.0F/raw.clear;
 auto&calibration=config_.sensors[i].calibration;
 auto&manual=config_.sensors[i].manualCorrection;
 auto convert=[](float value){return(uint8_t)constrain((int)lroundf(value),0,255);};
 float red=raw.red*scale*(calibration.valid?calibration.red:1.0F);
 float green=raw.green*scale*(calibration.valid?calibration.green:1.0F);
 float blue=raw.blue*scale*(calibration.valid?calibration.blue:1.0F);
 calibratedColor={convert(red),convert(green),convert(blue)};
 // Manual trims operate on the already auto-calibrated 8-bit color. Scaling
 // all adjusted channels back to the original maximum preserves HSV Value and
 // prevents the correction from introducing additional clipping.
 if(manual.red==manual.green&&manual.green==manual.blue)return calibratedColor;
 uint8_t baseMaximum=max(calibratedColor.red,max(calibratedColor.green,calibratedColor.blue));
 if(baseMaximum==0)return{};
 float adjustedRed=calibratedColor.red*manual.red;
 float adjustedGreen=calibratedColor.green*manual.green;
 float adjustedBlue=calibratedColor.blue*manual.blue;
 float adjustedMaximum=max(adjustedRed,max(adjustedGreen,adjustedBlue));
 if(!isfinite(adjustedMaximum)||adjustedMaximum<=0.0F)return calibratedColor;
 float preserveValue=baseMaximum/adjustedMaximum;
 return{
  convert(adjustedRed*preserveValue),
  convert(adjustedGreen*preserveValue),
  convert(adjustedBlue*preserveValue)
 };
}

bool SensorManager::sample(const DebugOverrides&o){
 uint32_t now=millis();
 if(now-lastSample_<config_.measurement.sampleIntervalMs)return false;
 lastSample_=now;
 lights_.apply(true,false);

 std::array<bool,MaxSensors> pending{};
 std::array<bool,MaxSensors> exposureChanged{};
 uint16_t sharedWaitMs=0;
 bool illuminationWasOff=config_.light.enabled&&config_.light.onlyDuringMeasurement;

 // Configure every sensor first. Their conversions then run concurrently
 // while the shared integration wait is in progress.
 for(size_t i=0;i<MaxSensors;i++){
  fresh_[i]=false;
  normalizedClear_[i]=0;
  states_[i].colorOverridden=config_.sensors[i].enabled&&o.sensorColorEnabled[i];

  if(!config_.sensors[i].enabled){
   invalidateReading(i);
   states_[i].colorOverridden=false;
   continue;
  }
   if(states_[i].colorOverridden){
   states_[i].raw={};
    states_[i].calibratedColor=o.sensorColors[i];
    states_[i].color=o.sensorColors[i];
    states_[i].lux=0;
    states_[i].colorTemperature=0;
    states_[i].saturated=false;
    states_[i].readUs=0;
    states_[i].sampledExposure=states_[i].activeExposure;
    fresh_[i]=true;
    continue;
  }
  if(!initialized_[i]){
   invalidateReading(i);
   continue;
  }

  SensorExposure requested=desired(i,o);
  states_[i].configuredExposure=requested;
  bool manual=config_.measurement.sensitivityMode!=SensitivityMode::Automatic||o.exposureEnabled;
  exposureChanged[i]=manual&&
   (requested.gain!=states_[i].activeExposure.gain||
    requested.integration!=states_[i].activeExposure.integration);
  if(manual&&!applyExposure(i,requested))continue;

  pending[i]=true;
  uint16_t integration=integrationMs(states_[i].activeExposure.integration);
  uint32_t elapsed=lastReadMs_[i]?now-lastReadMs_[i]:0;
  uint16_t remaining=elapsed>=integration?0:uint16_t(integration-elapsed);
  if(illuminationWasOff)remaining=uint16_t(integration*2U);
  else if(exposureChanged[i]||!lastReadMs_[i])remaining=integration;
  sharedWaitMs=max(sharedWaitMs,remaining);
 }

 if(sharedWaitMs)delay(sharedWaitMs+2);

 for(size_t i=0;i<MaxSensors;i++){
  if(!pending[i])continue;
  RawColor raw;
  uint32_t started=micros();
  if(!readRaw(i,raw)){
   invalidateReading(i);
   continue;
  }
  states_[i].readUs=micros()-started;
  states_[i].detected=true;
  states_[i].raw=raw;
  SensorExposure sampledExposure=states_[i].activeExposure;
  states_[i].sampledExposure=sampledExposure;
  uint16_t limit=saturationLimit(sampledExposure.integration);
  states_[i].saturated=raw.clear>=limit||raw.red>=limit||raw.green>=limit||raw.blue>=limit;
  states_[i].color=process(i,raw,states_[i].calibratedColor);
  states_[i].lux=states_[i].saturated?0.0F:estimateLux(raw,sampledExposure);
  states_[i].colorTemperature=sensors_[i].calculateColorTemperature_dn40(raw.red,raw.green,raw.blue,raw.clear);
  normalizedClear_[i]=normalizeClear(raw.clear,sampledExposure);
  fresh_[i]=true;
  lastReadMs_[i]=millis();
  if(config_.measurement.sensitivityMode==SensitivityMode::Automatic&&!o.exposureEnabled)autorange(i);
 }

 lights_.apply(false,false);
 return true;
}

RgbColor SensorManager::weightedColor()const{
 float red=0,green=0,blue=0,totalWeight=0;
 for(size_t i=0;i<MaxSensors;i++){
  if(!config_.sensors[i].enabled||!fresh_[i])continue;
  bool present=states_[i].colorOverridden||
   (states_[i].detected&&normalizedClear_[i]>=config_.vinyl.clearThreshold);
  if(config_.vinyl.presenceDetection&&!present)continue;
  float weight=max(0.0F,config_.sensors[i].weight);
  red+=states_[i].color.red*weight;
  green+=states_[i].color.green*weight;
  blue+=states_[i].color.blue*weight;
  totalWeight+=weight;
 }
 if(totalWeight<=0)return{};
 return{
  (uint8_t)constrain((int)lroundf(red/totalWeight),0,255),
  (uint8_t)constrain((int)lroundf(green/totalWeight),0,255),
  (uint8_t)constrain((int)lroundf(blue/totalWeight),0,255)
 };
}

bool SensorManager::hasContributingColorOverride()const{
 for(size_t i=0;i<MaxSensors;i++){
  if(!config_.sensors[i].enabled||!fresh_[i])continue;
  bool present=states_[i].colorOverridden||
   (states_[i].detected&&normalizedClear_[i]>=config_.vinyl.clearThreshold);
  if(config_.vinyl.presenceDetection&&!present)continue;
  if(config_.sensors[i].weight>0&&states_[i].colorOverridden)return true;
 }
 return false;
}

bool SensorManager::vinylPresent()const{
 if(!config_.vinyl.presenceDetection){
   for(size_t i=0;i<MaxSensors;i++)
    if(config_.sensors[i].enabled&&config_.sensors[i].weight>0&&fresh_[i]&&
       (states_[i].detected||states_[i].colorOverridden))return true;
   return false;
 }
 uint8_t count=0;bool contributingSensor=false;
 for(size_t i=0;i<MaxSensors;i++){
  if(!config_.sensors[i].enabled||!fresh_[i])continue;
  if(states_[i].colorOverridden||
     (states_[i].detected&&normalizedClear_[i]>=config_.vinyl.clearThreshold)){
    count++;
    contributingSensor|=config_.sensors[i].weight>0;
   }
 }
 return count>=config_.vinyl.requiredSensors&&contributingSensor;
}

bool SensorManager::readingFresh(size_t i)const{
 if(i>=MaxSensors||!fresh_[i]||!lastReadMs_[i])return false;
 uint32_t integration=integrationMs(states_[i].activeExposure.integration);
 uint32_t conversionWait=config_.light.enabled&&config_.light.onlyDuringMeasurement
  ?integration*2UL:integration;
 uint32_t maximumAge=config_.measurement.sampleIntervalMs+conversionWait+1000UL;
 return millis()-lastReadMs_[i]<=maximumAge;
}

bool SensorManager::sensorPresent(size_t i)const{
 if(i>=MaxSensors||!config_.sensors[i].enabled||!fresh_[i])return false;
 if(states_[i].colorOverridden)return true;
 return states_[i].detected&&
  (!config_.vinyl.presenceDetection||normalizedClear_[i]>=config_.vinyl.clearThreshold);
}

uint32_t SensorManager::normalizedClear(size_t i)const{
 return i<MaxSensors?normalizedClear_[i]:0;
}

bool SensorManager::relativeBrightness(size_t i,float&percent)const{
 percent=0.0F;
 if(i>=MaxSensors||!config_.sensors[i].enabled||!config_.light.enabled||
    !fresh_[i]||!readingFresh(i)||!states_[i].detected||states_[i].saturated||
    states_[i].colorOverridden)return false;
 const SensorCalibration&calibration=config_.sensors[i].calibration;
 if(!calibration.valid||!calibration.brightnessValid||
    calibration.lightBrightness!=config_.light.brightness||
    !isfinite(calibration.darkClear)||!isfinite(calibration.whiteClear)||
    calibration.darkClear<0.0F||
    calibration.whiteClear-calibration.darkClear<1.0F)return false;
 float value=100.0F*(float(normalizedClear_[i])-calibration.darkClear)/
  (calibration.whiteClear-calibration.darkClear);
 if(!isfinite(value))return false;
 percent=constrain(value,0.0F,100.0F);
 return true;
}

bool SensorManager::weightedBrightness(float&percent)const{
 percent=0.0F;
 float weighted=0.0F,totalWeight=0.0F;
 for(size_t i=0;i<MaxSensors;i++){
  if(!config_.sensors[i].enabled||!fresh_[i])continue;
  bool present=states_[i].colorOverridden||
   (states_[i].detected&&normalizedClear_[i]>=config_.vinyl.clearThreshold);
  if(config_.vinyl.presenceDetection&&!present)continue;
  float weight=max(0.0F,config_.sensors[i].weight);
  if(weight<=0.0F)continue;
  // A contributing override intentionally bypasses physical-darkness logic.
  if(states_[i].colorOverridden)return false;
  float sensorBrightness=0.0F;
  // Every physical sensor that contributes to the color must have a valid
  // reference; silently dropping one would change the configured weighting.
  if(!relativeBrightness(i,sensorBrightness))return false;
  weighted+=sensorBrightness*weight;
  totalWeight+=weight;
 }
 if(totalWeight<=0.0F)return false;
 float result=weighted/totalWeight;
 if(!isfinite(result))return false;
 percent=constrain(result,0.0F,100.0F);
 return true;
}

uint32_t SensorManager::readingAgeMs(size_t i)const{
 return i<MaxSensors&&lastReadMs_[i]?millis()-lastReadMs_[i]:UINT32_MAX;
}

bool SensorManager::calibrate(String&message){
 std::array<SensorCalibration,MaxSensors> previousCalibrations{};
 for(size_t i=0;i<MaxSensors;i++)previousCalibrations[i]=config_.sensors[i].calibration;
 auto invalidateAll=[&](){
  for(size_t i=0;i<MaxSensors;i++)invalidateReading(i,initialized_[i]);
  lastSample_=millis();
 };
 auto rollback=[&](){
  for(size_t i=0;i<MaxSensors;i++)config_.sensors[i].calibration=previousCalibrations[i];
  lights_.apply();
  invalidateAll();
 };

 size_t detected=0,calibrated=0;
 for(size_t i=0;i<MaxSensors;i++)
  if(config_.sensors[i].enabled&&initialized_[i])detected++;
 if(!detected){
  lights_.apply();
  message="Calibration failed: no enabled sensors were detected.";
  return false;
 }

 for(size_t i=0;i<MaxSensors;i++){
  if(!config_.sensors[i].enabled||!initialized_[i])continue;
  if(config_.measurement.sensitivityMode!=SensitivityMode::Automatic){
   SensorExposure configured=config_.measurement.sensitivityMode==SensitivityMode::SharedManual
    ?config_.measurement.sharedExposure:config_.sensors[i].exposure;
   states_[i].configuredExposure=configured;
   if(!applyExposure(i,configured)){
    rollback();
    message="Calibration failed: Sensor "+String(i+1)+" exposure could not be configured. Previous calibration was restored.";
    return false;
   }
  }
  lights_.applySingle(i);
  bool exposureReady=false;

  // Automatic mode is tuned against the white reference before factors are
  // calculated. Each attempt waits through two free-running conversions so
  // the accepted sample is guaranteed to have integrated under full light.
  for(uint8_t attempt=0;attempt<12&&!exposureReady;attempt++){
   SensorExposure exposure=states_[i].activeExposure;
   uint16_t integration=integrationMs(exposure.integration);
   uint16_t limit=saturationLimit(exposure.integration);
   delay(integration*2U+2U);
   RawColor raw;
   if(!readRaw(i,raw)){
    rollback();
    message="Calibration failed: Sensor "+String(i+1)+" could not be read. Previous calibration was restored.";
    return false;
   }

   bool saturated=raw.clear>=limit||raw.red>=limit||raw.green>=limit||raw.blue>=limit;
   if(config_.measurement.sensitivityMode==SensitivityMode::Automatic){
    uint16_t low=max<uint16_t>(200,limit/20);
    uint16_t high=uint16_t((uint32_t(limit)*3U)/4U);
    bool changed=false;
    if(saturated||raw.clear>high){
     if((int)exposure.integration>0){exposure.integration=(IntegrationPreset)((int)exposure.integration-1);changed=true;}
     else if((int)exposure.gain>0){exposure.gain=(SensorGain)((int)exposure.gain-1);changed=true;}
    }else if(raw.clear<low){
     if((int)exposure.gain<3){exposure.gain=(SensorGain)((int)exposure.gain+1);changed=true;}
     else if((int)exposure.integration<5){exposure.integration=(IntegrationPreset)((int)exposure.integration+1);changed=true;}
    }
    if(changed){
     if(!applyExposure(i,exposure)){
      rollback();
      message="Calibration failed: Sensor "+String(i+1)+" exposure could not be adjusted. Previous calibration was restored.";
      return false;
     }
     continue;
    }
    if(raw.clear<low){
     rollback();
     message="Calibration failed on Sensor "+String(i+1)+" because the white reference remained too dark at maximum exposure.";
     return false;
    }
   }

   exposureReady=true;
  }

  if(!exposureReady){
   rollback();
   message="Calibration failed: Sensor "+String(i+1)+" exposure could not be stabilized. Previous calibration was restored.";
   return false;
  }

  SensorExposure calibrationExposure=states_[i].activeExposure;
  uint16_t integration=integrationMs(calibrationExposure.integration);
  uint16_t limit=saturationLimit(calibrationExposure.integration);
  uint16_t minimumReference=max<uint16_t>(200,limit/20);
  constexpr size_t CalibrationSamples=7;
  std::array<uint32_t,CalibrationSamples> darkClearSamples{};
  std::array<uint16_t,CalibrationSamples> whiteRedSamples{},whiteGreenSamples{},
   whiteBlueSamples{},whiteRawClearSamples{};
  std::array<uint32_t,CalibrationSamples> whiteClearSamples{};

  // First measure the electronic/ambient baseline with every sensor LED off.
  // Two integrations ensure the first accepted conversion contains no light
  // from the preceding exposure-tuning phase.
  lights_.off();
  delay(integration*2U+2U);
  for(size_t sampleIndex=0;sampleIndex<CalibrationSamples;sampleIndex++){
   if(sampleIndex>0)delay(integration+2U);
   RawColor raw;
   if(!readRaw(i,raw)){
    rollback();
    message="Calibration failed: Sensor "+String(i+1)+" dark reference could not be read. Previous calibration was restored.";
    return false;
   }
   if(raw.clear>=limit||raw.red>=limit||raw.green>=limit||raw.blue>=limit){
    rollback();
    message="Calibration failed on Sensor "+String(i+1)+" because the dark reference was saturated. Block ambient light and try again.";
    return false;
   }
   darkClearSamples[sampleIndex]=normalizeClear(raw.clear,calibrationExposure);
  }

  // Relight only this sensor and again discard two full integrations. The
  // following samples therefore share the same settled exposure and white
  // illumination, independent of ADC phase at the switching instant.
  lights_.applySingle(i);
  delay(integration*2U+2U);
  for(size_t sampleIndex=0;sampleIndex<CalibrationSamples;sampleIndex++){
   if(sampleIndex>0)delay(integration+2U);
   RawColor raw;
   if(!readRaw(i,raw)){
    rollback();
    message="Calibration failed: Sensor "+String(i+1)+" white reference could not be read. Previous calibration was restored.";
    return false;
   }
   if(raw.clear>=limit||raw.red>=limit||raw.green>=limit||raw.blue>=limit){
    rollback();
    message="Calibration failed on Sensor "+String(i+1)+" because the white reference was saturated. Reduce illumination or exposure and try again.";
    return false;
   }
   whiteRedSamples[sampleIndex]=raw.red;
   whiteGreenSamples[sampleIndex]=raw.green;
   whiteBlueSamples[sampleIndex]=raw.blue;
   whiteRawClearSamples[sampleIndex]=raw.clear;
   whiteClearSamples[sampleIndex]=normalizeClear(raw.clear,calibrationExposure);
  }

  float red=medianOf(whiteRedSamples),green=medianOf(whiteGreenSamples),
   blue=medianOf(whiteBlueSamples),clear=medianOf(whiteRawClearSamples);
  if(clear<minimumReference||red<1||green<1||blue<1){
   rollback();
   message="Calibration failed on Sensor "+String(i+1)+" because the white reference was too dark for the selected exposure. Increase illumination or exposure and try again.";
   return false;
  }
  float darkReference=medianOf(darkClearSamples);
  float whiteReference=medianOf(whiteClearSamples);
  float darkMad=medianAbsoluteDeviation(darkClearSamples,darkReference);
  float whiteMad=medianAbsoluteDeviation(whiteClearSamples,whiteReference);
  float contrast=whiteReference-darkReference;
  float minimumContrast=max(50.0F,max(whiteReference*0.05F,6.0F*(darkMad+whiteMad)));
  if(!isfinite(darkReference)||!isfinite(whiteReference)||
     contrast<minimumContrast){
   rollback();
   message="Calibration failed on Sensor "+String(i+1)+" because the dark and white references did not have enough stable contrast. Check the reference surface, ambient light, and sensor illumination.";
   return false;
  }
  float target=(red+green+blue)/3.0F;
  float redFactor=target/red,greenFactor=target/green,blueFactor=target/blue;
  auto validFactor=[](float factor){return isfinite(factor)&&factor>=0.05F&&factor<=20.0F;};
  if(!validFactor(redFactor)||!validFactor(greenFactor)||!validFactor(blueFactor)){
   rollback();
   message="Calibration failed on Sensor "+String(i+1)+" because one color channel was too weak or unbalanced. Check the white reference and sensor alignment.";
   return false;
  }
  config_.sensors[i].calibration.red=redFactor;
  config_.sensors[i].calibration.green=greenFactor;
  config_.sensors[i].calibration.blue=blueFactor;
  config_.sensors[i].calibration.lightBrightness=config_.light.brightness;
  config_.sensors[i].calibration.valid=true;
  config_.sensors[i].calibration.darkClear=darkReference;
  config_.sensors[i].calibration.whiteClear=whiteReference;
  config_.sensors[i].calibration.brightnessValid=true;
  calibrated++;
 }

 lights_.apply();
 invalidateAll();
 message="Calibration completed for "+String(calibrated)+" sensor"+
  (calibrated==1?" with dark and white references.":"s with dark and white references.");
 return true;
}
