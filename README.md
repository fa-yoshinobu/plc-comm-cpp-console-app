# PLC Comm C++ Console App

[![ci](https://github.com/fa-yoshinobu/plc-comm-cpp-console-app/actions/workflows/ci.yml/badge.svg)](https://github.com/fa-yoshinobu/plc-comm-cpp-console-app/actions/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Arduino](https://img.shields.io/badge/Arduino-00878F?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-F5822A?logo=platformio&logoColor=white)](https://platformio.org/)

This repository contains ready-to-build applications for checking the PLC communication C++ libraries on supported development boards.

## Start here

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://docs.platformio.org/en/latest/integration/ide/vscode.html).
2. Clone this repository and open its folder in Visual Studio Code.
3. Connect one supported board to the PC with a USB data cable.
4. Open a PlatformIO terminal and build the application for that board.
5. Read the board guide before connecting a PLC or uploading the firmware.

Choose the command that matches your board:

| Board | Build command | Setup guide |
| --- | --- | --- |
| LILYGO T-RSS3 ESP32-S3 | `pio run -e t-rss3-verification-console` | [T-RSS3 guide](T_RSS3_VERIFICATION.md) |
| M5Stack StamPLC | `pio run -e stamplc-verification-console` | [StamPLC guide](STAMPLC_VERIFICATION.md) |

The first build downloads the required board tools and libraries automatically and can take several minutes.

## What the applications check

The T-RSS3 and StamPLC verification applications provide:

- SLMP TCP word reads, TCP keepalive inspection, and SLMP UDP word reads.
- MC protocol C4 ASCII Format 4 CPU-model and word reads.
- Configurable serial baud, data bits, parity, and stop bits.
- Machine-readable `RESULT` output and the effective ESP-IDF UART settings.

T-RSS3 also supports onboard RS-232 and optional external RTS/CTS. StamPLC uses ESP32 hardware half-duplex direction control for its onboard RS-485 port and shows status on its LCD and RGB light; buttons A/C change pages and button B prints status to USB serial.

No PLC request is sent when the board starts, and these applications do not provide a PLC write command. Building or uploading the application therefore does not start a PLC test. A read test begins only after you enter its command in the USB serial console.

## Other supported boards

| Board | Build command | Purpose |
| --- | --- | --- |
| M5Stack Atom Matrix | `pio run -e m5stack-atom-console` | Compact Wi-Fi SLMP console |
| WIZnet W6300-EVB-Pico2 | `pio run -e wiznet_6300_evb_pico2` | Ethernet SLMP dashboard and console |

The W6300 command-line helper is available with `python scripts/w6300_console_cli.py --help`.

## Windows build helper

The helper accepts a short board name:

```bat
build_console.bat trss3
build_console.bat stamplc
build_console.bat atom
build_console.bat w6300
build_console.bat all
```

`build_console.bat all` builds every published-package target. Run `run_ci.bat` to build all targets and analyze the owned T-RSS3 and StamPLC source files.

## Testing current library worktrees

The normal targets above use released library packages. Maintainers can instead test the current sibling `plc-comm-slmp-cpp-minimal` and `plc-comm-mcprotocol-serial-cpp` worktrees, including local SLMP profile changes:

```bash
pio run -e t-rss3-verification-console-local
pio run -e stamplc-verification-console-local
```

Place these three repositories under the same parent folder:

- `plc-comm-cpp-console-app`
- `plc-comm-slmp-cpp-minimal`
- `plc-comm-mcprotocol-serial-cpp`

The Windows helper provides the equivalent commands:

```bat
build_console.bat trss3-local
build_console.bat stamplc-local
```

The local environments read the libraries in place and do not copy or generate files in either library repository. They are excluded from GitHub Actions because clean runners do not contain the sibling worktrees.

Each board application owns its source files under `examples/<board>`. There is no shared example-source directory and no cross-board source dependency.
