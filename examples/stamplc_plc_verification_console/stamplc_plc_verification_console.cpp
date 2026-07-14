#include "stamplc_plc_verification_console.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "stamplc_board.h"
#include "stamplc_protocol_console.h"

namespace stamplc_plc_verification_console {
namespace {

constexpr uint32_t kRefreshIntervalMs = 500U;
constexpr uint8_t kPageCount = 3U;

uint8_t g_page = 0U;
uint32_t g_last_refresh_ms = 0U;
uint8_t g_last_status_color = 0xFFU;

void configureStatusLight() {
    auto& io = M5.getIOExpander(0);
    for (uint8_t pin = 4U; pin <= 6U; ++pin) {
        io.setDirection(pin, true);
        io.setPullMode(pin, false);
        io.setHighImpedance(pin, true);
    }
}

void setStatusLight(bool red, bool green, bool blue) {
    auto& io = M5.getIOExpander(0);
    const bool values[] = {blue, green, red};
    for (uint8_t index = 0U; index < 3U; ++index) {
        const uint8_t pin = static_cast<uint8_t>(4U + index);
        if (values[index]) {
            io.setHighImpedance(pin, false);
            io.digitalWrite(pin, false);
        } else {
            io.setHighImpedance(pin, true);
        }
    }
}

void updateStatusLight(const stamplc_protocol_console::ConsoleSnapshot& status) {
    uint8_t color = 0U;
    if (!status.mc_ready || status.mc_reset_required) {
        color = 1U;  // red
    } else if (status.mc_busy) {
        color = 2U;  // yellow
    } else if (status.wifi_connected) {
        color = 3U;  // green
    } else {
        color = 4U;  // blue
    }
    if (color == g_last_status_color) {
        return;
    }
    g_last_status_color = color;
    setStatusLight(
        color == 1U || color == 2U,
        color == 2U || color == 3U,
        color == 4U);
}

void drawHeader(const char* title) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.fillRect(0, 0, M5.Display.width(), 18, TFT_DARKCYAN);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    M5.Display.setCursor(5, 5);
    M5.Display.print(title);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

void drawFooter() {
    const int32_t y = M5.Display.height() - 14;
    M5.Display.fillRect(0, y, M5.Display.width(), 14, TFT_DARKGREY);
    M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    M5.Display.setCursor(5, y + 3);
    M5.Display.print("A:prev  B:status  C:next");
}

void drawStatusPage(const stamplc_protocol_console::ConsoleSnapshot& status) {
    drawHeader("PLC COMM / STATUS");
    M5.Display.setCursor(5, 23);
    M5.Display.printf("WiFi: %s\n", status.wifi_connected ? "CONNECTED" : "DISCONNECTED");
    if (status.wifi_connected) {
        const IPAddress address = WiFi.localIP();
        M5.Display.printf("IP: %u.%u.%u.%u\n", address[0], address[1], address[2], address[3]);
    } else {
        M5.Display.println("IP: -");
    }
    M5.Display.printf("SLMP: %.26s\n", status.slmp_profile);
    M5.Display.printf("Host: %.20s:%u\n", status.slmp_host, status.slmp_tcp_port);
    M5.Display.printf("MC: %.24s\n", status.mc_profile);
    M5.Display.printf("Serial: %lu/%u%c%u\n",
                      static_cast<unsigned long>(status.mc_baud),
                      status.mc_data_bits,
                      status.mc_parity,
                      status.mc_stop_bits);
    M5.Display.printf("UART: %s%s\n",
                      status.mc_ready ? "READY" : "NOT READY",
                      status.mc_busy ? " / BUSY" : "");
    drawFooter();
}

void drawBoardPage() {
    drawHeader("PLC COMM / STAMPLC");
    M5.Display.setCursor(5, 23);
    M5.Display.printf("Chip: %s rev %u\n", ESP.getChipModel(), ESP.getChipRevision());
    M5.Display.printf("Flash: %lu bytes\n", static_cast<unsigned long>(ESP.getFlashChipSize()));
    M5.Display.printf("PSRAM: %lu bytes\n", static_cast<unsigned long>(ESP.getPsramSize()));
    M5.Display.printf("Heap: %lu bytes\n", static_cast<unsigned long>(ESP.getFreeHeap()));
    M5.Display.println("RS-485: UART1 hardware DE");
    M5.Display.println("RGB: runtime state");
    M5.Display.println("Boot: no PLC traffic");
    drawFooter();
}

void drawControlsPage() {
    drawHeader("PLC COMM / CONTROLS");
    M5.Display.setCursor(5, 23);
    M5.Display.println("USB serial owns commands.");
    M5.Display.println("A: previous LCD page");
    M5.Display.println("B: print status to USB");
    M5.Display.println("C: next LCD page");
    M5.Display.println();
    M5.Display.println("RGB blue: ready / offline");
    M5.Display.println("green: WiFi connected");
    M5.Display.println("yellow: MC request active");
    M5.Display.println("red: UART/reset error");
    drawFooter();
}

void drawCurrentPage(const stamplc_protocol_console::ConsoleSnapshot& status) {
    M5.Display.startWrite();
    switch (g_page) {
        case 0U:
            drawStatusPage(status);
            break;
        case 1U:
            drawBoardPage();
            break;
        default:
            drawControlsPage();
            break;
    }
    M5.Display.endWrite();
}

void beginBoardUi() {
    auto config = M5.config();
    config.serial_baudrate = 0U;
    config.internal_imu = false;
    config.internal_rtc = false;
    config.internal_mic = false;
    config.internal_spk = false;
    config.external_imu = false;
    config.external_rtc = false;
    config.external_display_value = 0U;
    config.fallback_board = m5::board_t::board_M5StampPLC;
    M5.begin(config);

    // M5Unified briefly treats GPIO46 as a generic ESP32-S3 hold pin before
    // board detection. Return the StamPLC transceiver to receive mode until
    // the console configures UART1 hardware half-duplex.
    pinMode(stamplc_board::kRs485DirectionPin, OUTPUT);
    digitalWrite(stamplc_board::kRs485DirectionPin, LOW);

    configureStatusLight();
    setStatusLight(false, false, true);
    M5.Display.setRotation(1);
    M5.Display.setTextWrap(false);
}

}  // namespace

void setupConsole() {
    beginBoardUi();
    stamplc_protocol_console::setupConsole();
    const auto status = stamplc_protocol_console::snapshot();
    updateStatusLight(status);
    drawCurrentPage(status);
    g_last_refresh_ms = millis();
}

void loopConsole() {
    stamplc_protocol_console::loopConsole();
    M5.update();

    bool redraw = false;
    if (M5.BtnA.wasClicked()) {
        g_page = static_cast<uint8_t>((g_page + kPageCount - 1U) % kPageCount);
        redraw = true;
    }
    if (M5.BtnB.wasClicked()) {
        stamplc_protocol_console::printConsoleStatus();
        redraw = true;
    }
    if (M5.BtnC.wasClicked()) {
        g_page = static_cast<uint8_t>((g_page + 1U) % kPageCount);
        redraw = true;
    }

    const auto status = stamplc_protocol_console::snapshot();
    updateStatusLight(status);
    const uint32_t now = millis();
    if (redraw || static_cast<uint32_t>(now - g_last_refresh_ms) >= kRefreshIntervalMs) {
        drawCurrentPage(status);
        g_last_refresh_ms = now;
    }
}

}  // namespace stamplc_plc_verification_console
