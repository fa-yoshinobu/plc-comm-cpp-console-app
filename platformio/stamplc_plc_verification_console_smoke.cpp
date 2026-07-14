#include <Arduino.h>

#include "../examples/stamplc_plc_verification_console/stamplc_plc_verification_console.h"

void setup() {
    stamplc_plc_verification_console::setupConsole();
}

void loop() {
    stamplc_plc_verification_console::loopConsole();
}
