# M5Stack StamPLC C++ PLC Verification

This firmware turns the M5Stack StamPLC (SKU K141) into an explicit-command verification console for `slmp-connect-cpp-minimal` and `mcprotocol-serial-cpp`. It is an evidence-collection application, not production-control firmware.

## Safety and startup behavior

- The firmware does not connect to Wi-Fi or send a PLC frame at boot.
- Only MC CPU-model reads and direct word reads from `D` devices are exposed. There is no PLC write command.
- Uploading or opening the USB monitor does not start a PLC test. A test begins only when its command is entered.
- The application initializes the LCD, RGB status light, and A/B/C buttons through M5Unified. It does not start the factory Modbus slave or initialize the relay/input expander.
- StamPLC accepts DC 6–36 V input. The PWR-485 power pins are connected directly to the device input supply, so confirm that the selected input voltage is safe for every externally powered device before connecting it.

The authoritative product specifications, power warning, pin map, and PlatformIO starting configuration are in the [M5Stack StamPLC documentation](https://docs.m5stack.com/ja/core/StamPLC).

## Board definition and RS-485 pins

The target uses PlatformIO's `esp32-s3-devkitc-1` board with native USB CDC enabled, matching the official M5Stack PlatformIO example. StamPLC uses an ESP32-S3FN8 with 8 MB flash and no external PSRAM.

| Function | StamPLC connection |
| --- | --- |
| USB CDC console | Native USB, 115200 baud monitor setting |
| PWR-485 TX / BOOT | GPIO0 |
| PWR-485 RX | GPIO39 |
| PWR-485 direction | GPIO46 |

The pins match the M5Stack product pin map and the tagged [M5StamPLC 1.2.0 pin configuration](https://github.com/m5stack/M5StamPLC/blob/1.2.0/src/pin_config.h). The console configures UART1 with ESP-IDF `UART_MODE_RS485_HALF_DUPLEX`, as used by the tagged [M5StamPLC implementation](https://github.com/m5stack/M5StamPLC/blob/1.2.0/src/M5StamPLC.cpp). It therefore does not install the manual direction callbacks used by the T-RSS3 target.

StamPLC has no approved onboard RS-232 or external RTS/CTS path in this application. The corresponding commands are omitted from `help` and explicitly rejected if entered.

## LCD, RGB, and buttons

The LCD has three pages: communication status, board status, and local controls. It refreshes automatically without starting a PLC request.

- Button A selects the previous page.
- Button B prints the current full status to the USB serial console and refreshes the LCD.
- Button C selects the next page.
- Blue RGB means the MC UART is ready while Wi-Fi is disconnected.
- Green means Wi-Fi is connected.
- Yellow means an MC serial request is active.
- Red means the MC UART is unavailable or requires a transport reset.

Button actions never issue PLC reads or writes. PLC requests remain explicit USB serial commands.

## Build and monitor

Build against the pinned registry packages:

```bash
pio run -e stamplc-verification-console
```

Build against the current sibling library worktrees, including profile changes that are not yet in the registry packages:

```bash
pio run -e stamplc-verification-console-local
```

Uploading and opening the monitor are separate operator actions:

```bash
pio run -e stamplc-verification-console-local -t upload
pio device monitor -b 115200
```

The monitor should show `boot_policy=no_automatic_plc_communication read_only_commands_only`. Enter `status` and confirm all of the following before using the PLC interfaces:

```text
board_model=m5stack-stamplc
flash_bytes=8388608
psram_bytes=0
rs485_direction=hardware-half-duplex
matches_requested=1
```

The firmware also prints a deterministic build ID, PlatformIO Core version, application source ID, and dependency material IDs. A local build records the current sibling worktree commit and dirty-content identities; a registry build records the pinned package coordinates. StamPLC builds also record the pinned M5Unified and M5GFX versions.

## SLMP commands

Configure Wi-Fi at runtime. Credentials are not compiled into the firmware and the application disables Wi-Fi persistence:

```text
wifi connect "ssid" "password"
slmp endpoint 192.168.250.100 1025 1035
slmp profile iqr
slmp tcp-read D100
slmp udp-read D100
slmp tcp-keepalive D100 35000
```

The local-worktree target also exposes SLMP profile surface that exists only in the current sibling worktree. `help` reports the exact aliases compiled into the selected target.

## MC serial commands

Connect the PLC serial bus to the PWR-485 connector, including the appropriate common/reference conductor, termination, and bias for the complete bus. Then select the PLC profile and serial settings before issuing a read:

```text
mc interface rs485
mc profile iqr
mc serial 19200 8 E 1
mc model
mc read D100 1
```

Preset forms are also available:

```text
mc preset iqr-rs485
mc preset q-rs485
```

The console supports only MC protocol C4 ASCII Format 4 over this interface. It does not run Modbus RTU and does not activate the Modbus slave described for M5Stack's factory firmware.

## Evidence boundary

A successful build proves compilation and static compatibility for the selected dependency set. A boot/status capture proves the detected flash/PSRAM values and UART configuration. Neither is PLC compatibility evidence. PLC compatibility requires a separately authorized live test against the named PLC/profile and endpoint; no live communication is performed by the build or CI workflow.
