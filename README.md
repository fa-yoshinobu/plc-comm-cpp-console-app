# PLC Comm C++ Console App

[![ci](https://github.com/fa-yoshinobu/plc-comm-cpp-console-app/actions/workflows/ci.yml/badge.svg)](https://github.com/fa-yoshinobu/plc-comm-cpp-console-app/actions/workflows/ci.yml)

[![C++](https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Arduino](https://img.shields.io/badge/Arduino-00878F?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-F5822A?logo=platformio&logoColor=white)](https://platformio.org/)

Board applications for exercising the PLC communication C++ libraries without adding board-specific files to the library repositories.

## Console targets

- `t-rss3-verification-console`: T-RSS3 ESP32-S3 verification firmware using the published SLMP and MC serial packages.
- `t-rss3-verification-console-local`: the same firmware built from the sibling `plc-comm-slmp-cpp-minimal` and `plc-comm-mcprotocol-serial-cpp` worktrees.
- `m5stack-atom-console`: compact SLMP console.
- `wiznet_6300_evb_pico2`: W6300 SLMP dashboard and console.

The T-RSS3 firmware supports:

- SLMP TCP word reads over Wi-Fi.
- SLMP TCP keepalive inspection plus a second read on the same socket after more than 30 seconds idle.
- SLMP UDP word reads with local port `0` requested for an ephemeral bind.
- MC protocol C4 ASCII Format 4 CPU-model and word reads through onboard RS-232 or RS-485.
- Configurable UART baud, 7/8 data bits, N/E/O parity, 1/2 stop bits, and optional hardware RTS/CTS through an external level adapter.
- Machine-readable `RESULT` lines and ESP-IDF UART effective-setting output.

No PLC frame is sent at boot, and the T-RSS3 console intentionally provides no write command. See [T_RSS3_VERIFICATION.md](T_RSS3_VERIFICATION.md) for wiring, commands, and evidence boundaries.

## Build

Published packages:

```bash
python -m platformio run -e t-rss3-verification-console
```

Current sibling worktrees, including the SLMP profile changes under development:

```bash
python -m platformio run -e t-rss3-verification-console-local
```

Windows helpers:

```bat
build_console.bat trss3
build_console.bat trss3-local
build_console.bat all
```

The local target expects this sibling layout and does not copy or generate files in either library repository:

```text
D:\APP\plc-comm-cpp-console-app
D:\APP\plc-comm-slmp-cpp-minimal
D:\APP\plc-comm-mcprotocol-serial-cpp
```

The published-package target is part of CI. The local target is deliberately excluded from CI because sibling worktrees are not available on a clean GitHub runner.

## Existing targets

```bash
python -m platformio run -e m5stack-atom-console
python -m platformio run -e wiznet_6300_evb_pico2
python scripts/w6300_console_cli.py --help
```

By default the Windows helper scripts use `%~d0\pio` for the PlatformIO cache. Set `PLATFORMIO_CORE_DIR` before running a helper when another cache location is required.
