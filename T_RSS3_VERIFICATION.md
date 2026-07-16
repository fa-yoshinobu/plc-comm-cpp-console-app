# T-RSS3 C++ PLC Verification

This firmware turns the LILYGO T-RSS3 ESP32-S3 into an explicit-command verification console for `slmp-connect-cpp-minimal` and `mcprotocol-serial-cpp`. It is an evidence-collection application, not a production PLC controller.

## Safety and startup behavior

- Attach the external Wi-Fi antenna before power/reset. The board uses an external-antenna module. This firmware keeps Wi-Fi off until `wifi connect` is entered.
- The firmware does not connect to Wi-Fi or send any PLC frame at boot.
- Only CPU-model reads and direct word reads from `D` devices are exposed. There is no PLC write command.
- The RS-232 loopback command sends a non-MC diagnostic pattern. Run it only after removing the PLC cable and shorting T-RSS3 DB9 pins 2 and 3.
- Uploading or opening the USB monitor does not start a PLC test. A test begins only when its command is entered.
- Do not power the board from multiple sources unless the board documentation confirms the intended power arrangement.

## Board definition and pins

The local PlatformIO board definition is `boards/esp32s3_nopsram.json`: ESP32-S3, 240 MHz, 8 MB flash, DIO at 80 MHz, and no PSRAM. The `status` command reports the detected chip, flash capacity and clock, PSRAM size, and free heap. It does not detect QIO versus DIO. A successful boot of the built image is the runtime check for the configured DIO mode.

The vendor board definition and IDE material say 8 MB/no PSRAM/QIO, while an available schematic label says `ESP32-S3-MINI-1U-N4` and a factory image header says DIO. The verification target uses DIO because the physical T-RSS3 fails during QIO bootloader loading and the factory image also selects DIO. Retain the reported 8 MB capacity and successful DIO boot as the board evidence.

| Function | T-RSS3 connection |
| --- | --- |
| USB CDC console | Native USB, 115200 baud monitor setting |
| Onboard RS-232 TX | DB9 pin 2 |
| Onboard RS-232 RX | DB9 pin 3 |
| RS-232 isolated signal ground | DB9 pin 1 and pin 5 |
| Onboard RS-485 SGND | H2 pin 1 |
| Onboard RS-485 A | H2 pin 2 |
| Onboard RS-485 B | H2 pin 3 |
| RS-485 UART RX/TX/direction | GPIO 2 / GPIO 3 / GPIO 4 |
| Onboard RS-232 UART TX/RX | GPIO 41 / GPIO 42 |

DB9 pin 1 is tied to the board's isolated signal ground, although peer equipment commonly uses pin 1 for DCD or another signal. Use a deliberately wired three-conductor cable carrying only pins 2, 3, and 5, or verify every connected peer pin before using any fuller cable. The T-RSS3 data pins are DCE-style: pin 2 transmits and pin 3 receives. An RJ71C24-R2 direct connection therefore uses straight-through data wiring: T-RSS3 pin 2 to RJ71C24-R2 pin 2 (RD), T-RSS3 pin 3 to RJ71C24-R2 pin 3 (SD), and pin 5 to pin 5 (SG). Do not infer another peer's wiring from DB9 gender. Add RS-485 termination and biasing according to the complete bus, not merely at the ESP32 end.

The T-RSS3 connection exposes no RS-232 hardware-handshake lines. For a three-wire RJ71C24-R2 connection, set RS/CS control to disabled. The RJ71C24-R2 transmission-control selector has no `None` value; select DC-code control, then disable both DC1/DC3 and DC2/DC4 control. Do not select DTR/DSR control for this connection.

H1 accepts 7–24 V DC with pin 1 positive and pin 2 ground. J4 pin 30 is the 3.3 V logic supply and J4 pin 7 is logic ground. GPIOs are 3.3 V only. H2 pin 1 and DB9 pins 1/5 are isolated-side signal ground; never substitute them for J4 logic ground because doing so defeats the onboard isolation boundary.

The onboard DB9 exposes TX, RX, and isolated signal ground only. RTS/CTS testing requires an external 3.3 V UART-to-RS-232 level adapter connected on the J4 logic side, using J4 pin 30 for 3.3 V and pin 7 for logic ground when the adapter requires board power. GPIO 19 and 20 are reserved for native USB and are rejected by the external-interface command. A non-isolated external adapter does not inherit the onboard DB9 isolation.

The external-interface command accepts only GPIOs exposed on J4 and excludes the onboard RS-232, RS-485, key, LED, boot, and USB signals. The accepted GPIOs are 6–18, 21, 33–40, 43, 44, 47, and 48; use four distinct pins.

## Build and monitor

Use the local target when validating current uncommitted library changes. It links the sibling worktrees in place and adds nothing to either library repository:

```bash
pio run -e t-rss3-verification-console-local
```

Use `t-rss3-verification-console` to compile against the exact registry package versions pinned in `platformio.ini`. For a hardware check of the current sibling worktrees, upload the already-built local target and open the monitor as separate operator actions:

```bash
pio run -e t-rss3-verification-console-local -t upload
pio device monitor -b 115200
```

The monitor should show `boot_policy=no_automatic_plc_communication read_only_commands_only`. It also prints a deterministic firmware build ID, the PlatformIO Core version included in that ID, the app commit/dirty-content ID, and dependency material IDs. The local target's dependency IDs contain the SLMP and MC commit/dirty-content IDs; the registry target identifies their pinned registry coordinates. A verification build fails instead of emitting an unknown source ID when required Git provenance cannot be read. Enter `status` and confirm `flash_bytes=8388608`, `psram_bytes=0`, and a successful `uart_effective ... query_ok=1 ... matches_requested=1` line before using the PLC interfaces.

## SLMP commands

Configure Wi-Fi at runtime; credentials are neither compiled into the firmware nor persisted by this application to Wi-Fi NVS:

```text
wifi connect "ssid" "password"
slmp endpoint 192.168.250.100 1025 1035
slmp profile iqr
slmp tcp-read D100
```

Available profile aliases in the published-package target are `iqr`, `iqr-rj71`, `iqf`, `mxr`, `mxf`, `q`, and `l`. The local-worktree target also exposes the profile added in the current SLMP worktree:

```text
slmp profile mxr-rj71
```

This maps to canonical profile name `melsec:mx-r:rj71en71`. Keeping this capability on the local target prevents the registry target from claiming an enum that is absent from the currently published 3.1.0 package.

To verify the ESP32 TCP transport settings and reuse after an idle period:

```text
slmp tcp-keepalive D100 35000
```

A `PASS` proves that a nonzero `SO_KEEPALIVE` value and `TCP_KEEPIDLE=30` were read back, the file descriptor remained the same, and the second PLC read succeeded after the requested idle period. ESP32's lwIP can return its enabled `SOF_KEEPALIVE` bit value `8` instead of a normalized Boolean `1`. Packet-level proof that a keepalive probe appeared on the wire still requires a network capture.

To exercise UDP local-port-zero binding:

```text
slmp udp-read D100
```

A successful reply proves that the socket requested with local port `0` was usable. ESP32 Arduino 2.0.17 does not expose the selected local port through the public `WiFiUDP` API, so evidence of the exact numeric ephemeral port requires capture at the PLC or network side.

## MC serial commands

The MC client exposes two frame/code combinations for the iQ-R and Q profiles: C4 Binary Format 5 and C4 ASCII Format 4. It uses no sum check and the direct host-station route. Presets select C4 Binary Format 5 and start at 19200 baud, 8 data bits, even parity, and 1 stop bit:

```text
mc preset iqr-rs232
mc model
mc read D100 1

mc preset q-rs485
mc model
mc read D100 1
```

Select the frame explicitly when checking a different module configuration. The selected value is included in `mc status` and every completed MC `RESULT` line:

```text
mc frame c4-binary-format5
mc frame c4-ascii-format4
```

Override the UART format when the PLC station is configured differently:

```text
mc serial 19200 7 E 2
mc model
mc read D100 1
```

For an external RTS/CTS adapter, first choose four unused GPIOs in RX, TX, CTS, RTS order:

```text
mc interface external 6 7 8 9
mc serial 19200 8 E 1
mc model
mc read D100 1
```

Every UART change prints both the requested pins/settings and the effective ESP-IDF driver values. A request is rejected unless all effective values match. TX completion has a finite three-second deadline, so missing CTS cannot freeze the console; a timeout destroys the UART session and requires `mc reset`. RS-485 direction control is asserted only around the transmitted request and is returned to receive mode immediately after the UART reports physical TX completion.

To isolate the onboard RS-232 transmitter, receiver, and level-converter path from PLC settings and cable wiring, disconnect the PLC cable, short only DB9 pins 2 and 3 on the T-RSS3, and run:

```text
mc preset iqr-rs232
mc rs232-loopback disconnected-pins-2-3
```

The confirmation argument is intentional: the command emits a 16-byte non-MC diagnostic pattern and must not be used while any PLC or other peer is connected. `PASS` requires an exact 16-byte echo with no missing, changed, or extra bytes. Remove the short before reconnecting the PLC.

## Evidence boundary

Each completed communication command emits one `RESULT test=... status=PASS|FAIL` line. Preserve that line together with the firmware/source IDs, the preceding `status` and `uart_effective` output, the connected PLC/profile, endpoint or wiring, and the tested address.

T-RSS3 results are MCU transport evidence. They do not by themselves close a host-backend requirement that specifically names Windows/POSIX `COM3`, nor do they prove onboard RTS/CTS because the board DB9 has no RTS/CTS signals. Those checks require their named host platform or the documented external adapter, respectively.
