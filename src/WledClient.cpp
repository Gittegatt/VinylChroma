#include "WledClient.h"
#include <ArduinoJson.h>
using namespace VinylChroma;
bool WledClient::send(RgbColor c,bool on){
 lastCode_=0;latency_=0;reachable_=false;failureKind_=FailureKind::None;
 if(config_.wled.host.isEmpty()){failureKind_=FailureKind::HostMissing;return false;}
 if(WiFi.status()!=WL_CONNECTED){failureKind_=FailureKind::WifiDisconnected;return false;}
 HTTPClient h;String url="http://"+config_.wled.host+":"+String(config_.wled.port)+"/json/state";
 if(!h.begin(url)){lastCode_=HTTPC_ERROR_CONNECTION_REFUSED;failureKind_=FailureKind::BeginFailed;return false;}
 h.setConnectTimeout(250);h.setTimeout(500);
 h.addHeader("Content-Type","application/json");
 JsonDocument d;d["on"]=on;
 if(on){
  if(config_.wled.sendBrightness)d["bri"]=config_.wled.brightness;
  auto segments=d["seg"].to<JsonArray>();
  auto segment=segments.add<JsonObject>();segment["id"]=config_.wled.segment;segment["on"]=true;
  if(!config_.wled.keepSelectedEffect)segment["fx"]=0;
  auto colors=segment["col"].to<JsonArray>();
  auto rgb=colors.add<JsonArray>();rgb.add(c.red);rgb.add(c.green);rgb.add(c.blue);
 }
 String body;serializeJson(d,body);
 uint32_t st=millis();lastCode_=h.POST(body);latency_=millis()-st;reachable_=lastCode_>=200&&lastCode_<300;
 if(!reachable_)failureKind_=FailureKind::Http;
 h.end();return reachable_;
}
bool WledClient::stateMatches(RgbColor c,bool on)const{
 if(!hasLastState_||lastEnabled_!=config_.wled.enabled||lastHost_!=config_.wled.host||lastPort_!=config_.wled.port||lastOn_!=on)return false;
 if(!on)return true;
 if(lastSegment_!=config_.wled.segment||lastSendBrightness_!=config_.wled.sendBrightness||
    lastKeepSelectedEffect_!=config_.wled.keepSelectedEffect||
    (config_.wled.sendBrightness&&lastBrightness_!=config_.wled.brightness))return false;
 return last_.red==c.red&&last_.green==c.green&&last_.blue==c.blue;
}
void WledClient::rememberState(RgbColor c,bool on){
 last_=c;lastOn_=on;lastEnabled_=config_.wled.enabled;lastHost_=config_.wled.host;lastPort_=config_.wled.port;
 lastSegment_=config_.wled.segment;lastBrightness_=config_.wled.brightness;lastSendBrightness_=config_.wled.sendBrightness;
 lastKeepSelectedEffect_=config_.wled.keepSelectedEffect;hasLastState_=true;
}
uint32_t WledClient::backoffFor(uint8_t failures){
 if(failures<=1)return 1000;
 if(failures==2)return 2000;
 if(failures==3)return 5000;
 if(failures==4)return 10000;
 return 30000;
}
uint32_t WledClient::statusAgeMs()const{
 if(!enabled()||!statusValid_)return UINT32_MAX;
 return millis()-statusAt_;
}
bool WledClient::statusKnown()const{return enabled()&&statusValid_&&millis()-statusAt_<=StatusStaleMs;}
uint32_t WledClient::retryRemainingMs()const{
 if(!enabled()||consecutiveFailures_==0||!statusValid_)return 0;
 uint32_t elapsed=millis()-statusAt_;
 return elapsed>=retryDelayMs_?0:retryDelayMs_-elapsed;
}
void WledClient::clearDisabledStatus(){
 lastEnabled_=false;reachable_=false;statusValid_=false;lastCode_=0;latency_=0;
 consecutiveFailures_=0;retryDelayMs_=0;failureKind_=FailureKind::None;
}
void WledClient::recordResult(bool ok){
 statusAt_=millis();statusValid_=true;
 if(ok){
  if(consecutiveFailures_>0)logger_.log(LogLevel::Info,"WLED connection recovered");
  consecutiveFailures_=0;retryDelayMs_=0;failureKind_=FailureKind::None;lastSuccess_=statusAt_;
  return;
 }
 uint32_t previousDelay=retryDelayMs_;
 if(consecutiveFailures_<255)consecutiveFailures_++;
 retryDelayMs_=backoffFor(consecutiveFailures_);
 if(consecutiveFailures_!=1&&retryDelayMs_==previousDelay)return;
 String reason;
 if(failureKind_==FailureKind::HostMissing)reason="host is empty";
 else if(failureKind_==FailureKind::WifiDisconnected)reason="Wi-Fi is disconnected";
 else if(failureKind_==FailureKind::BeginFailed)reason="HTTP client initialization failed";
 else reason="HTTP "+String(lastCode_);
 logger_.log(LogLevel::Error,"WLED request failed ("+reason+"); retry in "+String((retryDelayMs_+999)/1000)+" s");
}
bool WledClient::attempt(RgbColor c,bool on){
 lastAttempt_=millis();hasAttempt_=true;
 bool ok=send(c,on);recordResult(ok);
 if(ok)rememberState(c,on);
 return ok;
}
void WledClient::update(RgbColor c,bool on,bool force,bool releaseAfterSend,uint16_t updateIntervalCeilingMs){
 bool enteringRelease=releaseAfterSend&&!releaseMode_;
 bool leavingRelease=!releaseAfterSend&&releaseMode_;
 bool resumingControl=!releaseAfterSend&&resumePending_;
 if(!releaseAfterSend){
  releaseMode_=false;
  releaseComplete_=false;
  resumePending_=false;
 }else{
  resumePending_=false;
  if(enteringRelease||force)releaseComplete_=false;
 }
 releaseMode_=releaseAfterSend;
 if(!config_.wled.enabled){
  clearDisabledStatus();
  releaseMode_=false;
  releaseComplete_=false;
  resumePending_=false;
  return;
 }
 if(releaseAfterSend&&releaseComplete_&&!force)return;
 uint32_t now=millis();
 bool sendNow=force||enteringRelease||leavingRelease||resumingControl;
 if(!sendNow&&consecutiveFailures_>0&&now-statusAt_<retryDelayMs_)return;
 bool retryDue=consecutiveFailures_>0;
 bool heartbeatDue=!releaseAfterSend&&hasLastState_&&now-lastSuccess_>=HeartbeatIntervalMs;
 if(!sendNow&&stateMatches(c,on)&&!retryDue&&!heartbeatDue)return;
 uint32_t updateIntervalMs=config_.wled.updateIntervalMs;
 if(updateIntervalCeilingMs>0)updateIntervalMs=min<uint32_t>(updateIntervalMs,updateIntervalCeilingMs);
 if(!sendNow&&!retryDue&&!heartbeatDue&&hasAttempt_&&now-lastAttempt_<updateIntervalMs)return;
 bool ok=attempt(c,on);
 if(ok&&releaseAfterSend){
  releaseComplete_=true;
  logger_.log(LogLevel::Info,"WLED switched off; automatic control released");
 }
}
bool WledClient::test(RgbColor c){return attempt(c,true);}
