#include "ConfigStore.h"
#include <ctype.h>
#include <math.h>
using namespace VinylChroma;

namespace {
constexpr float MinimumBrightnessReferenceGap=1.0F;

bool isNumber(JsonVariantConst value){
 return !value.isNull()&&!value.is<bool>()&&!value.is<const char*>()&&
  !value.is<JsonArrayConst>()&&!value.is<JsonObjectConst>();
}
bool readInteger(JsonVariantConst value,long minimum,long maximum,long&out){
 if(value.isNull())return true;
 if(!isNumber(value))return false;
 double number=value.as<double>();
 if(!isfinite(number)||floor(number)!=number||number<minimum||number>maximum)return false;
 out=(long)number;return true;
}
bool readFloat(JsonVariantConst value,float minimum,float maximum,float&out){
 if(value.isNull())return true;
 if(!isNumber(value))return false;
 double number=value.as<double>();
 if(!isfinite(number)||number<minimum||number>maximum)return false;
 out=(float)number;return true;
}
bool readString(JsonVariantConst value,String&out,size_t maximum,bool allowEmpty=true){
 if(value.isNull())return true;
 if(!value.is<const char*>())return false;
 String candidate=value.as<String>();
 if(candidate.length()>maximum||(!allowEmpty&&candidate.isEmpty()))return false;
 out=candidate;return true;
}
bool readBool(JsonVariantConst value,bool&out){
 if(value.isNull())return true;
 if(!value.is<bool>())return false;
 out=value.as<bool>();return true;
}
bool optionalObject(JsonVariantConst value){
 return value.isNull()||value.is<JsonObjectConst>();
}
bool validHostname(const String&hostname){
 if(hostname.isEmpty()||hostname.length()>63||hostname[0]=='-'||hostname[hostname.length()-1]=='-')return false;
 for(char c:hostname)if(!(isalnum((unsigned char)c)||c=='-'))return false;
 return true;
}
bool validWledHost(const String&host){
 if(host.isEmpty()||host.length()>253||host[0]=='.'||host[host.length()-1]=='.'||host[0]=='-'||host[host.length()-1]=='-')return false;
 char previous=0;size_t labelLength=0;
 for(char c:host){
  if(!(isalnum((unsigned char)c)||c=='.'||c=='-'))return false;
  if(c=='.'&&(previous==0||previous=='.'||previous=='-'))return false;
  if(previous=='.'&&c=='-')return false;
  if(c=='.')labelLength=0;
  else if(++labelLength>63)return false;
  previous=c;
 }
 return true;
}
bool readExposure(JsonVariantConst value,SensorExposure&exposure){
 if(value.isNull())return true;
 if(!value.is<JsonObjectConst>())return false;
 long gain=(long)exposure.gain,integration=(long)exposure.integration;
 if(!readInteger(value["gain"],0,3,gain)||!readInteger(value["integration"],0,5,integration))return false;
 exposure.gain=(SensorGain)gain;exposure.integration=(IntegrationPreset)integration;return true;
}
String migratedHostname(String value){
 value.trim();
 for(size_t i=0;i<value.length();i++)
  if(!(isalnum((unsigned char)value[i])||value[i]=='-'))value.setCharAt(i,'-');
 while(value.startsWith("-"))value.remove(0,1);
 while(value.endsWith("-"))value.remove(value.length()-1);
 if(value.length()>63)value.remove(63);
 while(value.endsWith("-"))value.remove(value.length()-1);
 return value.isEmpty()?String(DefaultHostname):value;
}
void resetBoardSpecificSettings(JsonDocument&document,const AppConfig&defaults){
 JsonObject hardware=document["hardware"].is<JsonObject>()
  ?document["hardware"].as<JsonObject>()
  :document["hardware"].to<JsonObject>();
 hardware["boardProfile"]=BoardProfile::Id;
 hardware["i2cSdaPin"]=defaults.hardware.i2cSdaPin;
 hardware["i2cSclPin"]=defaults.hardware.i2cSclPin;
 hardware.remove("sensorLedPins");
 auto pins=hardware["sensorLedPins"].to<JsonArray>();
 for(uint8_t pin:defaults.hardware.sensorLedPins)pins.add(pin);
 hardware.remove("tcaChannels");
 auto channels=hardware["tcaChannels"].to<JsonArray>();
 for(uint8_t channel:defaults.hardware.tcaChannels)channels.add(channel);

 auto sensors=document["sensors"];
 if(!sensors.is<JsonArray>()||sensors.size()!=MaxSensors)return;
 for(size_t i=0;i<MaxSensors;i++){
  JsonObject sensor=sensors[i];
  if(sensor.isNull())continue;
  JsonObject calibration=sensor["calibration"].is<JsonObject>()
   ?sensor["calibration"].as<JsonObject>()
   :sensor["calibration"].to<JsonObject>();
  calibration["red"]=defaults.sensors[i].calibration.red;
  calibration["green"]=defaults.sensors[i].calibration.green;
  calibration["blue"]=defaults.sensors[i].calibration.blue;
  calibration["lightBrightness"]=defaults.sensors[i].calibration.lightBrightness;
  calibration["valid"]=false;
  calibration["darkClear"]=0;
  calibration["whiteClear"]=0;
  calibration["brightnessValid"]=false;
  JsonObject manual=sensor["manualCorrection"].is<JsonObject>()
   ?sensor["manualCorrection"].as<JsonObject>()
   :sensor["manualCorrection"].to<JsonObject>();
  manual["red"]=defaults.sensors[i].manualCorrection.red;
  manual["green"]=defaults.sensors[i].manualCorrection.green;
  manual["blue"]=defaults.sensors[i].manualCorrection.blue;
 }
}
void migrateLegacy(JsonDocument&document,long schema){
 AppConfig defaults;
 setFactoryDefaults(defaults);
 auto hardware=document["hardware"];
 bool profileMissing=!hardware.is<JsonObject>()||hardware["boardProfile"].isNull();
 String storedProfile=profileMissing?String():hardware["boardProfile"].as<String>();
 bool profileMismatch=!profileMissing&&storedProfile!=BoardProfile::Id;
 // Schema 12 and earlier were released only for the S3 Super Mini. Preserve
 // its custom mapping on that board, but never transplant those pins to a
 // different compiled profile.
 if(profileMissing&&schema<13&&String(BoardProfile::Id)!="esp32-s3-supermini")profileMismatch=true;
 if(profileMismatch)resetBoardSpecificSettings(document,defaults);
 else{
  if(!hardware.is<JsonObject>())hardware=document["hardware"].to<JsonObject>();
  hardware["boardProfile"]=BoardProfile::Id;
 }
 if(schema>=ConfigStore::SchemaVersion){document["schemaVersion"]=ConfigStore::SchemaVersion;return;}
 hardware=document["hardware"];
 if(hardware.is<JsonObject>()){
  hardware["boardProfile"]=BoardProfile::Id;
  if(hardware["i2cSdaPin"].isNull())hardware["i2cSdaPin"]=defaults.hardware.i2cSdaPin;
  if(hardware["i2cSclPin"].isNull())hardware["i2cSclPin"]=defaults.hardware.i2cSclPin;
  if(hardware["sensorLedPins"].isNull()){
   auto pins=hardware["sensorLedPins"].to<JsonArray>();
   for(auto pin:defaults.hardware.sensorLedPins)pins.add(pin);
  }
  if(hardware["tcaChannels"].isNull()){
   auto channels=hardware["tcaChannels"].to<JsonArray>();
   for(auto channel:defaults.hardware.tcaChannels)channels.add(channel);
  }
 }
 auto wifi=document["wifi"];
 if(wifi.is<JsonObject>()){
  if(wifi["ssid"].isNull())wifi["ssid"]=defaults.wifi.ssid;
  if(wifi["password"].isNull())wifi["password"]=defaults.wifi.password;
  if(wifi["hostname"].isNull())wifi["hostname"]=defaults.wifi.hostname;
  if(wifi["fallbackEnabled"].isNull())wifi["fallbackEnabled"]=defaults.wifi.fallbackEnabled;
  if(wifi["fallbackDelaySeconds"].isNull())wifi["fallbackDelaySeconds"]=defaults.wifi.fallbackDelaySeconds;
  if(wifi["fallbackSsid"].isNull())wifi["fallbackSsid"]=defaults.wifi.fallbackSsid;
  if(wifi["fallbackPassword"].isNull())wifi["fallbackPassword"]=defaults.wifi.fallbackPassword;
  if(wifi["hostname"].is<const char*>())wifi["hostname"]=migratedHostname(wifi["hostname"].as<String>());
  if(wifi["fallbackSsid"].is<const char*>()&&wifi["fallbackSsid"].as<String>()=="VinylChroma-Setup")wifi["fallbackSsid"]="VinylChroma";
  if(wifi["fallbackPassword"].is<const char*>()&&wifi["fallbackPassword"].as<String>()=="vinylchroma")wifi["fallbackPassword"]="vinyl!1234";
  bool stationPasswordReset=false;
  if(wifi["password"].is<const char*>()){
   String password=wifi["password"].as<String>();
   if(!password.isEmpty()&&password.length()<8){wifi["password"]="";stationPasswordReset=true;}
  }
  if(wifi["fallbackPassword"].is<const char*>()){
   String password=wifi["fallbackPassword"].as<String>();
   if(password.length()<8||password.length()>63)wifi["fallbackPassword"]="vinyl!1234";
  }
  String ssid=wifi["ssid"].is<const char*>()?wifi["ssid"].as<String>():String();
  bool fallback=wifi["fallbackEnabled"].is<bool>()?wifi["fallbackEnabled"].as<bool>():true;
  if((ssid.isEmpty()||stationPasswordReset)&&!fallback)wifi["fallbackEnabled"]=true;
 }
 auto light=document["light"];
 if(light.is<JsonObject>()){
  if(light["enabled"].isNull())light["enabled"]=defaults.light.enabled;
  if(light["brightness"].isNull())light["brightness"]=defaults.light.brightness;
  if(light["activeHigh"].isNull())light["activeHigh"]=defaults.light.activeHigh;
  if(light["onlyDuringMeasurement"].isNull())light["onlyDuringMeasurement"]=defaults.light.onlyDuringMeasurement;
 }
 auto measurement=document["measurement"];
 if(measurement.is<JsonObject>()){
  if(measurement["sampleIntervalMs"].isNull())measurement["sampleIntervalMs"]=defaults.measurement.sampleIntervalMs;
  if(measurement["rollingSeconds"].isNull())measurement["rollingSeconds"]=defaults.measurement.rollingSeconds;
  if(measurement["averagingMode"].isNull())measurement["averagingMode"]=(int)defaults.measurement.averagingMode;
  if(measurement["rpm"].isNull())measurement["rpm"]=defaults.measurement.rpm;
  if(measurement["revolutions"].isNull())measurement["revolutions"]=defaults.measurement.revolutions;
  if(measurement["sensitivityMode"].isNull())measurement["sensitivityMode"]=(int)defaults.measurement.sensitivityMode;
  if(measurement["autoLowClear"].isNull())measurement["autoLowClear"]=defaults.measurement.autoLowClear;
  if(measurement["autoHighClear"].isNull())measurement["autoHighClear"]=defaults.measurement.autoHighClear;
  JsonObject shared=measurement["sharedExposure"].is<JsonObject>()
   ?measurement["sharedExposure"].as<JsonObject>()
   :measurement["sharedExposure"].to<JsonObject>();
  if(shared["gain"].isNull())shared["gain"]=(int)defaults.measurement.sharedExposure.gain;
  if(shared["integration"].isNull())shared["integration"]=(int)defaults.measurement.sharedExposure.integration;
 }
 auto wled=document["wled"];
 if(wled.is<JsonObject>()){
  if(wled["enabled"].isNull())wled["enabled"]=defaults.wled.enabled;
  if(wled["host"].isNull())wled["host"]=defaults.wled.host;
  if(wled["port"].isNull())wled["port"]=defaults.wled.port;
  if(wled["updateIntervalMs"].isNull())wled["updateIntervalMs"]=defaults.wled.updateIntervalMs;
  if(wled["segment"].isNull())wled["segment"]=defaults.wled.segment;
  if(wled["brightness"].isNull())wled["brightness"]=defaults.wled.brightness;
  if(wled["sendBrightness"].isNull())wled["sendBrightness"]=defaults.wled.sendBrightness;
  if(wled["keepSelectedEffect"].isNull())wled["keepSelectedEffect"]=defaults.wled.keepSelectedEffect;
  if(wled["host"].is<const char*>()){
   String host=wled["host"].as<String>();host.trim();
   if(host.startsWith("http://"))host.remove(0,7);
   int colon=host.lastIndexOf(':');
   if(colon>0&&host.indexOf(':')==colon){
    String portText=host.substring(colon+1);bool digits=!portText.isEmpty();
    for(char c:portText)digits&=isdigit((unsigned char)c);
    long port=digits?portText.toInt():0;
    if(port>=1&&port<=65535){wled["port"]=port;host.remove(colon);}
   }
   for(size_t i=0;i<host.length();i++)if(host[i]=='_')host.setCharAt(i,'-');
   wled["host"]=host;
  }
 }
 auto system=document["system"];
 if(system.is<JsonObject>()){
  if(system["developerMode"].isNull())system["developerMode"]=defaults.system.developerMode;
  if(system["logLevel"].isNull())system["logLevel"]=(int)defaults.system.logLevel;
  if(system["otaEnabled"].isNull())system["otaEnabled"]=defaults.system.otaEnabled;
  if(system["browserOtaEnabled"].isNull())system["browserOtaEnabled"]=defaults.system.browserOtaEnabled;
  if(system["platformioOtaEnabled"].isNull())system["platformioOtaEnabled"]=defaults.system.platformioOtaEnabled;
  if(system["authenticationEnabled"].isNull())system["authenticationEnabled"]=defaults.system.authenticationEnabled;
  if(system["authenticationUser"].isNull())system["authenticationUser"]=defaults.system.authenticationUser;
  if(system["authenticationPassword"].isNull())system["authenticationPassword"]=defaults.system.authenticationPassword;
  if(system["authenticationPasswordSet"].isNull())
   system["authenticationPasswordSet"]=system["authenticationPassword"].is<const char*>()&&!system["authenticationPassword"].as<String>().isEmpty();
  if(system["authenticationPassword"].is<const char*>()){
   String password=system["authenticationPassword"].as<String>();
   if((!password.isEmpty()&&password.length()<8)||password.length()>64){
    system["authenticationPassword"]="";
    system["authenticationPasswordSet"]=false;
    system["authenticationEnabled"]=false;
   }
  }
  if(system["logLevel"].is<long>()&&system["logLevel"].as<long>()>3)system["logLevel"]=3;
 }
 auto vinyl=document["vinyl"];
 if(vinyl.is<JsonObject>()){
  if(vinyl["presenceDetection"].isNull())vinyl["presenceDetection"]=defaults.vinyl.presenceDetection;
  if(vinyl["clearThreshold"].isNull())vinyl["clearThreshold"]=defaults.vinyl.clearThreshold;
   if(vinyl["requiredSensors"].isNull())vinyl["requiredSensors"]=defaults.vinyl.requiredSensors;
 if(vinyl["colorChangeThreshold"].isNull())vinyl["colorChangeThreshold"]=defaults.vinyl.colorChangeThreshold;
 if(vinyl["colorHoldSeconds"].isNull())vinyl["colorHoldSeconds"]=defaults.vinyl.colorHoldSeconds;
 if(vinyl["outputSaturationPercent"].isNull())vinyl["outputSaturationPercent"]=defaults.vinyl.outputSaturationPercent;
 if(vinyl["outputNormalizationStrength"].isNull())vinyl["outputNormalizationStrength"]=defaults.vinyl.outputNormalizationStrength;
  if(schema<8){
   vinyl["darknessCutoffEnabled"]=defaults.vinyl.darknessCutoffEnabled;
   vinyl["darknessCutoffPercent"]=defaults.vinyl.darknessCutoffPercent;
  }
 if(vinyl["defaultEnabled"].isNull())vinyl["defaultEnabled"]=defaults.vinyl.defaultEnabled;
  if(vinyl["defaultAfterSeconds"].isNull())vinyl["defaultAfterSeconds"]=defaults.vinyl.defaultAfterSeconds;
  JsonObject defaultColor=vinyl["defaultColor"].is<JsonObject>()
   ?vinyl["defaultColor"].as<JsonObject>()
   :vinyl["defaultColor"].to<JsonObject>();
  if(defaultColor["r"].isNull())defaultColor["r"]=defaults.vinyl.defaultColor.red;
  if(defaultColor["g"].isNull())defaultColor["g"]=defaults.vinyl.defaultColor.green;
  if(defaultColor["b"].isNull())defaultColor["b"]=defaults.vinyl.defaultColor.blue;
  if(vinyl["offEnabled"].isNull())vinyl["offEnabled"]=defaults.vinyl.offEnabled;
  if(vinyl["offAfterSeconds"].isNull())vinyl["offAfterSeconds"]=defaults.vinyl.offAfterSeconds;
  long threshold=vinyl["colorChangeThreshold"]|12L;
  if(threshold>764)vinyl["colorChangeThreshold"]=764;
  bool defaultEnabled=vinyl["defaultEnabled"]|true,offEnabled=vinyl["offEnabled"]|true;
  long defaultAfter=vinyl["defaultAfterSeconds"]|20L,offAfter=vinyl["offAfterSeconds"]|60L;
  if(defaultEnabled&&offEnabled&&offAfter<=defaultAfter){
   if(defaultAfter<300)vinyl["offAfterSeconds"]=defaultAfter+1;
   else vinyl["offEnabled"]=false;
  }
 }
 auto sensors=document["sensors"];
 if(sensors.is<JsonArray>()&&sensors.size()==MaxSensors){
  size_t enabled=0,firstEnabled=0;bool positive=false;
  for(size_t i=0;i<MaxSensors;i++){
   auto sensor=sensors[i];
   if(!sensor.is<JsonObject>())continue;
   if(sensor["enabled"].isNull())sensor["enabled"]=defaults.sensors[i].enabled;
   if(sensor["name"].isNull())sensor["name"]=defaults.sensors[i].name;
   if(sensor["weight"].isNull())sensor["weight"]=defaults.sensors[i].weight;
   if(sensor["lightCorrection"].isNull())sensor["lightCorrection"]=defaults.sensors[i].lightCorrection;
   JsonObject exposure=sensor["exposure"].is<JsonObject>()
    ?sensor["exposure"].as<JsonObject>()
    :sensor["exposure"].to<JsonObject>();
   if(exposure["gain"].isNull())exposure["gain"]=(int)defaults.sensors[i].exposure.gain;
   if(exposure["integration"].isNull())exposure["integration"]=(int)defaults.sensors[i].exposure.integration;
   JsonObject calibration=sensor["calibration"].is<JsonObject>()
    ?sensor["calibration"].as<JsonObject>()
    :sensor["calibration"].to<JsonObject>();
   if(calibration["red"].isNull())calibration["red"]=defaults.sensors[i].calibration.red;
   if(calibration["green"].isNull())calibration["green"]=defaults.sensors[i].calibration.green;
   if(calibration["blue"].isNull())calibration["blue"]=defaults.sensors[i].calibration.blue;
   if(calibration["lightBrightness"].isNull())calibration["lightBrightness"]=defaults.sensors[i].calibration.lightBrightness;
   if(calibration["valid"].isNull())calibration["valid"]=defaults.sensors[i].calibration.valid;
   if(schema<8){
    calibration["darkClear"]=0;
    calibration["whiteClear"]=0;
    calibration["brightnessValid"]=false;
   }
   JsonObject manualCorrection=sensor["manualCorrection"].is<JsonObject>()
    ?sensor["manualCorrection"].as<JsonObject>()
    :sensor["manualCorrection"].to<JsonObject>();
   if(manualCorrection["red"].isNull())manualCorrection["red"]=defaults.sensors[i].manualCorrection.red;
   if(manualCorrection["green"].isNull())manualCorrection["green"]=defaults.sensors[i].manualCorrection.green;
   if(manualCorrection["blue"].isNull())manualCorrection["blue"]=defaults.sensors[i].manualCorrection.blue;
   bool on=sensor["enabled"]|false;
   if(on){if(!enabled)firstEnabled=i;enabled++;}
   if(sensor["weight"].is<float>()){
    float weight=sensor["weight"].as<float>();
    if(weight<0)sensor["weight"]=0;
    else if(weight>100)sensor["weight"]=100;
    positive|=on&&weight>0;
   }
  }
  if(!enabled&&sensors[0].is<JsonObject>()){sensors[0]["enabled"]=true;enabled=1;firstEnabled=0;}
  if(!positive&&enabled&&sensors[firstEnabled].is<JsonObject>())sensors[firstEnabled]["weight"]=1;
  if(vinyl.is<JsonObject>()&&vinyl["requiredSensors"].is<long>()&&vinyl["requiredSensors"].as<long>()>(long)enabled)vinyl["requiredSensors"]=enabled;
 }
 document["schemaVersion"]=ConfigStore::SchemaVersion;
}
bool completeBackup(JsonVariantConst document){
 auto hardware=document["hardware"];auto wifi=document["wifi"];auto light=document["light"];
 auto measurement=document["measurement"];auto vinyl=document["vinyl"];auto wled=document["wled"];auto system=document["system"];
 if(!hardware.is<JsonObjectConst>()||hardware["boardProfile"].isNull()||hardware["i2cSdaPin"].isNull()||hardware["i2cSclPin"].isNull()||
    !hardware["sensorLedPins"].is<JsonArrayConst>()||!hardware["tcaChannels"].is<JsonArrayConst>())return false;
 if(!wifi.is<JsonObjectConst>()||wifi["ssid"].isNull()||wifi["password"].isNull()||wifi["hostname"].isNull()||
    wifi["fallbackEnabled"].isNull()||wifi["fallbackDelaySeconds"].isNull()||wifi["fallbackSsid"].isNull()||wifi["fallbackPassword"].isNull())return false;
 if(!light.is<JsonObjectConst>()||light["enabled"].isNull()||light["brightness"].isNull()||
    light["activeHigh"].isNull()||light["onlyDuringMeasurement"].isNull())return false;
 if(!measurement.is<JsonObjectConst>()||measurement["sampleIntervalMs"].isNull()||measurement["rollingSeconds"].isNull()||
    measurement["averagingMode"].isNull()||measurement["rpm"].isNull()||measurement["revolutions"].isNull()||
    measurement["sensitivityMode"].isNull()||measurement["autoLowClear"].isNull()||measurement["autoHighClear"].isNull()||
    !measurement["sharedExposure"].is<JsonObjectConst>()||measurement["sharedExposure"]["gain"].isNull()||
    measurement["sharedExposure"]["integration"].isNull())return false;
  if(!vinyl.is<JsonObjectConst>()||vinyl["presenceDetection"].isNull()||vinyl["clearThreshold"].isNull()||
     vinyl["requiredSensors"].isNull()||vinyl["colorChangeThreshold"].isNull()||vinyl["colorHoldSeconds"].isNull()||
    vinyl["outputSaturationPercent"].isNull()||vinyl["outputNormalizationStrength"].isNull()||
    vinyl["darknessCutoffEnabled"].isNull()||vinyl["darknessCutoffPercent"].isNull()||
    vinyl["defaultEnabled"].isNull()||vinyl["defaultAfterSeconds"].isNull()||!vinyl["defaultColor"].is<JsonObjectConst>()||
    vinyl["defaultColor"]["r"].isNull()||vinyl["defaultColor"]["g"].isNull()||vinyl["defaultColor"]["b"].isNull()||
    vinyl["offEnabled"].isNull()||vinyl["offAfterSeconds"].isNull())return false;
 if(!wled.is<JsonObjectConst>()||wled["enabled"].isNull()||wled["host"].isNull()||wled["port"].isNull()||
    wled["updateIntervalMs"].isNull()||wled["segment"].isNull()||wled["brightness"].isNull()||
    wled["sendBrightness"].isNull()||wled["keepSelectedEffect"].isNull())return false;
 if(!system.is<JsonObjectConst>()||system["developerMode"].isNull()||system["logLevel"].isNull()||system["otaEnabled"].isNull()||
    system["browserOtaEnabled"].isNull()||system["platformioOtaEnabled"].isNull()||
    system["authenticationEnabled"].isNull()||system["authenticationUser"].isNull()||
    system["authenticationPasswordSet"].isNull()||system["authenticationPassword"].isNull())return false;
 auto sensors=document["sensors"];
 if(!sensors.is<JsonArrayConst>()||sensors.size()!=MaxSensors)return false;
 for(auto sensor:sensors.as<JsonArrayConst>()){
  if(!sensor.is<JsonObjectConst>()||sensor["enabled"].isNull()||sensor["name"].isNull()||sensor["weight"].isNull()||
     sensor["lightCorrection"].isNull()||!sensor["exposure"].is<JsonObjectConst>()||sensor["exposure"]["gain"].isNull()||
     sensor["exposure"]["integration"].isNull()||!sensor["calibration"].is<JsonObjectConst>()||
     sensor["calibration"]["red"].isNull()||sensor["calibration"]["green"].isNull()||sensor["calibration"]["blue"].isNull()||
     sensor["calibration"]["lightBrightness"].isNull()||sensor["calibration"]["valid"].isNull()||
     sensor["calibration"]["darkClear"].isNull()||sensor["calibration"]["whiteClear"].isNull()||
     sensor["calibration"]["brightnessValid"].isNull()||
     !sensor["manualCorrection"].is<JsonObjectConst>()||sensor["manualCorrection"]["red"].isNull()||
     sensor["manualCorrection"]["green"].isNull()||sensor["manualCorrection"]["blue"].isNull())return false;
 }
 return true;
}
}

bool ConfigStore::begin(){
 setFactoryDefaults(config_);
 opened_=preferences_.begin("vinylchroma",false);
 if(!opened_){lastError_="configuration storage could not be opened";healthy_=false;return false;}
 if(!load())return false;
 healthy_=validatePublicRoundTrip();
 return healthy_;
}

bool ConfigStore::validatePublicRoundTrip(){
 AppConfig snapshot=config_;
 bool valid=importJson(exportJson(false),false);
 config_=snapshot;
 return valid;
}

bool ConfigStore::load(){
 setFactoryDefaults(config_);
 if(!opened_){lastError_="configuration storage is not open";healthy_=false;return false;}
 String stored=preferences_.getString("config","");
 if(stored.isEmpty()){lastError_="";healthy_=true;return true;}
 JsonDocument metadata;long storedSchema=0;bool schemaMigration=false,profileMigration=true;
 if(!deserializeJson(metadata,stored)&&readInteger(metadata["schemaVersion"],0,SchemaVersion,storedSchema))
  schemaMigration=storedSchema!=SchemaVersion;
 if(metadata["hardware"]["boardProfile"].is<const char*>())
  profileMigration=metadata["hardware"]["boardProfile"].as<String>()!=BoardProfile::Id;
 if(!importJson(stored)){healthy_=false;return false;}
 bool migrated=schemaMigration||profileMigration;
 if(config_.wifi.fallbackSsid=="VinylChroma-Setup"){config_.wifi.fallbackSsid="VinylChroma";migrated=true;}
 if(config_.wifi.fallbackPassword=="vinylchroma"){config_.wifi.fallbackPassword="vinyl!1234";migrated=true;}
 healthy_=true;
 return !migrated||save();
}

bool ConfigStore::save(){
 if(!opened_){lastError_="configuration storage is not open";return false;}
 bool ok=preferences_.putString("config",exportJson(true))>0;
 lastError_=ok?"":"configuration could not be written to non-volatile storage";
 healthy_=ok;
 return ok;
}

bool ConfigStore::reset(){
 if(!opened_||!preferences_.clear()){lastError_="configuration storage could not be cleared";healthy_=false;return false;}
 setFactoryDefaults(config_);
 lastError_="";
 healthy_=true;
 return true;
}

String ConfigStore::exportJson(bool includePassword)const{
 JsonDocument document;toJson(document,includePassword);String result;serializeJson(document,result);return result;
}

bool ConfigStore::importJson(const String&json,bool replace){
 lastError_="";
 if(json.isEmpty()||json.length()>16384){lastError_="request body is empty or exceeds 16 KB";return false;}
 JsonDocument document;
 if(deserializeJson(document,json)||!document.is<JsonObject>()){lastError_="request body is not a JSON object";return false;}
 long schema=0;
 if(!readInteger(document["schemaVersion"],0,SchemaVersion,schema)){
  lastError_="schema version is invalid or newer than this firmware";return false;
 }
 if(schema==DowngradeSourceSchemaVersion){
  document.remove("externalApi");
 }
 if(!replace&&schema<SchemaVersion&&document["system"].is<JsonObject>()){
  auto system=document["system"];
  if(system["otaEnabled"].isNull())system["otaEnabled"]=config_.system.otaEnabled;
  if(system["browserOtaEnabled"].isNull())system["browserOtaEnabled"]=config_.system.browserOtaEnabled;
  if(system["platformioOtaEnabled"].isNull())system["platformioOtaEnabled"]=config_.system.platformioOtaEnabled;
 }
 if(!replace&&schema<SchemaVersion&&document["vinyl"].is<JsonObject>()&&
    document["vinyl"]["outputSaturationPercent"].isNull())
  document["vinyl"]["outputSaturationPercent"]=config_.vinyl.outputSaturationPercent;
 migrateLegacy(document,schema);
 if(replace){
  if(!completeBackup(document)){lastError_="backup is incomplete";return false;}
 }
 AppConfig previous=config_;
 if(replace)setFactoryDefaults(config_);
 if(!fromJson(document.as<JsonVariantConst>())){config_=previous;return false;}
 lastError_="";
 return true;
}

void ConfigStore::toJson(JsonDocument&document,bool includePassword)const{
 document["schemaVersion"]=SchemaVersion;
 auto board=document["board"].to<JsonObject>();
 board["id"]=BoardProfile::Id;board["name"]=BoardProfile::Name;
 board["gpioNotes"]=BoardProfile::GpioNotes;
 auto allowedGpios=board["allowedGpios"].to<JsonArray>();for(uint8_t pin:BoardProfile::AllowedGpios)allowedGpios.add(pin);
 auto hardware=document["hardware"].to<JsonObject>();
 hardware["boardProfile"]=BoardProfile::Id;
 hardware["i2cSdaPin"]=config_.hardware.i2cSdaPin;hardware["i2cSclPin"]=config_.hardware.i2cSclPin;
 auto ledPins=hardware["sensorLedPins"].to<JsonArray>();for(auto pin:config_.hardware.sensorLedPins)ledPins.add(pin);
 auto channels=hardware["tcaChannels"].to<JsonArray>();for(auto channel:config_.hardware.tcaChannels)channels.add(channel);

 auto wifi=document["wifi"].to<JsonObject>();
 wifi["ssid"]=config_.wifi.ssid;if(includePassword)wifi["password"]=config_.wifi.password;
 wifi["hostname"]=config_.wifi.hostname;wifi["fallbackEnabled"]=config_.wifi.fallbackEnabled;wifi["fallbackDelaySeconds"]=config_.wifi.fallbackDelaySeconds;
 wifi["fallbackSsid"]=config_.wifi.fallbackSsid;wifi["fallbackPasswordSet"]=!config_.wifi.fallbackPassword.isEmpty();
 if(includePassword)wifi["fallbackPassword"]=config_.wifi.fallbackPassword;

 auto light=document["light"].to<JsonObject>();
 light["enabled"]=config_.light.enabled;light["brightness"]=config_.light.brightness;light["activeHigh"]=config_.light.activeHigh;light["onlyDuringMeasurement"]=config_.light.onlyDuringMeasurement;

 auto measurement=document["measurement"].to<JsonObject>();
 measurement["sampleIntervalMs"]=config_.measurement.sampleIntervalMs;measurement["rollingSeconds"]=config_.measurement.rollingSeconds;measurement["averagingMode"]=(int)config_.measurement.averagingMode;
 measurement["rpm"]=config_.measurement.rpm;measurement["revolutions"]=config_.measurement.revolutions;measurement["sensitivityMode"]=(int)config_.measurement.sensitivityMode;
 measurement["autoLowClear"]=config_.measurement.autoLowClear;measurement["autoHighClear"]=config_.measurement.autoHighClear;
 measurement["sharedExposure"]["gain"]=(int)config_.measurement.sharedExposure.gain;measurement["sharedExposure"]["integration"]=(int)config_.measurement.sharedExposure.integration;

 auto vinyl=document["vinyl"].to<JsonObject>();
  vinyl["presenceDetection"]=config_.vinyl.presenceDetection;vinyl["clearThreshold"]=config_.vinyl.clearThreshold;vinyl["requiredSensors"]=config_.vinyl.requiredSensors;
  vinyl["colorChangeThreshold"]=config_.vinyl.colorChangeThreshold;vinyl["colorHoldSeconds"]=config_.vinyl.colorHoldSeconds;
  vinyl["outputSaturationPercent"]=config_.vinyl.outputSaturationPercent;vinyl["outputNormalizationStrength"]=config_.vinyl.outputNormalizationStrength;
 vinyl["darknessCutoffEnabled"]=config_.vinyl.darknessCutoffEnabled;vinyl["darknessCutoffPercent"]=config_.vinyl.darknessCutoffPercent;vinyl["defaultEnabled"]=config_.vinyl.defaultEnabled;
 vinyl["defaultAfterSeconds"]=config_.vinyl.defaultAfterSeconds;vinyl["defaultColor"]["r"]=config_.vinyl.defaultColor.red;vinyl["defaultColor"]["g"]=config_.vinyl.defaultColor.green;
 vinyl["defaultColor"]["b"]=config_.vinyl.defaultColor.blue;vinyl["offEnabled"]=config_.vinyl.offEnabled;vinyl["offAfterSeconds"]=config_.vinyl.offAfterSeconds;

 auto wled=document["wled"].to<JsonObject>();
 wled["enabled"]=config_.wled.enabled;wled["host"]=config_.wled.host;wled["port"]=config_.wled.port;wled["updateIntervalMs"]=config_.wled.updateIntervalMs;
 wled["segment"]=config_.wled.segment;wled["brightness"]=config_.wled.brightness;wled["sendBrightness"]=config_.wled.sendBrightness;wled["keepSelectedEffect"]=config_.wled.keepSelectedEffect;

 auto system=document["system"].to<JsonObject>();
 system["developerMode"]=config_.system.developerMode;system["logLevel"]=(int)config_.system.logLevel;system["otaEnabled"]=config_.system.otaEnabled;
 system["browserOtaEnabled"]=config_.system.browserOtaEnabled;system["platformioOtaEnabled"]=config_.system.platformioOtaEnabled;system["authenticationEnabled"]=config_.system.authenticationEnabled;
 system["authenticationUser"]=config_.system.authenticationUser;system["authenticationPasswordSet"]=!config_.system.authenticationPassword.isEmpty();
 if(includePassword)system["authenticationPassword"]=config_.system.authenticationPassword;

 auto sensors=document["sensors"].to<JsonArray>();
 for(const auto&sensor:config_.sensors){
  auto item=sensors.add<JsonObject>();item["enabled"]=sensor.enabled;item["name"]=sensor.name;item["weight"]=sensor.weight;item["lightCorrection"]=sensor.lightCorrection;
  item["exposure"]["gain"]=(int)sensor.exposure.gain;item["exposure"]["integration"]=(int)sensor.exposure.integration;
  item["calibration"]["red"]=sensor.calibration.red;item["calibration"]["green"]=sensor.calibration.green;item["calibration"]["blue"]=sensor.calibration.blue;
  item["calibration"]["lightBrightness"]=sensor.calibration.lightBrightness;item["calibration"]["valid"]=sensor.calibration.valid;
  item["calibration"]["darkClear"]=sensor.calibration.darkClear;item["calibration"]["whiteClear"]=sensor.calibration.whiteClear;item["calibration"]["brightnessValid"]=sensor.calibration.brightnessValid;
  item["manualCorrection"]["red"]=sensor.manualCorrection.red;item["manualCorrection"]["green"]=sensor.manualCorrection.green;item["manualCorrection"]["blue"]=sensor.manualCorrection.blue;
 }
}

bool ConfigStore::fromJson(JsonVariantConst document){
 lastError_="configuration section structure is invalid";
 const char*sections[]={"hardware","wifi","light","measurement","vinyl","wled","system"};
 for(const char*section:sections)if(!optionalObject(document[section]))return false;
 lastError_="hardware or GPIO mapping is invalid";
 auto hardware=document["hardware"];
 String profile=config_.hardware.boardProfile;
 if(!readString(hardware["boardProfile"],profile,64,false)||profile!=BoardProfile::Id)return false;
 config_.hardware.boardProfile=profile;
 long integer=config_.hardware.i2cSdaPin;
 if(!readInteger(hardware["i2cSdaPin"],0,255,integer))return false;config_.hardware.i2cSdaPin=(uint8_t)integer;
 integer=config_.hardware.i2cSclPin;if(!readInteger(hardware["i2cSclPin"],0,255,integer))return false;config_.hardware.i2cSclPin=(uint8_t)integer;
 if(!hardware["sensorLedPins"].isNull()){
  if(!hardware["sensorLedPins"].is<JsonArrayConst>()||hardware["sensorLedPins"].size()!=MaxSensors)return false;
  size_t index=0;for(auto value:hardware["sensorLedPins"].as<JsonArrayConst>()){long pin=0;if(!readInteger(value,0,255,pin))return false;config_.hardware.sensorLedPins[index++]=(uint8_t)pin;}
 }
 if(!hardware["tcaChannels"].isNull()){
  if(!hardware["tcaChannels"].is<JsonArrayConst>()||hardware["tcaChannels"].size()!=MaxSensors)return false;
  size_t index=0;for(auto value:hardware["tcaChannels"].as<JsonArrayConst>()){long channel=0;if(!readInteger(value,0,7,channel))return false;config_.hardware.tcaChannels[index++]=(uint8_t)channel;}
 }

 lastError_="Wi-Fi or fallback access point settings are invalid";
 auto wifi=document["wifi"];
 if(!readString(wifi["ssid"],config_.wifi.ssid,32)||!readString(wifi["password"],config_.wifi.password,63)||!readString(wifi["hostname"],config_.wifi.hostname,63,false)||
     !readString(wifi["fallbackSsid"],config_.wifi.fallbackSsid,32)||!readString(wifi["fallbackPassword"],config_.wifi.fallbackPassword,63))return false;
 if(!readBool(wifi["fallbackEnabled"],config_.wifi.fallbackEnabled))return false;
 integer=config_.wifi.fallbackDelaySeconds;if(!readInteger(wifi["fallbackDelaySeconds"],0,3600,integer))return false;config_.wifi.fallbackDelaySeconds=(uint16_t)integer;
 if(!validHostname(config_.wifi.hostname))return false;
 if(!config_.wifi.password.isEmpty()&&config_.wifi.password.length()<8)return false;
 if(!config_.wifi.fallbackEnabled&&config_.wifi.ssid.isEmpty())return false;
 if((!config_.wifi.fallbackPassword.isEmpty()&&config_.wifi.fallbackPassword.length()<8)||
    config_.wifi.fallbackPassword.length()>63)return false;
 if(config_.wifi.fallbackEnabled&&(config_.wifi.fallbackSsid.isEmpty()||config_.wifi.fallbackPassword.isEmpty()))return false;

 lastError_="sensor illumination settings are invalid";
 auto light=document["light"];
 if(!readBool(light["enabled"],config_.light.enabled)||!readBool(light["activeHigh"],config_.light.activeHigh)||
    !readBool(light["onlyDuringMeasurement"],config_.light.onlyDuringMeasurement))return false;
 integer=config_.light.brightness;if(!readInteger(light["brightness"],0,100,integer))return false;config_.light.brightness=(uint8_t)integer;

 lastError_="measurement or sensitivity settings are invalid";
 auto measurement=document["measurement"];
 integer=config_.measurement.sampleIntervalMs;if(!readInteger(measurement["sampleIntervalMs"],50,5000,integer))return false;config_.measurement.sampleIntervalMs=(uint16_t)integer;
 if(!readFloat(measurement["rollingSeconds"],0.1F,300.0F,config_.measurement.rollingSeconds)||!readFloat(measurement["rpm"],1.0F,1000.0F,config_.measurement.rpm)||
    !readFloat(measurement["revolutions"],0.1F,20.0F,config_.measurement.revolutions))return false;
 config_.measurement.rollingSeconds=roundf(config_.measurement.rollingSeconds*10.0F)/10.0F;
 config_.measurement.revolutions=roundf(config_.measurement.revolutions*10.0F)/10.0F;
 integer=(long)config_.measurement.averagingMode;if(!readInteger(measurement["averagingMode"],0,3,integer))return false;
 if(integer==(long)AveragingMode::LegacyMultipleRevolutions)integer=(long)AveragingMode::Revolution;config_.measurement.averagingMode=(AveragingMode)integer;
 integer=(long)config_.measurement.sensitivityMode;if(!readInteger(measurement["sensitivityMode"],0,2,integer))return false;config_.measurement.sensitivityMode=(SensitivityMode)integer;
 integer=config_.measurement.autoLowClear;if(!readInteger(measurement["autoLowClear"],0,65534,integer))return false;config_.measurement.autoLowClear=(uint16_t)integer;
 integer=config_.measurement.autoHighClear;if(!readInteger(measurement["autoHighClear"],1,65535,integer))return false;config_.measurement.autoHighClear=(uint16_t)integer;
 if(config_.measurement.autoLowClear>=config_.measurement.autoHighClear||!readExposure(measurement["sharedExposure"],config_.measurement.sharedExposure))return false;

 lastError_="vinyl detection, output, or timer settings are invalid";
 auto vinyl=document["vinyl"];
 if(!readBool(vinyl["presenceDetection"],config_.vinyl.presenceDetection)||!readBool(vinyl["defaultEnabled"],config_.vinyl.defaultEnabled)||
    !readBool(vinyl["offEnabled"],config_.vinyl.offEnabled)||!readBool(vinyl["darknessCutoffEnabled"],config_.vinyl.darknessCutoffEnabled))return false;
 integer=config_.vinyl.clearThreshold;if(!readInteger(vinyl["clearThreshold"],0,65535,integer))return false;config_.vinyl.clearThreshold=(uint16_t)integer;
 integer=config_.vinyl.requiredSensors;if(!readInteger(vinyl["requiredSensors"],1,MaxSensors,integer))return false;config_.vinyl.requiredSensors=(uint8_t)integer;
  integer=config_.vinyl.colorChangeThreshold;if(!readInteger(vinyl["colorChangeThreshold"],0,764,integer))return false;config_.vinyl.colorChangeThreshold=(uint16_t)integer;
  if(!readFloat(vinyl["colorHoldSeconds"],0.0F,300.0F,config_.vinyl.colorHoldSeconds))return false;
  config_.vinyl.colorHoldSeconds=roundf(config_.vinyl.colorHoldSeconds*10.0F)/10.0F;
  integer=config_.vinyl.outputSaturationPercent;if(!readInteger(vinyl["outputSaturationPercent"],0,200,integer))return false;config_.vinyl.outputSaturationPercent=(uint16_t)integer;
  integer=config_.vinyl.outputNormalizationStrength;if(!readInteger(vinyl["outputNormalizationStrength"],0,100,integer))return false;config_.vinyl.outputNormalizationStrength=(uint8_t)integer;
 if(!readFloat(vinyl["darknessCutoffPercent"],0.0F,100.0F,config_.vinyl.darknessCutoffPercent))return false;
 config_.vinyl.darknessCutoffPercent=roundf(config_.vinyl.darknessCutoffPercent*10.0F)/10.0F;
 integer=config_.vinyl.defaultAfterSeconds;if(!readInteger(vinyl["defaultAfterSeconds"],0,300,integer))return false;config_.vinyl.defaultAfterSeconds=(uint16_t)integer;
 integer=config_.vinyl.offAfterSeconds;if(!readInteger(vinyl["offAfterSeconds"],0,300,integer))return false;config_.vinyl.offAfterSeconds=(uint16_t)integer;
 if(!optionalObject(vinyl["defaultColor"]))return false;
 long red=config_.vinyl.defaultColor.red,green=config_.vinyl.defaultColor.green,blue=config_.vinyl.defaultColor.blue;
 if(!readInteger(vinyl["defaultColor"]["r"],0,255,red)||!readInteger(vinyl["defaultColor"]["g"],0,255,green)||!readInteger(vinyl["defaultColor"]["b"],0,255,blue))return false;
 config_.vinyl.defaultColor={(uint8_t)red,(uint8_t)green,(uint8_t)blue};
 if(config_.vinyl.presenceDetection&&config_.vinyl.defaultEnabled&&config_.vinyl.offEnabled&&config_.vinyl.offAfterSeconds<=config_.vinyl.defaultAfterSeconds)return false;

 lastError_="WLED settings are invalid";
 auto wled=document["wled"];
 if(!readBool(wled["enabled"],config_.wled.enabled)||
    !readBool(wled["sendBrightness"],config_.wled.sendBrightness)||
    !readBool(wled["keepSelectedEffect"],config_.wled.keepSelectedEffect))return false;
 if(!readString(wled["host"],config_.wled.host,253))return false;
 integer=config_.wled.port;if(!readInteger(wled["port"],1,65535,integer))return false;config_.wled.port=(uint16_t)integer;
 integer=config_.wled.updateIntervalMs;if(!readInteger(wled["updateIntervalMs"],20,60000,integer))return false;config_.wled.updateIntervalMs=(uint16_t)integer;
 integer=config_.wled.segment;if(!readInteger(wled["segment"],0,255,integer))return false;config_.wled.segment=(uint8_t)integer;
 integer=config_.wled.brightness;if(!readInteger(wled["brightness"],0,255,integer))return false;config_.wled.brightness=(uint8_t)integer;
 if((config_.wled.enabled&&config_.wled.host.isEmpty())||
    (!config_.wled.host.isEmpty()&&!validWledHost(config_.wled.host)))return false;

 lastError_="system, OTA, or web authentication settings are invalid";
 auto system=document["system"];
 if(!readBool(system["developerMode"],config_.system.developerMode)||!readBool(system["otaEnabled"],config_.system.otaEnabled)||
    !readBool(system["browserOtaEnabled"],config_.system.browserOtaEnabled)||!readBool(system["platformioOtaEnabled"],config_.system.platformioOtaEnabled)||
    !readBool(system["authenticationEnabled"],config_.system.authenticationEnabled))return false;
 integer=(long)config_.system.logLevel;if(!readInteger(system["logLevel"],0,3,integer))return false;config_.system.logLevel=(LogLevel)integer;
 if(!readString(system["authenticationUser"],config_.system.authenticationUser,32)||!readString(system["authenticationPassword"],config_.system.authenticationPassword,64))return false;
 config_.system.authenticationUser.trim();
 if(!config_.system.authenticationPassword.isEmpty()&&config_.system.authenticationPassword.length()<8)return false;
 if(config_.system.authenticationEnabled&&(config_.system.authenticationUser.isEmpty()||config_.system.authenticationUser.indexOf(':')>=0||config_.system.authenticationPassword.length()<8))return false;

 lastError_="sensor settings or calibration values are invalid";
 auto sensors=document["sensors"];
 if(!sensors.isNull()){
  if(!sensors.is<JsonArrayConst>()||sensors.size()!=MaxSensors)return false;
  size_t index=0;
  for(auto item:sensors.as<JsonArrayConst>()){
   if(!item.is<JsonObjectConst>())return false;
   auto&sensor=config_.sensors[index++];
   if(!readBool(item["enabled"],sensor.enabled))return false;
   if(!readString(item["name"],sensor.name,32,false)||!readFloat(item["weight"],0.0F,100.0F,sensor.weight)||!readFloat(item["lightCorrection"],0.0F,2.0F,sensor.lightCorrection)||!readExposure(item["exposure"],sensor.exposure))return false;
   if(!optionalObject(item["calibration"]))return false;
   float calibrationRed=sensor.calibration.red,calibrationGreen=sensor.calibration.green,calibrationBlue=sensor.calibration.blue;
   float darkClear=sensor.calibration.darkClear,whiteClear=sensor.calibration.whiteClear;
   if(!readFloat(item["calibration"]["red"],0.05F,20.0F,calibrationRed)||!readFloat(item["calibration"]["green"],0.05F,20.0F,calibrationGreen)||
      !readFloat(item["calibration"]["blue"],0.05F,20.0F,calibrationBlue)||
      !readFloat(item["calibration"]["darkClear"],0.0F,10000000.0F,darkClear)||
      !readFloat(item["calibration"]["whiteClear"],0.0F,10000000.0F,whiteClear))return false;
   integer=sensor.calibration.lightBrightness;if(!readInteger(item["calibration"]["lightBrightness"],0,100,integer))return false;
   bool calibrationValid=sensor.calibration.valid;if(!readBool(item["calibration"]["valid"],calibrationValid))return false;
   bool brightnessValid=sensor.calibration.brightnessValid;if(!readBool(item["calibration"]["brightnessValid"],brightnessValid))return false;
   if(brightnessValid&&(!calibrationValid||whiteClear-darkClear<MinimumBrightnessReferenceGap))return false;
   sensor.calibration={calibrationRed,calibrationGreen,calibrationBlue,(uint8_t)integer,calibrationValid,darkClear,whiteClear,brightnessValid};
   if(!optionalObject(item["manualCorrection"]))return false;
   float manualRed=sensor.manualCorrection.red,manualGreen=sensor.manualCorrection.green,manualBlue=sensor.manualCorrection.blue;
   if(!readFloat(item["manualCorrection"]["red"],0.8F,1.2F,manualRed)||
      !readFloat(item["manualCorrection"]["green"],0.8F,1.2F,manualGreen)||
      !readFloat(item["manualCorrection"]["blue"],0.8F,1.2F,manualBlue))return false;
   sensor.manualCorrection={manualRed,manualGreen,manualBlue};
  }
 }

 size_t enabledSensors=0;bool positiveWeight=false;
 for(const auto&sensor:config_.sensors)if(sensor.enabled){enabledSensors++;positiveWeight|=sensor.weight>0;}
 if(enabledSensors==0||!positiveWeight)return false;
 if(config_.vinyl.presenceDetection&&config_.vinyl.requiredSensors>enabledSensors)return false;
 lastError_="hardware or GPIO mapping is invalid";
 if(!validateHardwareConfig(config_.hardware))return false;
 lastError_="";
 return true;
}
