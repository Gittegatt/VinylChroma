#pragma once
#include <Arduino.h>
#include <deque>
#include "Types.h"
namespace VinylChroma {
class Logger { public: void setLevel(LogLevel l){level_=l;} void log(LogLevel,const String&); String text()const; void clear(){lines_.clear();} private: LogLevel level_{LogLevel::Info}; std::deque<String> lines_; };
}
