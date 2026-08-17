#pragma once
#include <deque>
#include "AppConfig.h"
#include "Types.h"
namespace VinylChroma {
class ColorEngine {
 public:
  explicit ColorEngine(AppConfig&c):config_(c){}
  void update(RgbColor,bool,const DebugOverrides&,bool normalizeAcceptedOutput=false,
              bool lightLevelAvailable=false,float lightLevelPercent=0.0F,
              bool contributingColorOverride=false);
  void resetColor();
  void resetAveraging();
  void stopDebugOutput();
  RgbColor measured()const{return measured_;}
  RgbColor output()const{return output_;}
  bool outputOn()const{return outputOn_;}
  bool vinylPresent()const{return vinylPresent_;}
  bool automaticOffActive()const{return state_=="Off";}
  bool darknessCutoffActive()const{return darknessCutoffActive_&&!debugOutputActive_;}
  String stateName()const{return state_;}
  uint32_t holdRemainingMs()const;
  uint32_t defaultRemainingMs()const;
  uint32_t offRemainingMs()const;
  const std::deque<RgbColor>& history()const{return history_;}
  bool chooseHistory(size_t,DebugOverrides&);

 private:
  struct Sample{uint32_t time;RgbColor color;uint32_t weight;};
  static constexpr size_t MaxAveragingSamples=1024;
  AppConfig&config_;
  std::deque<Sample>samples_;
  std::deque<RgbColor>history_;
  RgbColor measured_{},candidate_{},output_{};
  bool outputOn_{false},vinylPresent_{false},previousPresence_{false},debugOutputActive_{false},acceptedThisPresence_{false};
  bool darknessCutoffActive_{false};
  uint8_t darknessBelowSamples_{0},darknessAboveSamples_{0};
  RgbColor outputBeforeDebug_{},measuredBeforeDebug_{};
  bool outputOnBeforeDebug_{false},normalOutputStored_{false};
  String stateBeforeDebug_{"Boot"};
  uint32_t candidateSince_{0},presenceSince_{0},absenceSince_{0};
  String state_{"Boot"};
  RgbColor averageWindow(uint32_t)const;
  uint32_t windowMs()const;
  void compactSamples();
  static uint16_t distance(RgbColor,RgbColor);
  void recordHistory(RgbColor);
  void setOutput(RgbColor,bool,const String&,bool addToHistory=false);
  void rememberNormalOutput();
  void restoreNormalOutput();
  void resetDarknessCutoff();
  void resetNormalAcceptance(uint32_t);
};
}
