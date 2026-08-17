#include "AppConfig.h"
namespace VinylChroma {
bool isUsableGpio(uint8_t pin){ return pin==1||pin==2||(pin>=4&&pin<=18)||pin==21; }
bool validateHardwareConfig(const HardwareConfig& c){
 bool usedPins[22]{};
 auto usePin=[&](uint8_t pin){if(!isUsableGpio(pin)||usedPins[pin])return false;usedPins[pin]=true;return true;};
 if(!usePin(c.i2cSdaPin)||!usePin(c.i2cSclPin))return false;
 for(auto pin:c.sensorLedPins)if(!usePin(pin))return false;
 bool usedChannels[8]{};
 for(auto channel:c.tcaChannels){if(channel>7||usedChannels[channel])return false;usedChannels[channel]=true;}
 return true;
}
void setFactoryDefaults(AppConfig& c){ c=AppConfig{}; for(size_t i=0;i<MaxSensors;i++){ c.sensors[i].enabled=(i==0); c.sensors[i].name="Sensor "+String(i+1); c.sensors[i].weight=1; c.sensors[i].lightCorrection=1; }}
}
