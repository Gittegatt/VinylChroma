#include "Logger.h"
using namespace VinylChroma;
void Logger::log(LogLevel l,const String&m){if(l==LogLevel::Off||int(l)>int(level_))return;String s=String(millis()/1000)+"s "+m;Serial.println(s);lines_.push_back(s);while(lines_.size()>80)lines_.pop_front();}
String Logger::text()const{String s;for(auto&l:lines_){s+=l;s+='\n';}return s;}
