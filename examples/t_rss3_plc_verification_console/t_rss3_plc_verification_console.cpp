#include "t_rss3_plc_verification_console.h"

#include "t_rss3_board.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ctype.h>
#include <driver/uart.h>
#include <errno.h>
#include <freertos/FreeRTOS.h>
#include <lwip/sockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcprotocol/serial/compat/cstddef.hpp"
#include "mcprotocol/serial/compat/cstdint.hpp"
#include "mcprotocol_serial.hpp"
#include "mcprotocol/serial/span_compat.hpp"
#include "mcprotocol/serial/version.hpp"
#include <slmp_arduino_transport.h>

#ifndef T_RSS3_SLMP_HAS_MXR_RJ71_PROFILE
#define T_RSS3_SLMP_HAS_MXR_RJ71_PROFILE 0
#endif

#ifndef T_RSS3_SLMP_HAS_TRAFFIC_STATS
#define T_RSS3_SLMP_HAS_TRAFFIC_STATS 0
#endif

#ifndef T_RSS3_BUILD_ENV
#define T_RSS3_BUILD_ENV "unavailable"
#endif

#ifndef T_RSS3_FIRMWARE_BUILD_ID
#define T_RSS3_FIRMWARE_BUILD_ID "unavailable"
#endif

#ifndef T_RSS3_PIO_CORE_VERSION
#define T_RSS3_PIO_CORE_VERSION "unavailable"
#endif

#ifndef T_RSS3_APP_SOURCE_ID
#define T_RSS3_APP_SOURCE_ID "unavailable"
#endif

#ifndef T_RSS3_SLMP_SOURCE_ID
#define T_RSS3_SLMP_SOURCE_ID "unavailable"
#endif

#ifndef T_RSS3_MC_SOURCE_ID
#define T_RSS3_MC_SOURCE_ID "unavailable"
#endif

namespace t_rss3_plc_verification_console {
namespace {

constexpr size_t kCommandCapacity = 256U;
constexpr size_t kMaxTokens = 16U;
constexpr size_t kMaxUsbBytesPerLoop = 64U;
constexpr size_t kSlmpBufferCapacity = 512U;
constexpr size_t kMcUartBufferCapacity = 1024U;
constexpr size_t kMcMaxFiniteWriteFrameBytes = 256U;
constexpr size_t kMcMaxReadPoints = 8U;
constexpr uint32_t kDefaultSlmpTimeoutMs = 3000U;
constexpr uint32_t kDefaultKeepAliveTestIdleMs = 35000U;
constexpr uint32_t kMcTxCompletionTimeoutMs = 3000U;
constexpr slmp::TargetAddress kSlmpTarget = {
    0x00U,
    0xFFU,
    0x03FFU,
    0x00U,
};

char g_command[kCommandCapacity] = {};
size_t g_command_length = 0U;
bool g_last_was_cr = false;
bool g_discard_command_until_eol = false;

char g_slmp_host[48] = {};
uint16_t g_slmp_tcp_port = t_rss3_board::kDefaultSlmpTcpPort;
uint16_t g_slmp_udp_port = t_rss3_board::kDefaultSlmpUdpPort;
slmp::PlcProfile g_slmp_profile = slmp::PlcProfile::IqR;

WiFiClient g_slmp_tcp_socket;
slmp::ArduinoClientTransport g_slmp_tcp_transport(
    g_slmp_tcp_socket,
    slmp::configureEsp32WifiClientKeepAlive);
WiFiUDP g_slmp_udp_socket;
slmp::ArduinoUdpTransport g_slmp_udp_transport(g_slmp_udp_socket, 0U);
uint8_t g_slmp_tcp_tx[kSlmpBufferCapacity] = {};
uint8_t g_slmp_tcp_rx[kSlmpBufferCapacity] = {};
uint8_t g_slmp_udp_tx[kSlmpBufferCapacity] = {};
uint8_t g_slmp_udp_rx[kSlmpBufferCapacity] = {};
slmp::SlmpClient g_slmp_tcp_client(
    g_slmp_tcp_transport,
    slmp::PlcProfile::IqR,
    kSlmpTarget,
    g_slmp_tcp_tx,
    sizeof(g_slmp_tcp_tx),
    g_slmp_tcp_rx,
    sizeof(g_slmp_tcp_rx));
slmp::SlmpClient g_slmp_udp_client(
    g_slmp_udp_transport,
    slmp::PlcProfile::IqR,
    kSlmpTarget,
    g_slmp_udp_tx,
    sizeof(g_slmp_udp_tx),
    g_slmp_udp_rx,
    sizeof(g_slmp_udp_rx));

enum class McInterface : uint8_t {
    Rs232,
    Rs485,
    ExternalRtsCts,
};

enum class McOperation : uint8_t {
    None,
    WordRead,
    CpuModel,
};

struct McSettings {
    McInterface interface_kind = McInterface::Rs232;
    mcprotocol::serial::PlcProfile profile = mcprotocol::serial::PlcProfile::MelsecIqR;
    uint32_t baud = t_rss3_board::kDefaultMcBaud;
    uint8_t data_bits = 8U;
    char parity = 'E';
    uint8_t stop_bits = 1U;
    int external_rx = 6;
    int external_tx = 7;
    int external_cts = 8;
    int external_rts = 9;
};

McSettings g_mc_settings;
HardwareSerial* g_mc_uart = &Serial0;
uart_port_t g_mc_uart_port = UART_NUM_0;
mcprotocol::serial::MelsecSerialClient g_mc_client;
uint16_t g_mc_words[kMcMaxReadPoints] = {};
mcprotocol::serial::CpuModelInfo g_mc_model = {};
uint32_t g_mc_device_number = 100U;
uint16_t g_mc_points = 1U;
McOperation g_mc_operation = McOperation::None;
bool g_mc_uart_ready = false;
bool g_mc_request_active = false;
bool g_mc_tx_sent = false;
bool g_mc_request_done = false;
bool g_mc_result_reported = false;
mcprotocol::serial::Status g_mc_completion_status = {};

void printPrompt() {
    Serial.print(F("plc> "));
}

bool equalsIgnoreCase(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        const unsigned char l = static_cast<unsigned char>(*lhs);
        const unsigned char r = static_cast<unsigned char>(*rhs);
        if (tolower(l) != tolower(r)) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

size_t tokenize(char* line, char* tokens[], size_t capacity) {
    size_t count = 0U;
    char* cursor = line;
    while (*cursor != '\0' && count < capacity) {
        while (isspace(static_cast<unsigned char>(*cursor)) != 0) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (*cursor == '"') {
            ++cursor;
            tokens[count++] = cursor;
            while (*cursor != '\0' && *cursor != '"') {
                ++cursor;
            }
            if (*cursor == '"') {
                *cursor++ = '\0';
            }
            continue;
        }
        tokens[count++] = cursor;
        while (*cursor != '\0' && isspace(static_cast<unsigned char>(*cursor)) == 0) {
            ++cursor;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

bool parseUnsigned(const char* text, unsigned long& value, int base = 10) {
    if (text == nullptr || *text == '\0' || *text == '-' || *text == '+') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    value = strtoul(text, &end, base);
    return errno != ERANGE && end != text && *end == '\0';
}

bool parsePort(const char* text, uint16_t& value) {
    unsigned long parsed = 0UL;
    if (!parseUnsigned(text, parsed) || parsed == 0UL || parsed > 65535UL) {
        return false;
    }
    value = static_cast<uint16_t>(parsed);
    return true;
}

bool parseGpio(const char* text, int& value) {
    unsigned long parsed = 0UL;
    if (!parseUnsigned(text, parsed) || parsed > 48UL) {
        return false;
    }
    value = static_cast<int>(parsed);
    return (value >= 6 && value <= 18) || value == 21 ||
           (value >= 33 && value <= 40) || value == 43 || value == 44 ||
           value == 47 || value == 48;
}

bool parseDDevice(const char* text, uint32_t& number) {
    if (text == nullptr || (text[0] != 'D' && text[0] != 'd')) {
        return false;
    }
    unsigned long parsed = 0UL;
    if (!parseUnsigned(text + 1, parsed) || parsed > 0xFFFFFFFFUL) {
        return false;
    }
    number = static_cast<uint32_t>(parsed);
    return true;
}

void printUint64(uint64_t value) {
    char text[24] = {};
    snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(value));
    Serial.print(text);
}

const char* slmpProfileName(slmp::PlcProfile profile) {
    switch (profile) {
        case slmp::PlcProfile::IqR:
            return "melsec:iq-r";
        case slmp::PlcProfile::IqRRj71En71:
            return "melsec:iq-r:rj71en71";
        case slmp::PlcProfile::IqF:
            return "melsec:iq-f";
        case slmp::PlcProfile::MxR:
            return "melsec:mx-r";
#if T_RSS3_SLMP_HAS_MXR_RJ71_PROFILE
        case slmp::PlcProfile::MxRRj71En71:
            return "melsec:mx-r:rj71en71";
#endif
        case slmp::PlcProfile::MxF:
            return "melsec:mx-f";
        case slmp::PlcProfile::QCpu:
            return "melsec:qcpu";
        case slmp::PlcProfile::LCpu:
            return "melsec:lcpu";
        case slmp::PlcProfile::QnU:
            return "melsec:qnu";
        case slmp::PlcProfile::QnUDV:
            return "melsec:qnudv";
        default:
            return "unknown";
    }
}

bool parseSlmpProfile(const char* text, slmp::PlcProfile& profile) {
    if (equalsIgnoreCase(text, "iqr") || equalsIgnoreCase(text, "melsec:iq-r")) {
        profile = slmp::PlcProfile::IqR;
    } else if (equalsIgnoreCase(text, "iqr-rj71") || equalsIgnoreCase(text, "melsec:iq-r:rj71en71")) {
        profile = slmp::PlcProfile::IqRRj71En71;
    } else if (equalsIgnoreCase(text, "iqf") || equalsIgnoreCase(text, "melsec:iq-f")) {
        profile = slmp::PlcProfile::IqF;
    } else if (equalsIgnoreCase(text, "mxr") || equalsIgnoreCase(text, "melsec:mx-r")) {
        profile = slmp::PlcProfile::MxR;
#if T_RSS3_SLMP_HAS_MXR_RJ71_PROFILE
    } else if (equalsIgnoreCase(text, "mxr-rj71") || equalsIgnoreCase(text, "melsec:mx-r:rj71en71")) {
        profile = slmp::PlcProfile::MxRRj71En71;
#endif
    } else if (equalsIgnoreCase(text, "mxf") || equalsIgnoreCase(text, "melsec:mx-f")) {
        profile = slmp::PlcProfile::MxF;
    } else if (equalsIgnoreCase(text, "q") || equalsIgnoreCase(text, "qcpu") || equalsIgnoreCase(text, "melsec:qcpu")) {
        profile = slmp::PlcProfile::QCpu;
    } else if (equalsIgnoreCase(text, "l") || equalsIgnoreCase(text, "lcpu") || equalsIgnoreCase(text, "melsec:lcpu")) {
        profile = slmp::PlcProfile::LCpu;
    } else {
        return false;
    }
    return true;
}

void printSlmpStats(const slmp::SlmpClient& client) {
#if T_RSS3_SLMP_HAS_TRAFFIC_STATS
    const slmp::TrafficStats stats = client.trafficStats();
    Serial.print(F(" request_count="));
    printUint64(stats.request_count);
    Serial.print(F(" tx_bytes="));
    printUint64(stats.tx_bytes);
    Serial.print(F(" rx_bytes="));
    printUint64(stats.rx_bytes);
#else
    (void)client;
#endif
}

void printSlmpError(const char* test_name, const slmp::SlmpClient& client) {
    Serial.print(F("RESULT test="));
    Serial.print(test_name);
    Serial.print(F(" status=FAIL error="));
    Serial.print(static_cast<unsigned int>(client.lastError()));
    Serial.print(F(" end_code=0x"));
    Serial.print(client.lastEndCode(), HEX);
    printSlmpStats(client);
    Serial.println();
}

bool wifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void printWifiStatus() {
    Serial.print(F("wifi_status="));
    Serial.print(wifiConnected() ? F("connected") : F("disconnected"));
    if (wifiConnected()) {
        Serial.print(F(" ssid=\""));
        Serial.print(WiFi.SSID());
        Serial.print(F("\" ip="));
        Serial.print(WiFi.localIP());
        Serial.print(F(" rssi="));
        Serial.print(WiFi.RSSI());
    }
    Serial.println();
}

void connectWifi(const char* ssid, const char* password) {
    if (ssid == nullptr || *ssid == '\0') {
        Serial.println(F("wifi connect usage: wifi connect \"ssid\" \"password\""));
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, false);
    delay(100);
    Serial.print(F("wifi connecting ssid=\""));
    Serial.print(ssid);
    Serial.println(F("\""));
    WiFi.begin(ssid, password == nullptr ? "" : password);
    const uint32_t started = millis();
    while (!wifiConnected() && static_cast<uint32_t>(millis() - started) < 15000U) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    printWifiStatus();
}

bool requireWifi(const char* test_name) {
    if (wifiConnected()) {
        return true;
    }
    Serial.print(F("RESULT test="));
    Serial.print(test_name);
    Serial.println(F(" status=FAIL reason=wifi_not_connected"));
    return false;
}

void applySlmpProfile(slmp::PlcProfile profile) {
    g_slmp_tcp_client.close();
    g_slmp_udp_client.close();
    const slmp::Error tcp_error = g_slmp_tcp_client.setPlcProfile(profile);
    const slmp::Error udp_error = g_slmp_udp_client.setPlcProfile(profile);
    if (tcp_error != slmp::Error::Ok || udp_error != slmp::Error::Ok) {
        Serial.print(F("slmp profile rejected tcp_error="));
        Serial.print(static_cast<unsigned int>(tcp_error));
        Serial.print(F(" udp_error="));
        Serial.println(static_cast<unsigned int>(udp_error));
        return;
    }
    g_slmp_profile = profile;
    Serial.print(F("slmp_profile="));
    Serial.println(slmpProfileName(profile));
}

bool readSlmpWord(slmp::SlmpClient& client, uint32_t device_number, uint16_t& value) {
    return client.readOneWord(
               slmp::DeviceAddress{g_slmp_profile, slmp::DeviceCode::D, device_number},
               value) == slmp::Error::Ok;
}

void runSlmpTcpRead(uint32_t device_number) {
    constexpr const char* kTest = "slmp-tcp-read";
    if (!requireWifi(kTest)) {
        return;
    }
    g_slmp_tcp_client.close();
    if (!g_slmp_tcp_client.connect(g_slmp_host, g_slmp_tcp_port)) {
        printSlmpError(kTest, g_slmp_tcp_client);
        return;
    }
    uint16_t value = 0U;
    if (!readSlmpWord(g_slmp_tcp_client, device_number, value)) {
        printSlmpError(kTest, g_slmp_tcp_client);
        g_slmp_tcp_client.close();
        return;
    }
    Serial.print(F("RESULT test="));
    Serial.print(kTest);
    Serial.print(F(" status=PASS profile="));
    Serial.print(slmpProfileName(g_slmp_profile));
    Serial.print(F(" device=D"));
    Serial.print(device_number);
    Serial.print(F(" value="));
    Serial.print(value);
    printSlmpStats(g_slmp_tcp_client);
    Serial.println();
    g_slmp_tcp_client.close();
}

bool readSocketOption(int fd, int level, int option, int& value) {
    socklen_t length = sizeof(value);
    value = 0;
    return fd >= 0 && getsockopt(fd, level, option, &value, &length) == 0 && length == sizeof(value);
}

void runSlmpTcpKeepAlive(uint32_t device_number, uint32_t idle_ms) {
    constexpr const char* kTest = "slmp-tcp-keepalive";
    if (!requireWifi(kTest)) {
        return;
    }
    if (idle_ms <= 30000U || idle_ms > 600000U) {
        Serial.println(F("slmp tcp-keepalive idle_ms must be 30001..600000"));
        return;
    }
    g_slmp_tcp_client.close();
    if (!g_slmp_tcp_client.connect(g_slmp_host, g_slmp_tcp_port)) {
        printSlmpError(kTest, g_slmp_tcp_client);
        return;
    }

    const int fd_before = g_slmp_tcp_socket.fd();
    int keepalive_enabled = 0;
    int keepalive_idle = 0;
    const bool keepalive_readable =
        readSocketOption(fd_before, SOL_SOCKET, SO_KEEPALIVE, keepalive_enabled) &&
        readSocketOption(fd_before, IPPROTO_TCP, TCP_KEEPIDLE, keepalive_idle);
    uint16_t before_value = 0U;
    if (!readSlmpWord(g_slmp_tcp_client, device_number, before_value)) {
        printSlmpError(kTest, g_slmp_tcp_client);
        g_slmp_tcp_client.close();
        return;
    }

    Serial.print(F("keepalive_socket fd="));
    Serial.print(fd_before);
    Serial.print(F(" so_keepalive="));
    Serial.print(keepalive_enabled);
    Serial.print(F(" tcp_keepidle="));
    Serial.println(keepalive_idle);
    Serial.print(F("idle_wait_ms="));
    Serial.println(idle_ms);

    const uint32_t started = millis();
    uint32_t next_report = 5000U;
    while (static_cast<uint32_t>(millis() - started) < idle_ms) {
        const uint32_t elapsed = static_cast<uint32_t>(millis() - started);
        if (elapsed >= next_report) {
            Serial.print(F("idle_elapsed_ms="));
            Serial.println(elapsed);
            next_report += 5000U;
        }
        delay(25);
    }

    const int fd_after = g_slmp_tcp_socket.fd();
    uint16_t after_value = 0U;
    const bool second_read_ok = readSlmpWord(g_slmp_tcp_client, device_number, after_value);
    const bool pass = keepalive_readable && keepalive_enabled == 1 && keepalive_idle == 30 &&
                      fd_before >= 0 && fd_before == fd_after && second_read_ok;
    Serial.print(F("RESULT test="));
    Serial.print(kTest);
    Serial.print(F(" status="));
    Serial.print(pass ? F("PASS") : F("FAIL"));
    Serial.print(F(" profile="));
    Serial.print(slmpProfileName(g_slmp_profile));
    Serial.print(F(" device=D"));
    Serial.print(device_number);
    Serial.print(F(" first_value="));
    Serial.print(before_value);
    Serial.print(F(" second_value="));
    Serial.print(after_value);
    Serial.print(F(" fd_before="));
    Serial.print(fd_before);
    Serial.print(F(" fd_after="));
    Serial.print(fd_after);
    Serial.print(F(" so_keepalive="));
    Serial.print(keepalive_enabled);
    Serial.print(F(" tcp_keepidle="));
    Serial.print(keepalive_idle);
    if (!second_read_ok) {
        Serial.print(F(" error="));
        Serial.print(static_cast<unsigned int>(g_slmp_tcp_client.lastError()));
    }
    printSlmpStats(g_slmp_tcp_client);
    Serial.println();
    g_slmp_tcp_client.close();
}

void runSlmpUdpRead(uint32_t device_number) {
    constexpr const char* kTest = "slmp-udp-ephemeral-read";
    if (!requireWifi(kTest)) {
        return;
    }
    g_slmp_udp_client.close();
    if (!g_slmp_udp_client.connect(g_slmp_host, g_slmp_udp_port)) {
        printSlmpError(kTest, g_slmp_udp_client);
        return;
    }
    uint16_t value = 0U;
    if (!readSlmpWord(g_slmp_udp_client, device_number, value)) {
        printSlmpError(kTest, g_slmp_udp_client);
        g_slmp_udp_client.close();
        return;
    }
    Serial.print(F("RESULT test="));
    Serial.print(kTest);
    Serial.print(F(" status=PASS profile="));
    Serial.print(slmpProfileName(g_slmp_profile));
    Serial.print(F(" device=D"));
    Serial.print(device_number);
    Serial.print(F(" value="));
    Serial.print(value);
    Serial.print(F(" requested_local_port=0 ephemeral_binding_proved_by_reply=true"));
    printSlmpStats(g_slmp_udp_client);
    Serial.println();
    Serial.println(F("note: capture the UDP source port on the PLC/network side when numeric-port evidence is required"));
    g_slmp_udp_client.close();
}

const char* mcInterfaceName(McInterface value) {
    switch (value) {
        case McInterface::Rs232:
            return "rs232-onboard";
        case McInterface::Rs485:
            return "rs485-onboard";
        case McInterface::ExternalRtsCts:
            return "external-rtscts";
    }
    return "unknown";
}

const char* mcProfileName(mcprotocol::serial::PlcProfile profile) {
    return mcprotocol::serial::plc_profile_name(profile);
}

const char* uartDataBitsName(uart_word_length_t value) {
    switch (value) {
        case UART_DATA_5_BITS:
            return "5";
        case UART_DATA_6_BITS:
            return "6";
        case UART_DATA_7_BITS:
            return "7";
        case UART_DATA_8_BITS:
            return "8";
        default:
            return "unknown";
    }
}

const char* uartParityName(uart_parity_t value) {
    switch (value) {
        case UART_PARITY_DISABLE:
            return "N";
        case UART_PARITY_EVEN:
            return "E";
        case UART_PARITY_ODD:
            return "O";
        default:
            return "unknown";
    }
}

const char* uartStopBitsName(uart_stop_bits_t value) {
    switch (value) {
        case UART_STOP_BITS_1:
            return "1";
        case UART_STOP_BITS_1_5:
            return "1.5";
        case UART_STOP_BITS_2:
            return "2";
        default:
            return "unknown";
    }
}

const char* uartFlowControlName(uart_hw_flowcontrol_t value) {
    switch (value) {
        case UART_HW_FLOWCTRL_DISABLE:
            return "none";
        case UART_HW_FLOWCTRL_RTS:
            return "rts";
        case UART_HW_FLOWCTRL_CTS:
            return "cts";
        case UART_HW_FLOWCTRL_CTS_RTS:
            return "rtscts";
        default:
            return "unknown";
    }
}

bool printEffectiveMcUart() {
    uint32_t baud = 0U;
    uart_word_length_t data_bits = UART_DATA_8_BITS;
    uart_parity_t parity = UART_PARITY_DISABLE;
    uart_stop_bits_t stop_bits = UART_STOP_BITS_1;
    uart_hw_flowcontrol_t flow_control = UART_HW_FLOWCTRL_DISABLE;
    const bool query_ok =
        uart_get_baudrate(g_mc_uart_port, &baud) == ESP_OK &&
        uart_get_word_length(g_mc_uart_port, &data_bits) == ESP_OK &&
        uart_get_parity(g_mc_uart_port, &parity) == ESP_OK &&
        uart_get_stop_bits(g_mc_uart_port, &stop_bits) == ESP_OK &&
        uart_get_hw_flow_ctrl(g_mc_uart_port, &flow_control) == ESP_OK;
    const uart_word_length_t expected_data_bits =
        g_mc_settings.data_bits == 7U ? UART_DATA_7_BITS : UART_DATA_8_BITS;
    const uart_parity_t expected_parity =
        g_mc_settings.parity == 'E'
            ? UART_PARITY_EVEN
            : (g_mc_settings.parity == 'O' ? UART_PARITY_ODD : UART_PARITY_DISABLE);
    const uart_stop_bits_t expected_stop_bits =
        g_mc_settings.stop_bits == 2U ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
    const uart_hw_flowcontrol_t expected_flow_control =
        g_mc_settings.interface_kind == McInterface::ExternalRtsCts
            ? UART_HW_FLOWCTRL_CTS_RTS
            : UART_HW_FLOWCTRL_DISABLE;
    const bool matches_requested =
        query_ok && baud == g_mc_settings.baud && data_bits == expected_data_bits &&
        parity == expected_parity && stop_bits == expected_stop_bits &&
        flow_control == expected_flow_control;
    Serial.print(F("uart_effective controller="));
    Serial.print(static_cast<unsigned int>(g_mc_uart_port));
    Serial.print(F(" query_ok="));
    Serial.print(query_ok ? 1 : 0);
    Serial.print(F(" baud="));
    Serial.print(baud);
    Serial.print(F(" data_bits="));
    Serial.print(uartDataBitsName(data_bits));
    Serial.print(F(" parity="));
    Serial.print(uartParityName(parity));
    Serial.print(F(" stop_bits="));
    Serial.print(uartStopBitsName(stop_bits));
    Serial.print(F(" flow="));
    Serial.print(uartFlowControlName(flow_control));
    Serial.print(F(" matches_requested="));
    Serial.println(matches_requested ? 1 : 0);
    return matches_requested;
}

bool parseMcProfile(const char* text, mcprotocol::serial::PlcProfile& profile) {
    if (equalsIgnoreCase(text, "iqr") || equalsIgnoreCase(text, "melsec:iq-r")) {
        profile = mcprotocol::serial::PlcProfile::MelsecIqR;
    } else if (equalsIgnoreCase(text, "q") || equalsIgnoreCase(text, "qcpu") || equalsIgnoreCase(text, "melsec:qcpu")) {
        profile = mcprotocol::serial::PlcProfile::MelsecQ;
    } else {
        return false;
    }
    return true;
}

uint32_t serialConfigValue(uint8_t data_bits, char parity, uint8_t stop_bits) {
    if (data_bits == 7U && parity == 'N' && stop_bits == 1U) return SERIAL_7N1;
    if (data_bits == 7U && parity == 'N' && stop_bits == 2U) return SERIAL_7N2;
    if (data_bits == 7U && parity == 'E' && stop_bits == 1U) return SERIAL_7E1;
    if (data_bits == 7U && parity == 'E' && stop_bits == 2U) return SERIAL_7E2;
    if (data_bits == 7U && parity == 'O' && stop_bits == 1U) return SERIAL_7O1;
    if (data_bits == 7U && parity == 'O' && stop_bits == 2U) return SERIAL_7O2;
    if (data_bits == 8U && parity == 'N' && stop_bits == 1U) return SERIAL_8N1;
    if (data_bits == 8U && parity == 'N' && stop_bits == 2U) return SERIAL_8N2;
    if (data_bits == 8U && parity == 'E' && stop_bits == 1U) return SERIAL_8E1;
    if (data_bits == 8U && parity == 'E' && stop_bits == 2U) return SERIAL_8E2;
    if (data_bits == 8U && parity == 'O' && stop_bits == 1U) return SERIAL_8O1;
    if (data_bits == 8U && parity == 'O' && stop_bits == 2U) return SERIAL_8O2;
    return 0U;
}

mcprotocol::serial::ProtocolConfig makeMcProtocol() {
    return mcprotocol::serial::ProtocolConfig::ascii(
        mcprotocol::serial::AsciiFrameKind::C4,
        mcprotocol::serial::AsciiFormat::Format4,
        g_mc_settings.profile,
        mcprotocol::serial::SumCheckMode::Disabled,
        mcprotocol::serial::RouteConfig{mcprotocol::serial::HostStationRoute{}});
}

void mcRs485TxBegin(void*) {
    digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485TxEnable);
    delayMicroseconds(100U);
}

void mcRs485TxEnd(void*) {
    digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
}

bool configureMcClient() {
    const mcprotocol::serial::Status config_status = g_mc_client.configure(makeMcProtocol());
    if (!config_status.ok()) {
        Serial.print(F("mc configure failed code="));
        Serial.print(static_cast<unsigned int>(config_status.code));
        Serial.print(F(" message="));
        Serial.println(config_status.message);
        return false;
    }
    mcprotocol::serial::Rs485Hooks hooks{};
    if (g_mc_settings.interface_kind == McInterface::Rs485) {
        hooks.on_tx_begin = mcRs485TxBegin;
        hooks.on_tx_end = mcRs485TxEnd;
    }
    const mcprotocol::serial::Status hook_status = g_mc_client.set_rs485_hooks(hooks);
    if (!hook_status.ok()) {
        Serial.print(F("mc hook configure failed code="));
        Serial.print(static_cast<unsigned int>(hook_status.code));
        Serial.print(F(" message="));
        Serial.println(hook_status.message);
        return false;
    }
    return true;
}

void drainMcUart() {
    if (g_mc_uart == nullptr) {
        return;
    }
    while (g_mc_uart->available() > 0) {
        (void)g_mc_uart->read();
    }
}

bool applyMcUart() {
    if (g_mc_client.busy()) {
        Serial.println(F("mc uart change rejected: request busy"));
        return false;
    }
    const uint32_t config = serialConfigValue(
        g_mc_settings.data_bits,
        g_mc_settings.parity,
        g_mc_settings.stop_bits);
    if (config == 0U) {
        Serial.println(F("mc uart change rejected: unsupported serial format"));
        return false;
    }

    g_mc_uart_ready = false;
    if (g_mc_uart != nullptr) {
        g_mc_uart->end();
    }
    int rx_pin = t_rss3_board::kRs232RxPin;
    int tx_pin = t_rss3_board::kRs232TxPin;
    if (g_mc_settings.interface_kind == McInterface::Rs485) {
        g_mc_uart = &Serial1;
        g_mc_uart_port = UART_NUM_1;
        rx_pin = t_rss3_board::kRs485RxPin;
        tx_pin = t_rss3_board::kRs485TxPin;
        pinMode(t_rss3_board::kRs485DirectionPin, OUTPUT);
        digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
    } else {
        g_mc_uart = &Serial0;
        g_mc_uart_port = UART_NUM_0;
        if (g_mc_settings.interface_kind == McInterface::ExternalRtsCts) {
            rx_pin = g_mc_settings.external_rx;
            tx_pin = g_mc_settings.external_tx;
        }
    }

    const bool buffers_ok =
        g_mc_uart->setRxBufferSize(kMcUartBufferCapacity) >= kMcUartBufferCapacity &&
        g_mc_uart->setTxBufferSize(kMcUartBufferCapacity) >= kMcUartBufferCapacity;
    g_mc_uart->begin(g_mc_settings.baud, config, rx_pin, tx_pin);
    bool flow_ok = true;
    if (g_mc_settings.interface_kind == McInterface::ExternalRtsCts) {
        flow_ok = g_mc_uart->setPins(
                      static_cast<int8_t>(g_mc_settings.external_rx),
                      static_cast<int8_t>(g_mc_settings.external_tx),
                      static_cast<int8_t>(g_mc_settings.external_cts),
                      static_cast<int8_t>(g_mc_settings.external_rts)) &&
                  g_mc_uart->setHwFlowCtrlMode(UART_HW_FLOWCTRL_CTS_RTS);
    } else {
        flow_ok = g_mc_uart->setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE);
    }
    delay(20U);
    drainMcUart();
    const bool protocol_ok = configureMcClient();
    const bool effective_ok = printEffectiveMcUart();
    g_mc_uart_ready = buffers_ok && flow_ok && protocol_ok && effective_ok;
    if (!g_mc_uart_ready) {
        g_mc_uart->end();
        digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
        Serial.println(F("mc uart initialization failed"));
        return false;
    }
    Serial.print(F("mc_uart_ready interface="));
    Serial.print(mcInterfaceName(g_mc_settings.interface_kind));
    Serial.print(F(" profile="));
    Serial.print(mcProfileName(g_mc_settings.profile));
    Serial.print(F(" baud="));
    Serial.print(g_mc_settings.baud);
    Serial.print(F(" format="));
    Serial.print(g_mc_settings.data_bits);
    Serial.print(g_mc_settings.parity);
    Serial.print(g_mc_settings.stop_bits);
    Serial.print(F(" rx="));
    Serial.print(rx_pin);
    Serial.print(F(" tx="));
    Serial.print(tx_pin);
    if (g_mc_settings.interface_kind == McInterface::ExternalRtsCts) {
        Serial.print(F(" cts="));
        Serial.print(g_mc_settings.external_cts);
        Serial.print(F(" rts="));
        Serial.print(g_mc_settings.external_rts);
    }
    Serial.println();
    return true;
}

bool requireVerifiedMcUart(const char* test_name) {
    if (!g_mc_uart_ready) {
        Serial.print(F("RESULT test="));
        Serial.print(test_name);
        Serial.println(F(" status=FAIL reason=uart_not_ready action=mc_reset"));
        return false;
    }
    if (printEffectiveMcUart()) {
        return true;
    }
    g_mc_uart_ready = false;
    g_mc_uart->end();
    digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
    Serial.print(F("RESULT test="));
    Serial.print(test_name);
    Serial.println(F(" status=FAIL reason=effective_uart_mismatch action=mc_reset"));
    return false;
}

void mcCompletion(void*, mcprotocol::serial::Status status) {
    g_mc_completion_status = status;
    g_mc_request_active = false;
    g_mc_tx_sent = false;
    g_mc_request_done = true;
}

void pumpMcTx();

void startMcRead(uint32_t device_number, uint16_t points) {
    if (!requireVerifiedMcUart("mc-serial-read")) {
        return;
    }
    if (g_mc_client.busy() || g_mc_request_active) {
        Serial.println(F("RESULT test=mc-serial-read status=FAIL reason=request_busy"));
        return;
    }
    if (g_mc_client.requires_transport_reset()) {
        Serial.println(F("RESULT test=mc-serial-read status=FAIL reason=transport_reset_required action=mc_reset"));
        return;
    }
    if (points == 0U || points > kMcMaxReadPoints) {
        Serial.println(F("mc read points must be 1..8"));
        return;
    }
    drainMcUart();
    memset(g_mc_words, 0, sizeof(g_mc_words));
    g_mc_device_number = device_number;
    g_mc_points = points;
    g_mc_operation = McOperation::WordRead;
    g_mc_request_done = false;
    g_mc_result_reported = false;
    g_mc_tx_sent = false;
    const mcprotocol::serial::Status status = g_mc_client.async_batch_read_words(
        millis(),
        mcprotocol::serial::BatchReadWordsRequest(
            mcprotocol::serial::DeviceAddress{mcprotocol::serial::DeviceCode::D, device_number},
            points),
        std::span<uint16_t>(g_mc_words, points),
        mcCompletion,
        nullptr);
    if (!status.ok()) {
        g_mc_completion_status = status;
        g_mc_request_done = true;
        return;
    }
    g_mc_request_active = true;
    pumpMcTx();
}

void startMcCpuModelRead() {
    if (!requireVerifiedMcUart("mc-cpu-model")) {
        return;
    }
    if (g_mc_client.busy() || g_mc_request_active) {
        Serial.println(F("RESULT test=mc-cpu-model status=FAIL reason=request_busy"));
        return;
    }
    if (g_mc_client.requires_transport_reset()) {
        Serial.println(F("RESULT test=mc-cpu-model status=FAIL reason=transport_reset_required action=mc_reset"));
        return;
    }
    drainMcUart();
    g_mc_model = {};
    g_mc_operation = McOperation::CpuModel;
    g_mc_request_done = false;
    g_mc_result_reported = false;
    g_mc_tx_sent = false;
    const mcprotocol::serial::Status status =
        g_mc_client.async_read_cpu_model(millis(), g_mc_model, mcCompletion, nullptr);
    if (!status.ok()) {
        g_mc_completion_status = status;
        g_mc_request_done = true;
        return;
    }
    g_mc_request_active = true;
    pumpMcTx();
}

void pumpMcTx() {
    if (!g_mc_request_active || g_mc_tx_sent || g_mc_uart == nullptr) {
        return;
    }
    const std::span<const std::byte> frame = g_mc_client.pending_tx_frame();
    if (frame.empty()) {
        return;
    }
    const bool bounded_write = frame.size() <= kMcMaxFiniteWriteFrameBytes;
    const size_t written =
        bounded_write
            ? g_mc_uart->write(
                  reinterpret_cast<const uint8_t*>(frame.data()),
                  frame.size())
            : 0U;
    const bool accepted = bounded_write && written == frame.size();
    const bool tx_completed =
        accepted &&
        uart_wait_tx_done(
            g_mc_uart_port,
            pdMS_TO_TICKS(kMcTxCompletionTimeoutMs)) == ESP_OK;
    const mcprotocol::serial::Status transport_status =
        tx_completed
            ? mcprotocol::serial::ok_status()
            : mcprotocol::serial::make_status(
                  mcprotocol::serial::StatusCode::Transport,
                  !bounded_write
                      ? "Request frame exceeds the console finite-write limit"
                      : (accepted
                             ? "UART transmit did not complete before the finite deadline"
                             : "UART did not accept the complete request frame"));
    if (!transport_status.ok()) {
        g_mc_uart->end();
        g_mc_uart_ready = false;
    }
    const mcprotocol::serial::Status notify_status =
        g_mc_client.notify_tx_complete(millis(), transport_status);
    if (!notify_status.ok()) {
        g_mc_uart->end();
        g_mc_uart_ready = false;
        digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
        g_mc_completion_status = notify_status;
        g_mc_request_active = false;
        g_mc_request_done = true;
        return;
    }
    g_mc_tx_sent = g_mc_request_active && g_mc_client.busy();
}

void pumpMcRx() {
    if (g_mc_uart == nullptr) {
        return;
    }
    std::byte bytes[64] = {};
    size_t count = 0U;
    while (g_mc_uart->available() > 0 && count < sizeof(bytes)) {
        const int value = g_mc_uart->read();
        if (value < 0) {
            break;
        }
        bytes[count++] = static_cast<std::byte>(static_cast<uint8_t>(value));
    }
    if (count > 0U && g_mc_client.busy()) {
        g_mc_client.on_rx_bytes(millis(), std::span<const std::byte>(bytes, count));
    }
}

void reportMcResult() {
    if (!g_mc_request_done || g_mc_result_reported) {
        return;
    }
    g_mc_result_reported = true;
    Serial.print(F("RESULT test="));
    Serial.print(g_mc_operation == McOperation::CpuModel ? F("mc-cpu-model") : F("mc-serial-read"));
    Serial.print(F(" status="));
    Serial.print(g_mc_completion_status.ok() ? F("PASS") : F("FAIL"));
    Serial.print(F(" interface="));
    Serial.print(mcInterfaceName(g_mc_settings.interface_kind));
    Serial.print(F(" profile="));
    Serial.print(mcProfileName(g_mc_settings.profile));
    Serial.print(F(" serial="));
    Serial.print(g_mc_settings.baud);
    Serial.print('/');
    Serial.print(g_mc_settings.data_bits);
    Serial.print(g_mc_settings.parity);
    Serial.print(g_mc_settings.stop_bits);
    Serial.print(F(" flow="));
    Serial.print(g_mc_settings.interface_kind == McInterface::ExternalRtsCts ? F("rtscts") : F("none"));
    Serial.print(F(" uart_effective_verified=1"));
    if (g_mc_operation == McOperation::WordRead) {
        Serial.print(F(" device=D"));
        Serial.print(g_mc_device_number);
        Serial.print(F(" points="));
        Serial.print(g_mc_points);
    }
    if (g_mc_completion_status.ok()) {
        if (g_mc_operation == McOperation::CpuModel) {
            Serial.print(F(" model=\""));
            Serial.print(g_mc_model.model_name.data());
            Serial.print(F("\" model_code=0x"));
            Serial.print(g_mc_model.model_code, HEX);
        } else {
            Serial.print(F(" values="));
            for (uint16_t index = 0U; index < g_mc_points; ++index) {
                if (index != 0U) Serial.print(',');
                Serial.print(g_mc_words[index]);
            }
        }
    } else {
        Serial.print(F(" code="));
        Serial.print(static_cast<unsigned int>(g_mc_completion_status.code));
        Serial.print(F(" plc_error=0x"));
        Serial.print(g_mc_completion_status.plc_error_code, HEX);
        Serial.print(F(" message=\""));
        Serial.print(g_mc_completion_status.message);
        Serial.print('"');
    }
    Serial.print(F(" transport_reset_required="));
    Serial.print(g_mc_client.requires_transport_reset() ? 1 : 0);
    Serial.println();
    g_mc_operation = McOperation::None;
}

void printMcStatus() {
    if (g_mc_uart_ready) {
        const bool effective_matches = printEffectiveMcUart();
        if (!effective_matches && !g_mc_client.busy()) {
            g_mc_uart_ready = false;
            g_mc_uart->end();
            digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
        }
    }
    Serial.print(F("mc_interface="));
    Serial.print(mcInterfaceName(g_mc_settings.interface_kind));
    Serial.print(F(" mc_profile="));
    Serial.print(mcProfileName(g_mc_settings.profile));
    Serial.print(F(" serial="));
    Serial.print(g_mc_settings.baud);
    Serial.print('/');
    Serial.print(g_mc_settings.data_bits);
    Serial.print(g_mc_settings.parity);
    Serial.print(g_mc_settings.stop_bits);
    Serial.print(F(" flow="));
    Serial.print(g_mc_settings.interface_kind == McInterface::ExternalRtsCts ? F("rtscts") : F("none"));
    Serial.print(F(" ready="));
    Serial.print(g_mc_uart_ready ? 1 : 0);
    Serial.print(F(" busy="));
    Serial.print(g_mc_client.busy() ? 1 : 0);
    Serial.print(F(" reset_required="));
    Serial.println(g_mc_client.requires_transport_reset() ? 1 : 0);
}

void printBoardStatus() {
    Serial.print(F("board_model=t-rss3 chip="));
    Serial.print(ESP.getChipModel());
    Serial.print(F(" revision="));
    Serial.print(ESP.getChipRevision());
    Serial.print(F(" flash_bytes="));
    Serial.print(ESP.getFlashChipSize());
    Serial.print(F(" flash_hz="));
    Serial.print(ESP.getFlashChipSpeed());
    Serial.print(F(" psram_bytes="));
    Serial.print(ESP.getPsramSize());
    Serial.print(F(" free_heap="));
    Serial.println(ESP.getFreeHeap());
}

void printStatus() {
    printBoardStatus();
    printWifiStatus();
    Serial.print(F("slmp_endpoint="));
    Serial.print(g_slmp_host);
    Serial.print(F(" tcp="));
    Serial.print(g_slmp_tcp_port);
    Serial.print(F(" udp="));
    Serial.print(g_slmp_udp_port);
    Serial.print(F(" profile="));
    Serial.println(slmpProfileName(g_slmp_profile));
    printMcStatus();
}

void printHelp() {
    Serial.println(F("=== T-RSS3 PLC C++ verification console (read-only) ==="));
    Serial.println(F("help | status"));
    Serial.println(F("wifi connect \"ssid\" \"password\" | wifi status | wifi disconnect"));
    Serial.println(F("slmp endpoint <ipv4> <tcp_port> <udp_port>"));
#if T_RSS3_SLMP_HAS_MXR_RJ71_PROFILE
    Serial.println(F("slmp profile <iqr|iqr-rj71|iqf|mxr|mxr-rj71|mxf|q|l>"));
#else
    Serial.println(F("slmp profile <iqr|iqr-rj71|iqf|mxr|mxf|q|l>"));
#endif
    Serial.println(F("slmp tcp-read D100"));
    Serial.println(F("slmp tcp-keepalive D100 [idle_ms=30001..600000]"));
    Serial.println(F("slmp udp-read D100"));
    Serial.println(F("mc profile <iqr|q>"));
    Serial.println(F("mc interface <rs232|rs485>"));
    Serial.println(F("mc interface external <rx> <tx> <cts> <rts>  (external level adapter required)"));
    Serial.println(F("mc serial <baud=1200..256000> <7|8> <N|E|O> <1|2>"));
    Serial.println(F("mc preset <iqr-rs232|q-rs232|iqr-rs485|q-rs485>"));
    Serial.println(F("mc model | mc read D100 [points] | mc reset | mc status"));
    Serial.println(F("No PLC communication starts at boot. This firmware exposes no write command."));
}

void handleWifiCommand(char* tokens[], size_t count) {
    if (count < 2U || equalsIgnoreCase(tokens[1], "status")) {
        printWifiStatus();
    } else if (equalsIgnoreCase(tokens[1], "connect")) {
        if (count < 3U) {
            Serial.println(F("wifi connect usage: wifi connect \"ssid\" \"password\""));
            return;
        }
        connectWifi(tokens[2], count > 3U ? tokens[3] : "");
    } else if (equalsIgnoreCase(tokens[1], "disconnect")) {
        g_slmp_tcp_client.close();
        g_slmp_udp_client.close();
        WiFi.disconnect(true, false);
        Serial.println(F("wifi disconnected"));
    } else {
        Serial.println(F("unknown wifi command"));
    }
}

void handleSlmpCommand(char* tokens[], size_t count) {
    if (count < 2U) {
        Serial.println(F("slmp command required"));
        return;
    }
    if (equalsIgnoreCase(tokens[1], "endpoint")) {
        if (count != 5U) {
            Serial.println(F("slmp endpoint usage: slmp endpoint <ipv4> <tcp_port> <udp_port>"));
            return;
        }
        IPAddress parsed;
        uint16_t tcp_port = 0U;
        uint16_t udp_port = 0U;
        if (!parsed.fromString(tokens[2]) || !parsePort(tokens[3], tcp_port) || !parsePort(tokens[4], udp_port)) {
            Serial.println(F("slmp endpoint rejected"));
            return;
        }
        g_slmp_tcp_client.close();
        g_slmp_udp_client.close();
        strncpy(g_slmp_host, tokens[2], sizeof(g_slmp_host) - 1U);
        g_slmp_host[sizeof(g_slmp_host) - 1U] = '\0';
        g_slmp_tcp_port = tcp_port;
        g_slmp_udp_port = udp_port;
        printStatus();
    } else if (equalsIgnoreCase(tokens[1], "profile")) {
        slmp::PlcProfile profile = slmp::PlcProfile::Unspecified;
        if (count != 3U || !parseSlmpProfile(tokens[2], profile)) {
            Serial.println(F("slmp profile rejected"));
            return;
        }
        applySlmpProfile(profile);
    } else if (equalsIgnoreCase(tokens[1], "tcp-read")) {
        uint32_t device = 0U;
        if (count != 3U || !parseDDevice(tokens[2], device)) {
            Serial.println(F("slmp tcp-read usage: slmp tcp-read D100"));
            return;
        }
        runSlmpTcpRead(device);
    } else if (equalsIgnoreCase(tokens[1], "tcp-keepalive")) {
        uint32_t device = 0U;
        unsigned long idle_ms = kDefaultKeepAliveTestIdleMs;
        if (count < 3U || count > 4U || !parseDDevice(tokens[2], device) ||
            (count == 4U && !parseUnsigned(tokens[3], idle_ms)) ||
            idle_ms <= 30000UL || idle_ms > 600000UL) {
            Serial.println(F("slmp tcp-keepalive usage: slmp tcp-keepalive D100 [30001..600000]"));
            return;
        }
        runSlmpTcpKeepAlive(device, static_cast<uint32_t>(idle_ms));
    } else if (equalsIgnoreCase(tokens[1], "udp-read")) {
        uint32_t device = 0U;
        if (count != 3U || !parseDDevice(tokens[2], device)) {
            Serial.println(F("slmp udp-read usage: slmp udp-read D100"));
            return;
        }
        runSlmpUdpRead(device);
    } else {
        Serial.println(F("unknown slmp command"));
    }
}

void applyMcPreset(const char* name) {
    if (equalsIgnoreCase(name, "iqr-rs232")) {
        g_mc_settings.profile = mcprotocol::serial::PlcProfile::MelsecIqR;
        g_mc_settings.interface_kind = McInterface::Rs232;
    } else if (equalsIgnoreCase(name, "q-rs232")) {
        g_mc_settings.profile = mcprotocol::serial::PlcProfile::MelsecQ;
        g_mc_settings.interface_kind = McInterface::Rs232;
    } else if (equalsIgnoreCase(name, "iqr-rs485")) {
        g_mc_settings.profile = mcprotocol::serial::PlcProfile::MelsecIqR;
        g_mc_settings.interface_kind = McInterface::Rs485;
    } else if (equalsIgnoreCase(name, "q-rs485")) {
        g_mc_settings.profile = mcprotocol::serial::PlcProfile::MelsecQ;
        g_mc_settings.interface_kind = McInterface::Rs485;
    } else {
        Serial.println(F("mc preset rejected"));
        return;
    }
    g_mc_settings.baud = t_rss3_board::kDefaultMcBaud;
    g_mc_settings.data_bits = 8U;
    g_mc_settings.parity = 'E';
    g_mc_settings.stop_bits = 1U;
    (void)applyMcUart();
}

void handleMcCommand(char* tokens[], size_t count) {
    if (count < 2U || equalsIgnoreCase(tokens[1], "status")) {
        printMcStatus();
        return;
    }
    if (equalsIgnoreCase(tokens[1], "profile")) {
        mcprotocol::serial::PlcProfile profile = mcprotocol::serial::PlcProfile::Unspecified;
        if (count != 3U || !parseMcProfile(tokens[2], profile) || g_mc_client.busy()) {
            Serial.println(F("mc profile rejected"));
            return;
        }
        if (g_mc_client.requires_transport_reset()) {
            Serial.println(F("mc profile rejected: transport reset required; use mc reset first"));
            return;
        }
        g_mc_settings.profile = profile;
        (void)configureMcClient();
        printMcStatus();
    } else if (equalsIgnoreCase(tokens[1], "interface")) {
        if (count == 3U && equalsIgnoreCase(tokens[2], "rs232")) {
            g_mc_settings.interface_kind = McInterface::Rs232;
        } else if (count == 3U && equalsIgnoreCase(tokens[2], "rs485")) {
            g_mc_settings.interface_kind = McInterface::Rs485;
        } else if (count == 7U && equalsIgnoreCase(tokens[2], "external")) {
            int rx = 0;
            int tx = 0;
            int cts = 0;
            int rts = 0;
            if (!parseGpio(tokens[3], rx) || !parseGpio(tokens[4], tx) ||
                !parseGpio(tokens[5], cts) || !parseGpio(tokens[6], rts) ||
                rx == tx || rx == cts || rx == rts || tx == cts || tx == rts || cts == rts) {
                Serial.println(F("mc external pins rejected (use four distinct GPIOs exposed on J4)"));
                return;
            }
            g_mc_settings.interface_kind = McInterface::ExternalRtsCts;
            g_mc_settings.external_rx = rx;
            g_mc_settings.external_tx = tx;
            g_mc_settings.external_cts = cts;
            g_mc_settings.external_rts = rts;
            Serial.println(F("warning: external 3.3V UART-to-RS232 adapter with CTS/RTS is required"));
        } else {
            Serial.println(F("mc interface usage: mc interface <rs232|rs485|external rx tx cts rts>"));
            return;
        }
        (void)applyMcUart();
    } else if (equalsIgnoreCase(tokens[1], "serial")) {
        unsigned long baud = 0UL;
        unsigned long data_bits = 0UL;
        unsigned long stop_bits = 0UL;
        if (count != 6U || !parseUnsigned(tokens[2], baud) || baud < 1200UL || baud > 256000UL ||
            !parseUnsigned(tokens[3], data_bits) ||
            (data_bits != 7UL && data_bits != 8UL) ||
            tokens[4] == nullptr || strlen(tokens[4]) != 1U ||
            !parseUnsigned(tokens[5], stop_bits) ||
            (stop_bits != 1UL && stop_bits != 2UL)) {
            Serial.println(F("mc serial usage: mc serial <baud> <7|8> <N|E|O> <1|2>"));
            return;
        }
        const char parity = static_cast<char>(toupper(static_cast<unsigned char>(tokens[4][0])));
        if (parity != 'N' && parity != 'E' && parity != 'O') {
            Serial.println(F("mc parity must be N, E, or O"));
            return;
        }
        g_mc_settings.baud = static_cast<uint32_t>(baud);
        g_mc_settings.data_bits = static_cast<uint8_t>(data_bits);
        g_mc_settings.parity = parity;
        g_mc_settings.stop_bits = static_cast<uint8_t>(stop_bits);
        (void)applyMcUart();
    } else if (equalsIgnoreCase(tokens[1], "preset")) {
        if (count != 3U) {
            Serial.println(F("mc preset usage: mc preset <iqr-rs232|q-rs232|iqr-rs485|q-rs485>"));
            return;
        }
        applyMcPreset(tokens[2]);
    } else if (equalsIgnoreCase(tokens[1], "read")) {
        uint32_t device = 0U;
        unsigned long points = 1UL;
        if (count < 3U || count > 4U || !parseDDevice(tokens[2], device) ||
            (count == 4U && !parseUnsigned(tokens[3], points)) ||
            points == 0UL || points > kMcMaxReadPoints) {
            Serial.println(F("mc read usage: mc read D100 [points]"));
            return;
        }
        startMcRead(device, static_cast<uint16_t>(points));
    } else if (equalsIgnoreCase(tokens[1], "model")) {
        if (count != 2U) {
            Serial.println(F("mc model usage: mc model"));
            return;
        }
        startMcCpuModelRead();
    } else if (equalsIgnoreCase(tokens[1], "reset")) {
        if (g_mc_client.busy()) {
            Serial.println(F("mc reset rejected: request busy"));
            return;
        }
        (void)applyMcUart();
    } else {
        Serial.println(F("unknown mc command"));
    }
}

void handleCommand(char* line) {
    char* tokens[kMaxTokens] = {};
    const size_t count = tokenize(line, tokens, kMaxTokens);
    if (count == 0U) {
        return;
    }
    const bool safe_while_mc_busy =
        equalsIgnoreCase(tokens[0], "help") || equalsIgnoreCase(tokens[0], "?") ||
        equalsIgnoreCase(tokens[0], "status") ||
        (equalsIgnoreCase(tokens[0], "mc") && count >= 2U &&
         equalsIgnoreCase(tokens[1], "status"));
    if (g_mc_client.busy() && !safe_while_mc_busy) {
        Serial.println(F("command rejected: MC request is active; use mc status and wait for RESULT"));
        return;
    }
    if (equalsIgnoreCase(tokens[0], "help") || equalsIgnoreCase(tokens[0], "?")) {
        printHelp();
    } else if (equalsIgnoreCase(tokens[0], "status")) {
        printStatus();
    } else if (equalsIgnoreCase(tokens[0], "wifi")) {
        handleWifiCommand(tokens, count);
    } else if (equalsIgnoreCase(tokens[0], "slmp")) {
        handleSlmpCommand(tokens, count);
    } else if (equalsIgnoreCase(tokens[0], "mc")) {
        handleMcCommand(tokens, count);
    } else {
        Serial.println(F("unknown command; enter help"));
    }
}

void submitCommand() {
    g_command[g_command_length] = '\0';
    Serial.println();
    handleCommand(g_command);
    g_command_length = 0U;
    memset(g_command, 0, sizeof(g_command));
    printPrompt();
}

void pollUsbConsole() {
    size_t processed = 0U;
    while (Serial.available() > 0 && processed < kMaxUsbBytesPerLoop) {
        const int raw = Serial.read();
        if (raw < 0) {
            break;
        }
        ++processed;
        const char value = static_cast<char>(raw);
        if (g_discard_command_until_eol) {
            if (value == '\r' || value == '\n') {
                if (value == '\n' && g_last_was_cr) {
                    g_last_was_cr = false;
                    continue;
                }
                g_last_was_cr = value == '\r';
                g_discard_command_until_eol = false;
                Serial.println();
                printPrompt();
            }
            continue;
        }
        if (value == '\r' || value == '\n') {
            if (value == '\n' && g_last_was_cr) {
                g_last_was_cr = false;
                continue;
            }
            g_last_was_cr = value == '\r';
            submitCommand();
            if (g_mc_client.busy() || (g_mc_request_done && !g_mc_result_reported)) {
                break;
            }
            continue;
        }
        g_last_was_cr = false;
        if (value == '\b' || value == 127) {
            if (g_command_length > 0U) {
                --g_command_length;
                g_command[g_command_length] = '\0';
            }
            continue;
        }
        if (isprint(static_cast<unsigned char>(value)) == 0 && value != '\t') {
            continue;
        }
        if (g_command_length + 1U >= sizeof(g_command)) {
            Serial.println(F("command too long; discarded"));
            g_command_length = 0U;
            memset(g_command, 0, sizeof(g_command));
            g_discard_command_until_eol = true;
            g_last_was_cr = false;
            continue;
        }
        g_command[g_command_length++] = value;
    }
}

}  // namespace

void setupConsole() {
    Serial.begin(t_rss3_board::kUsbConsoleBaud);
    const uint32_t started = millis();
    while (!Serial && static_cast<uint32_t>(millis() - started) < 2500U) {
        delay(10U);
    }
    strncpy(g_slmp_host, t_rss3_board::kDefaultSlmpHost, sizeof(g_slmp_host) - 1U);
    g_slmp_host[sizeof(g_slmp_host) - 1U] = '\0';
    (void)g_slmp_tcp_client.setTimeoutMs(kDefaultSlmpTimeoutMs);
    (void)g_slmp_udp_client.setTimeoutMs(kDefaultSlmpTimeoutMs);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_OFF);
    pinMode(t_rss3_board::kKeyPin, INPUT_PULLUP);
    pinMode(t_rss3_board::kRs485DirectionPin, OUTPUT);
    digitalWrite(t_rss3_board::kRs485DirectionPin, t_rss3_board::kRs485RxEnable);
    (void)applyMcUart();

    Serial.println();
    Serial.println(F("T-RSS3 ESP32-S3 PLC C++ verification console"));
    Serial.print(F("firmware_build_id="));
    Serial.print(F(T_RSS3_FIRMWARE_BUILD_ID));
    Serial.print(F(" build_env="));
    Serial.print(F(T_RSS3_BUILD_ENV));
    Serial.print(F(" pio_core="));
    Serial.println(F(T_RSS3_PIO_CORE_VERSION));
    Serial.print(F("source_app="));
    Serial.print(F(T_RSS3_APP_SOURCE_ID));
    Serial.print(F(" dependency_slmp="));
    Serial.print(F(T_RSS3_SLMP_SOURCE_ID));
    Serial.print(F(" dependency_mc="));
    Serial.println(F(T_RSS3_MC_SOURCE_ID));
    Serial.print(F("mcprotocol-serial-cpp="));
    Serial.println(MCPROTOCOL_SERIAL_VERSION_STRING);
#if T_RSS3_SLMP_HAS_MXR_RJ71_PROFILE
    Serial.println(F("slmp_surface=current_worktree_with_melsec:mx-r:rj71en71"));
#else
    Serial.println(F("slmp_surface=published_package"));
#endif
    Serial.println(F("boot_policy=no_automatic_plc_communication read_only_commands_only"));
    printHelp();
    printStatus();
    printPrompt();
}

void loopConsole() {
    pumpMcTx();
    pumpMcRx();
    if (g_mc_client.busy()) {
        g_mc_client.poll(millis());
    }
    reportMcResult();
    pollUsbConsole();
    delay(1U);
}

}  // namespace t_rss3_plc_verification_console
