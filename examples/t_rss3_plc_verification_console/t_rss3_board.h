#pragma once

#include <Arduino.h>

namespace t_rss3_board {

constexpr char kModelName[] = "t-rss3";
constexpr char kDisplayName[] = "T-RSS3 ESP32-S3";
constexpr bool kHasOnboardRs232 = true;
constexpr bool kHasExternalRtsCts = true;
constexpr bool kUseHardwareRs485HalfDuplex = false;

constexpr int kRs485RxPin = 2;
constexpr int kRs485TxPin = 3;
constexpr int kRs485DirectionPin = 4;
constexpr int kKeyPin = 5;
constexpr int kRs232TxPin = 41;
constexpr int kRs232RxPin = 42;
constexpr int kLedPin = 1;
constexpr int kBootPin = 0;

constexpr uint8_t kRs485TxEnable = HIGH;
constexpr uint8_t kRs485RxEnable = LOW;

constexpr uint32_t kUsbConsoleBaud = 115200U;
constexpr uint32_t kDefaultMcBaud = 19200U;
constexpr char kDefaultSlmpHost[] = "192.168.250.100";
constexpr uint16_t kDefaultSlmpTcpPort = 1025U;
constexpr uint16_t kDefaultSlmpUdpPort = 1035U;

constexpr bool isExternalGpioAllowed(int value) {
    return (value >= 6 && value <= 18) || value == 21 ||
           (value >= 33 && value <= 40) || value == 43 || value == 44 ||
           value == 47 || value == 48;
}

}  // namespace t_rss3_board
