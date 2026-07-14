#pragma once

#include <stdint.h>

namespace stamplc_protocol_console {

struct ConsoleSnapshot {
    bool wifi_connected;
    const char* slmp_host;
    uint16_t slmp_tcp_port;
    uint16_t slmp_udp_port;
    const char* slmp_profile;
    const char* mc_interface;
    const char* mc_profile;
    uint32_t mc_baud;
    uint8_t mc_data_bits;
    char mc_parity;
    uint8_t mc_stop_bits;
    bool mc_ready;
    bool mc_busy;
    bool mc_reset_required;
};

void setupConsole();
void loopConsole();
ConsoleSnapshot snapshot();
void printConsoleStatus();

}  // namespace stamplc_protocol_console
