#pragma once

#include <Arduino.h>

namespace t_rss3_board {

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

}  // namespace t_rss3_board
