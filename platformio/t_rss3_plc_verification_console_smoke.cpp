#include <Arduino.h>

#include "../examples/t_rss3_plc_verification_console/t_rss3_plc_verification_console.h"

void setup() {
    t_rss3_plc_verification_console::setupConsole();
}

void loop() {
    t_rss3_plc_verification_console::loopConsole();
}
