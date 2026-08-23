#pragma once

#include <Arduino.h>
#include <array>

// Exactly one VinylChroma board macro is supplied by the selected PlatformIO
// environment. Keeping the electrical profile compile-time prevents a WebGUI
// setting or imported backup from selecting pins for a different PCB.
#if (defined(VINYLCHROMA_BOARD_ESP32_S3_SUPERMINI) + \
     defined(VINYLCHROMA_BOARD_ESP32_S3_ZERO) + \
     defined(VINYLCHROMA_BOARD_ESP32_C3_SUPERMINI) + \
     defined(VINYLCHROMA_BOARD_ESP32_C3_ZERO) + \
     defined(VINYLCHROMA_BOARD_SEEED_XIAO_ESP32_S3) + \
     defined(VINYLCHROMA_BOARD_ADAFRUIT_QTPY_ESP32_S3_N4R2) + \
     defined(VINYLCHROMA_BOARD_ADAFRUIT_QTPY_ESP32_S3_NOPSRAM) + \
     defined(VINYLCHROMA_BOARD_ESP32_S3_TINY)) != 1
#error "Select exactly one supported VinylChroma board environment."
#endif

namespace VinylChroma::BoardProfile {

#if defined(VINYLCHROMA_BOARD_ESP32_S3_SUPERMINI)
inline constexpr char Id[]="esp32-s3-supermini";
inline constexpr char Name[]="ESP32-S3 Super Mini";
inline constexpr uint8_t I2cSdaPin=12,I2cSclPin=13;
inline constexpr std::array<uint8_t,4> SensorLedPins{11,10,9,8};
inline constexpr std::array<uint8_t,18> AllowedGpios{1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,21};
inline constexpr char GpioNotes[]="GPIO 0, 3, 45, and 46 are boot-strapping pins; GPIO 19 and 20 are reserved for native USB.";

#elif defined(VINYLCHROMA_BOARD_ESP32_S3_ZERO)
inline constexpr char Id[]="esp32-s3-zero";
inline constexpr char Name[]="Waveshare ESP32-S3 Zero (4 MB / 2 MB)";
inline constexpr uint8_t I2cSdaPin=12,I2cSclPin=13;
inline constexpr std::array<uint8_t,4> SensorLedPins{11,10,9,8};
inline constexpr std::array<uint8_t,19> AllowedGpios{1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,43,44};
inline constexpr char GpioNotes[]="GPIO 21 drives the onboard RGB LED; GPIO 19 and 20 are native USB; boot-strapping and memory pins are excluded.";

#elif defined(VINYLCHROMA_BOARD_ESP32_C3_SUPERMINI)
inline constexpr char Id[]="esp32-c3-supermini";
inline constexpr char Name[]="ESP32-C3 Super Mini";
inline constexpr uint8_t I2cSdaPin=4,I2cSclPin=5;
inline constexpr std::array<uint8_t,4> SensorLedPins{6,7,10,3};
inline constexpr std::array<uint8_t,10> AllowedGpios{0,1,3,4,5,6,7,10,20,21};
inline constexpr char GpioNotes[]="GPIO 2, 8, and 9 are boot-strapping pins; GPIO 8 is commonly the onboard LED; GPIO 18 and 19 are native USB.";

#elif defined(VINYLCHROMA_BOARD_ESP32_C3_ZERO)
inline constexpr char Id[]="esp32-c3-zero";
inline constexpr char Name[]="Waveshare ESP32-C3 Zero";
inline constexpr uint8_t I2cSdaPin=4,I2cSclPin=5;
inline constexpr std::array<uint8_t,4> SensorLedPins{6,7,3,1};
inline constexpr std::array<uint8_t,9> AllowedGpios{0,1,3,4,5,6,7,20,21};
inline constexpr char GpioNotes[]="GPIO 10 drives the onboard RGB LED; GPIO 2, 8, and 9 are boot-strapping pins; GPIO 12-17 are flash and GPIO 18/19 are native USB.";

#elif defined(VINYLCHROMA_BOARD_SEEED_XIAO_ESP32_S3)
inline constexpr char Id[]="seeed-xiao-esp32-s3";
inline constexpr char Name[]="Seeed Studio XIAO ESP32-S3";
inline constexpr uint8_t I2cSdaPin=5,I2cSclPin=6;
inline constexpr std::array<uint8_t,4> SensorLedPins{1,2,4,7};
inline constexpr std::array<uint8_t,10> AllowedGpios{1,2,4,5,6,7,8,9,43,44};
inline constexpr char GpioNotes[]="Only exposed XIAO pins are listed. GPIO 3 is a boot-strapping pin; GPIO 19/20 are native USB and GPIO 21 is the onboard LED.";

#elif defined(VINYLCHROMA_BOARD_ADAFRUIT_QTPY_ESP32_S3_N4R2)
inline constexpr char Id[]="adafruit-qtpy-esp32-s3-n4r2";
inline constexpr char Name[]="Adafruit QT Py ESP32-S3 N4R2";
inline constexpr uint8_t I2cSdaPin=7,I2cSclPin=6;
inline constexpr std::array<uint8_t,4> SensorLedPins{18,17,9,8};
inline constexpr std::array<uint8_t,14> AllowedGpios{5,6,7,8,9,16,17,18,35,36,37,40,41,42};
inline constexpr char GpioNotes[]="GPIO 38 powers and GPIO 39 drives the onboard NeoPixel. Boot-strapping, native-USB, and memory pins are excluded.";

#elif defined(VINYLCHROMA_BOARD_ADAFRUIT_QTPY_ESP32_S3_NOPSRAM)
inline constexpr char Id[]="adafruit-qtpy-esp32-s3-nopsram";
inline constexpr char Name[]="Adafruit QT Py ESP32-S3 No PSRAM";
inline constexpr uint8_t I2cSdaPin=7,I2cSclPin=6;
inline constexpr std::array<uint8_t,4> SensorLedPins{18,17,9,8};
inline constexpr std::array<uint8_t,14> AllowedGpios{5,6,7,8,9,16,17,18,35,36,37,40,41,42};
inline constexpr char GpioNotes[]="GPIO 38 powers and GPIO 39 drives the onboard NeoPixel. Boot-strapping and native-USB pins are excluded.";

#elif defined(VINYLCHROMA_BOARD_ESP32_S3_TINY)
inline constexpr char Id[]="esp32-s3-tiny";
inline constexpr char Name[]="Waveshare ESP32-S3 Tiny (4 MB / 2 MB)";
inline constexpr uint8_t I2cSdaPin=12,I2cSclPin=13;
inline constexpr std::array<uint8_t,4> SensorLedPins{11,10,9,8};
inline constexpr std::array<uint8_t,17> AllowedGpios{1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18};
inline constexpr char GpioNotes[]="The conservative list excludes boot-strapping, native-USB, memory, and high-numbered board-resource pins.";
#endif

inline constexpr bool isUsableGpio(uint8_t pin){
 for(uint8_t allowed:AllowedGpios)if(pin==allowed)return true;
 return false;
}

inline constexpr bool defaultGpiosAreValid(){
 std::array<uint8_t,6> pins{I2cSdaPin,I2cSclPin,SensorLedPins[0],SensorLedPins[1],SensorLedPins[2],SensorLedPins[3]};
 for(size_t i=0;i<pins.size();i++){
  if(!isUsableGpio(pins[i]))return false;
  for(size_t previous=0;previous<i;previous++)if(pins[i]==pins[previous])return false;
 }
 return true;
}

static_assert(defaultGpiosAreValid(),"VinylChroma board defaults must be unique and included in the board GPIO allowlist.");

inline constexpr uint8_t minimumGpio(){
 uint8_t result=AllowedGpios[0];
 for(uint8_t pin:AllowedGpios)if(pin<result)result=pin;
 return result;
}

inline constexpr uint8_t maximumGpio(){
 uint8_t result=AllowedGpios[0];
 for(uint8_t pin:AllowedGpios)if(pin>result)result=pin;
 return result;
}

}
