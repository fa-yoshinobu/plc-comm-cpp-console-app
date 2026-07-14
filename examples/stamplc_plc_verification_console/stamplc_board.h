#pragma once

#include <Arduino.h>

namespace stamplc_board {

constexpr char kModelName[] = "m5stack-stamplc";
constexpr char kDisplayName[] = "M5Stack StamPLC";
constexpr bool kHasOnboardRs232 = false;
constexpr bool kHasExternalRtsCts = false;
constexpr bool kUseHardwareRs485HalfDuplex = true;

// M5Stack M5StamPLC 1.2.0 src/pin_config.h.
constexpr int kRs485TxPin = 0;
constexpr int kRs485RxPin = 39;
constexpr int kRs485DirectionPin = 46;
constexpr int kRs232TxPin = -1;
constexpr int kRs232RxPin = -1;
constexpr int kKeyPin = -1;

// Hardware-managed UART_MODE_RS485_HALF_DUPLEX owns the direction signal.
// These values keep the shared manual-direction helpers well-formed; the
// StamPLC build never writes them directly.
constexpr uint8_t kRs485TxEnable = HIGH;
constexpr uint8_t kRs485RxEnable = LOW;

constexpr uint32_t kUsbConsoleBaud = 115200U;
constexpr uint32_t kDefaultMcBaud = 19200U;
constexpr char kDefaultSlmpHost[] = "192.168.250.100";
constexpr uint16_t kDefaultSlmpTcpPort = 1025U;
constexpr uint16_t kDefaultSlmpUdpPort = 1035U;

constexpr bool isExternalGpioAllowed(int) {
    return false;
}

}  // namespace stamplc_board
