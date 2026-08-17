#include "ColorEngine.h"
using namespace VinylChroma;
namespace {
struct HsvColor { float hue,saturation,value; };
float clamp01(float value){return max(0.0F,min(1.0F,value));}
HsvColor rgbToHsv(RgbColor color){
 float r=color.red/255.0F,g=color.green/255.0F,b=color.blue/255.0F,high=max(r,max(g,b)),low=min(r,min(g,b)),delta=high-low,hue=0;
 if(delta>0.0001F){if(high==r)hue=fmodf((g-b)/delta,6.0F);else if(high==g)hue=(b-r)/delta+2.0F;else hue=(r-g)/delta+4.0F;hue/=6.0F;if(hue<0)hue+=1.0F;}
 return{hue,high<=0?0:delta/high,high};
}
RgbColor hsvToRgb(HsvColor color){
 float h=color.hue-floorf(color.hue),s=clamp01(color.saturation),v=clamp01(color.value),scaled=h*6.0F;
 int sector=(int)floorf(scaled);float f=scaled-sector,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f)),r,g,b;
 switch(sector%6){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t;break;case 3:r=p;g=q;b=v;break;case 4:r=t;g=p;b=v;break;default:r=v;g=p;b=q;break;}
 return{(uint8_t)lroundf(r*255),(uint8_t)lroundf(g*255),(uint8_t)lroundf(b*255)};
}
RgbColor blend(RgbColor a,RgbColor b,float amount){amount=clamp01(amount);return{(uint8_t)lroundf(a.red+(b.red-a.red)*amount),(uint8_t)lroundf(a.green+(b.green-a.green)*amount),(uint8_t)lroundf(a.blue+(b.blue-a.blue)*amount)};}
RgbColor normalizeValue(RgbColor color,uint8_t strength){
 uint8_t maximum=max(color.red,max(color.green,color.blue));
 if(maximum==0||maximum==255||strength==0)return color;
 uint32_t targetHundred=100U*maximum+uint32_t(strength)*(255U-maximum);
 uint32_t denominator=100U*maximum;
 auto scale=[targetHundred,denominator](uint8_t channel){return(uint8_t)((uint32_t(channel)*targetHundred+denominator/2U)/denominator);};
 return{scale(color.red),scale(color.green),scale(color.blue)};
}
RgbColor marblePaletteColor(RgbColor base,uint16_t count,uint16_t index,uint8_t strength){
 if(strength==0)return base;
 constexpr float TwoPi=6.28318530718F,HueRadius=28.0F/360.0F;
 HsvColor hsv=rgbToHsv(base);float phase=TwoPi*index/max<uint16_t>(2,count);
 // Marble moves only around the selected hue. Keeping saturation and value
 // fixed prevents pale or white peaks and keeps every channel within the
 // selected base color's maximum channel level.
 return hsvToRgb({hsv.hue+sinf(phase)*HueRadius*(strength/100.0F),hsv.saturation,hsv.value});
}
uint16_t marbleRandomIndex(uint32_t slot,uint16_t count){uint32_t value=slot+0x9E3779B9UL;value^=value>>16;value*=0x7FEB352DUL;value^=value>>15;value*=0x846CA68BUL;value^=value>>16;return value%max<uint16_t>(2,count);}
RgbColor marbleColor(RgbColor base,uint16_t count,uint8_t strength,uint32_t now){
 constexpr uint32_t TransitionMs=900;uint32_t slot=now/TransitionMs;float t=(now%TransitionMs)/(float)TransitionMs;t=t*t*(3.0F-2.0F*t);
 RgbColor from=marblePaletteColor(base,count,marbleRandomIndex(slot,count),strength),to=marblePaletteColor(base,count,marbleRandomIndex(slot+1,count),strength);
 return blend(from,to,t);
}
}

uint16_t ColorEngine::distance(RgbColor a,RgbColor b){
 return abs(int(a.red)-b.red)+abs(int(a.green)-b.green)+abs(int(a.blue)-b.blue);
}

uint32_t ColorEngine::windowMs()const{
 switch(config_.measurement.averagingMode){
  case AveragingMode::Instant:return 1;
  case AveragingMode::RollingAverage:return uint32_t(config_.measurement.rollingSeconds*1000.0F);
  case AveragingMode::Revolution:
  case AveragingMode::LegacyMultipleRevolutions:
   return uint32_t(60000.0F/max(1.0F,config_.measurement.rpm)*
                   max(0.1F,config_.measurement.revolutions));
 }
 return 1000;
}

RgbColor ColorEngine::averageWindow(uint32_t window)const{
 if(samples_.empty())return{};
 uint32_t now=millis();
 uint64_t red=0,green=0,blue=0,totalWeight=0;
 for(auto it=samples_.rbegin();it!=samples_.rend();++it){
  if(now-it->time>window)break;
  red+=(uint64_t)it->color.red*it->weight;
  green+=(uint64_t)it->color.green*it->weight;
  blue+=(uint64_t)it->color.blue*it->weight;
  totalWeight+=it->weight;
 }
 return totalWeight
  ?RgbColor{(uint8_t)(red/totalWeight),(uint8_t)(green/totalWeight),(uint8_t)(blue/totalWeight)}
  :samples_.back().color;
}

void ColorEngine::compactSamples(){
 std::deque<Sample> compacted;
 for(size_t i=0;i<samples_.size();i+=2){
  if(i+1>=samples_.size()){compacted.push_back(samples_[i]);continue;}
  const auto&a=samples_[i];
  const auto&b=samples_[i+1];
  uint32_t weight=a.weight+b.weight;
  compacted.push_back({
   b.time,
   {
    (uint8_t)(((uint64_t)a.color.red*a.weight+(uint64_t)b.color.red*b.weight)/weight),
    (uint8_t)(((uint64_t)a.color.green*a.weight+(uint64_t)b.color.green*b.weight)/weight),
    (uint8_t)(((uint64_t)a.color.blue*a.weight+(uint64_t)b.color.blue*b.weight)/weight)
   },
   weight
  });
 }
 samples_.swap(compacted);
}

void ColorEngine::recordHistory(RgbColor color){
 constexpr uint16_t HistoryDistance=12;
 // The history is an independent, RAM-only most-recent list. A near match is
 // moved to the front instead of creating another indistinguishable square.
 auto it=history_.begin();
 if(it!=history_.end()&&distance(color,*it)<=HistoryDistance){
  *it=color;
  ++it;
  while(it!=history_.end()){
   if(distance(color,*it)<=HistoryDistance)it=history_.erase(it);
   else ++it;
  }
  return;
 }
 while(it!=history_.end()){
  if(distance(color,*it)<=HistoryDistance)it=history_.erase(it);
  else ++it;
 }
 history_.push_front(color);
 while(history_.size()>20)history_.pop_back();
}

void ColorEngine::setOutput(RgbColor color,bool on,const String&state,bool addToHistory){
 output_=color;
 outputOn_=on;
 state_=state;
 if(on&&addToHistory)recordHistory(color);
}

void ColorEngine::rememberNormalOutput(){
 if(normalOutputStored_)return;
 outputBeforeDebug_=output_;
 measuredBeforeDebug_=measured_;
 outputOnBeforeDebug_=outputOn_;
 stateBeforeDebug_=state_;
 normalOutputStored_=true;
}

void ColorEngine::restoreNormalOutput(){
 if(normalOutputStored_){
  output_=outputBeforeDebug_;
  outputOn_=outputOnBeforeDebug_;
  measured_=measuredBeforeDebug_;
  state_=stateBeforeDebug_;
 }
 normalOutputStored_=false;
}

void ColorEngine::resetDarknessCutoff(){
 darknessCutoffActive_=false;
 darknessBelowSamples_=0;
 darknessAboveSamples_=0;
}

void ColorEngine::resetNormalAcceptance(uint32_t now){
 samples_.clear();
 candidate_={};
 candidateSince_=now;
 presenceSince_=0;
 absenceSince_=0;
 acceptedThisPresence_=false;
 previousPresence_=false;
}

void ColorEngine::stopDebugOutput(){
 if(!debugOutputActive_)return;
 restoreNormalOutput();
 samples_.clear();
 candidate_={};
 candidateSince_=millis();
 presenceSince_=0;
 acceptedThisPresence_=false;
 previousPresence_=false;
 absenceSince_=0;
 debugOutputActive_=false;
}

void ColorEngine::resetAveraging(){
 samples_.clear();
 candidate_={};
 candidateSince_=millis();
 presenceSince_=0;
 acceptedThisPresence_=false;
 previousPresence_=false;
 absenceSince_=0;
 resetDarknessCutoff();
}

void ColorEngine::resetColor(){
 samples_.clear();
 history_.clear();
 measured_={};
 candidate_={};
 output_={};
 outputOn_=false;
 vinylPresent_=false;
 previousPresence_=false;
 debugOutputActive_=false;
 acceptedThisPresence_=false;
 normalOutputStored_=false;
 outputBeforeDebug_={};
 measuredBeforeDebug_={};
 outputOnBeforeDebug_=false;
 stateBeforeDebug_="ColorReset";
 candidateSince_=millis();
 presenceSince_=0;
 absenceSince_=0;
 state_="ColorReset";
 resetDarknessCutoff();
}

void ColorEngine::update(RgbColor raw,bool present,const DebugOverrides&o,bool normalizeAcceptedOutput,
                         bool lightLevelAvailable,float lightLevelPercent,
                         bool contributingColorOverride){
 uint32_t now=millis();

 if(o.simulation!=SimulationMode::Off){
  rememberNormalOutput();
  switch(o.simulation){
   case SimulationMode::RandomColors:
    raw={(uint8_t)random(256),(uint8_t)random(256),(uint8_t)random(256)};
    break;
   case SimulationMode::Rainbow:
   case SimulationMode::ColorWheel:{
    uint8_t position=(now/20)%255;
    if(position<85)raw={(uint8_t)(255-position*3),(uint8_t)(position*3),0};
    else if(position<170){
     position-=85;
     raw={0,(uint8_t)(255-position*3),(uint8_t)(position*3)};
    }else{
     position-=170;
     raw={(uint8_t)(position*3),0,(uint8_t)(255-position*3)};
    }
    break;
   }
   case SimulationMode::Custom:raw=o.simulationColor;break;
   case SimulationMode::Marble:
    raw=marbleColor(
     o.simulationColor,
     constrain(o.marbleColors,(uint16_t)2,(uint16_t)255),
     constrain(o.marbleStrength,(uint8_t)0,(uint8_t)100),
     now
    );
    break;
   default:break;
  }
  samples_.clear();
  measured_=raw;
  candidate_=raw;
  candidateSince_=now;
  output_=raw;
  outputOn_=true;
  debugOutputActive_=true;
  absenceSince_=0;
  state_=o.simulation==SimulationMode::Rainbow||o.simulation==SimulationMode::ColorWheel
   ?"DebugRainbow":o.simulation==SimulationMode::Marble?"DebugMarble":"DebugSimulation";
  return;
 }

 if(o.colorEnabled&&o.averageOnly){
  bool changed=state_!="DebugColorOverride"||distance(output_,o.averageColor)>0;
  rememberNormalOutput();
  samples_.clear();
  measured_=o.averageColor;
  candidate_=measured_;
  candidateSince_=now;
  debugOutputActive_=true;
  absenceSince_=0;
  setOutput(measured_,true,"DebugColorOverride",changed);
  return;
 }

 // A real measurement after any direct debug output starts a completely new
 // presence and stability period; no stale pre-effect candidate is reused.
 if(debugOutputActive_){
  stopDebugOutput();
  vinylPresent_=false;
 }

 vinylPresent_=present;
 bool cutoffEligible=config_.vinyl.darknessCutoffEnabled&&!contributingColorOverride&&present&&
  lightLevelAvailable&&isfinite(lightLevelPercent);

 if(!cutoffEligible){
  bool wasActive=darknessCutoffActive_;
  resetDarknessCutoff();
  if(wasActive&&present)resetNormalAcceptance(now);
 }else{
  float threshold=constrain((float)config_.vinyl.darknessCutoffPercent,0.0F,100.0F);
  if(darknessCutoffActive_){
   float releaseThreshold=min(100.0F,threshold+max(1.0F,threshold*0.25F));
   if(lightLevelPercent>=releaseThreshold){
    if(darknessAboveSamples_<2)darknessAboveSamples_++;
   }else darknessAboveSamples_=0;
   darknessBelowSamples_=0;
   if(darknessAboveSamples_>=2){
    resetDarknessCutoff();
    resetNormalAcceptance(now);
   }
  }else{
   if(lightLevelPercent<threshold){
    if(darknessBelowSamples_<2)darknessBelowSamples_++;
   }else darknessBelowSamples_=0;
   darknessAboveSamples_=0;
   if(darknessBelowSamples_>=2){
    darknessCutoffActive_=true;
    darknessBelowSamples_=0;
   }
  }

  if(darknessCutoffActive_){
   measured_=raw;
   samples_.clear();
   candidate_={};
   candidateSince_=now;
   presenceSince_=0;
   absenceSince_=0;
   acceptedThisPresence_=false;
   previousPresence_=false;
   setOutput({},false,"DarknessCutoff");
   return;
  }
 }
 bool presenceChanged=present!=previousPresence_;
 if(presenceChanged){
  samples_.clear();
  candidate_=raw;
  candidateSince_=now;
  presenceSince_=present?now:0;
  acceptedThisPresence_=false;
 }

 if(present){
  samples_.push_back({now,raw,1});
  if(samples_.size()>MaxAveragingSamples)compactSamples();
  uint32_t keep=max<uint32_t>(windowMs(),1000)+1000;
  while(!samples_.empty()&&now-samples_.front().time>keep)samples_.pop_front();
  measured_=averageWindow(windowMs());
  absenceSince_=0;

 if(!previousPresence_||distance(measured_,candidate_)>config_.vinyl.colorChangeThreshold){
   candidate_=measured_;
   candidateSince_=now;
  }
  uint32_t holdMs=uint32_t(config_.vinyl.colorHoldSeconds*1000.0F);
  bool stable=now-candidateSince_>=holdMs;
  bool initialFallback=!acceptedThisPresence_&&!stable&&
   now-presenceSince_>=windowMs()+holdMs;
  if(stable||initialFallback){
   if(initialFallback){
    candidate_=measured_;
    candidateSince_=now-holdMs;
   }
   setOutput(normalizeAcceptedOutput?normalizeValue(candidate_,config_.vinyl.outputNormalizationStrength):candidate_,true,"VinylColor",true);
   acceptedThisPresence_=true;
  }else state_="WaitingForStableColor";
 }else{
  // Keep the current raw/background color visible on the dashboard, but never
  // feed it into a later vinyl averaging window.
  samples_.clear();
  measured_=raw;
  presenceSince_=0;
  acceptedThisPresence_=false;
  if(!config_.vinyl.presenceDetection){
   absenceSince_=0;
   state_="NoFreshSensorData";
   previousPresence_=false;
   return;
  }
  if(previousPresence_||absenceSince_==0)absenceSince_=now;
  uint32_t elapsed=now-absenceSince_;
  if(config_.vinyl.offEnabled&&elapsed>=uint32_t(config_.vinyl.offAfterSeconds)*1000)
   setOutput({},false,"Off");
  else if(config_.vinyl.defaultEnabled&&elapsed>=uint32_t(config_.vinyl.defaultAfterSeconds)*1000)
   setOutput(config_.vinyl.defaultColor,true,"DefaultColor");
  else state_="VinylRemoved";
 }
 previousPresence_=present;
}

uint32_t ColorEngine::holdRemainingMs()const{
 if(!vinylPresent_||state_!="WaitingForStableColor")return 0;
 uint32_t duration=uint32_t(config_.vinyl.colorHoldSeconds*1000.0F);
 uint32_t elapsed=millis()-candidateSince_;
 uint32_t stableRemaining=elapsed>=duration?0:duration-elapsed;
 if(!acceptedThisPresence_&&!previousPresence_)return stableRemaining;
 if(acceptedThisPresence_)return stableRemaining;
 uint32_t initialDuration=windowMs()+duration;
 uint32_t initialElapsed=millis()-presenceSince_;
 uint32_t initialRemaining=initialElapsed>=initialDuration?0:initialDuration-initialElapsed;
 return min(stableRemaining,initialRemaining);
}

uint32_t ColorEngine::defaultRemainingMs()const{
 if(!config_.vinyl.presenceDetection||!config_.vinyl.defaultEnabled||vinylPresent_||!absenceSince_)return 0;
 uint32_t duration=config_.vinyl.defaultAfterSeconds*1000UL;
 uint32_t elapsed=millis()-absenceSince_;
 return elapsed>=duration?0:duration-elapsed;
}

uint32_t ColorEngine::offRemainingMs()const{
 if(!config_.vinyl.presenceDetection||!config_.vinyl.offEnabled||vinylPresent_||!absenceSince_)return 0;
 uint32_t duration=config_.vinyl.offAfterSeconds*1000UL;
 uint32_t elapsed=millis()-absenceSince_;
 return elapsed>=duration?0:duration-elapsed;
}

bool ColorEngine::chooseHistory(size_t index,DebugOverrides&o){
 if(index>=history_.size())return false;
 RgbColor selected=history_[index];
 rememberNormalOutput();
 o.simulation=SimulationMode::Off;
 o.colorEnabled=true;
 o.averageOnly=true;
 o.averageColor=selected;
 o.sensorColorEnabled.fill(false);
 samples_.clear();
 measured_=selected;
 candidate_=selected;
 candidateSince_=millis();
 debugOutputActive_=true;
 absenceSince_=0;
 setOutput(selected,true,"HistoryColor");
 return true;
}
