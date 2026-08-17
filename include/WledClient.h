#pragma once
#include <HTTPClient.h>
#include "AppConfig.h"
#include "Logger.h"
namespace VinylChroma {
class WledClient {
 public:
  WledClient(AppConfig&c,Logger&l):config_(c),logger_(l){}
  void update(RgbColor,bool,bool force=false,bool releaseAfterSend=false,uint16_t updateIntervalCeilingMs=0);
  void cancelRelease(){if(releaseMode_)resumePending_=true;releaseMode_=false;releaseComplete_=false;}
  bool test(RgbColor);
  bool enabled()const{return config_.wled.enabled;}
  bool released()const{return releaseMode_&&releaseComplete_;}
  bool statusKnown()const;
  bool reachable()const{return enabled()&&statusKnown()&&reachable_;}
  int lastCode()const{return enabled()&&statusKnown()?lastCode_:0;}
  uint32_t lastLatency()const{return enabled()&&statusKnown()?latency_:0;}
  uint32_t statusAgeMs()const;
  uint8_t consecutiveFailures()const{return enabled()?consecutiveFailures_:0;}
  uint32_t retryRemainingMs()const;

 private:
  enum class FailureKind:uint8_t {None,HostMissing,WifiDisconnected,BeginFailed,Http};
  static constexpr uint32_t HeartbeatIntervalMs=30000;
  static constexpr uint32_t StatusStaleMs=90000;
  AppConfig&config_;
  Logger&logger_;
  uint32_t lastAttempt_{0},lastSuccess_{0},statusAt_{0},retryDelayMs_{0};
  RgbColor last_{};
  String lastHost_;
  uint16_t lastPort_{0};
  uint8_t lastSegment_{0},lastBrightness_{0},consecutiveFailures_{0};
  bool lastOn_{false},lastEnabled_{false},lastSendBrightness_{true},lastKeepSelectedEffect_{false};
  bool hasLastState_{false},hasAttempt_{false},statusValid_{false},reachable_{false};
  bool releaseMode_{false},releaseComplete_{false},resumePending_{false};
  int lastCode_{0};
  uint32_t latency_{0};
  FailureKind failureKind_{FailureKind::None};
  bool send(RgbColor,bool);
  bool attempt(RgbColor,bool);
  bool stateMatches(RgbColor,bool)const;
  void rememberState(RgbColor,bool);
  void recordResult(bool);
  void clearDisabledStatus();
  static uint32_t backoffFor(uint8_t);
};
}
