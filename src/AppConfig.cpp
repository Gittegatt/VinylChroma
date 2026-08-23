#include "AppConfig.h"
namespace VinylChroma {
bool isUsableGpio(uint8_t pin){ return BoardProfile::isUsableGpio(pin); }
bool validateHardwareConfig(const HardwareConfig& c){
 if(c.boardProfile!=BoardProfile::Id)return false;
 std::array<uint8_t,MaxSensors+2> pins{c.i2cSdaPin,c.i2cSclPin,c.sensorLedPins[0],c.sensorLedPins[1],c.sensorLedPins[2],c.sensorLedPins[3]};
 for(size_t i=0;i<pins.size();i++){
  if(!isUsableGpio(pins[i]))return false;
  for(size_t previous=0;previous<i;previous++)if(pins[i]==pins[previous])return false;
 }
 bool usedChannels[8]{};
 for(auto channel:c.tcaChannels){if(channel>7||usedChannels[channel])return false;usedChannels[channel]=true;}
 return true;
}
void setFactoryDefaults(AppConfig& c){ c=AppConfig{}; for(size_t i=0;i<MaxSensors;i++){ c.sensors[i].enabled=(i==0); c.sensors[i].name="Sensor "+String(i+1); c.sensors[i].weight=1; c.sensors[i].lightCorrection=1; }}
}
