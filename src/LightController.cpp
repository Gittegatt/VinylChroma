#include "LightController.h"
using namespace VinylChroma;
void LightController::begin(AppConfig&c){config_=&c;for(size_t i=0;i<MaxSensors;i++){ledcSetup(i,Hardware::LedPwmFrequency,Hardware::LedPwmResolution);ledcAttachPin(config_->hardware.sensorLedPins[i],i);}apply();}
void LightController::write(size_t i,bool on){float corrected=config_->light.brightness*config_->sensors[i].lightCorrection;uint8_t logicalDuty=(uint8_t)constrain((int)lroundf(corrected*2.55F),0,255);if(!on)logicalDuty=0;active_[i]=logicalDuty>0;uint8_t physicalDuty=config_->light.activeHigh?logicalDuty:255-logicalDuty;ledcWrite(i,physicalDuty);}
void LightController::apply(bool measurement,bool calibration){if(!config_)return;for(size_t i=0;i<MaxSensors;i++){bool on=config_->light.enabled&&config_->sensors[i].enabled;if(config_->light.onlyDuringMeasurement&&!measurement)on=false;if(calibration)on=config_->sensors[i].enabled;write(i,on);}}
void LightController::applySingle(size_t sensorIndex){if(!config_)return;for(size_t i=0;i<MaxSensors;i++)write(i,i==sensorIndex&&config_->sensors[i].enabled);}
void LightController::off(){if(!config_)return;for(size_t i=0;i<MaxSensors;i++)write(i,false);}
