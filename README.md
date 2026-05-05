# SLMP C++ Minimal Console App

[![ci](https://github.com/fa-yoshinobu/plc-comm-slmp-cpp-minimal-console-app/actions/workflows/ci.yml/badge.svg)](https://github.com/fa-yoshinobu/plc-comm-slmp-cpp-minimal-console-app/actions/workflows/ci.yml)

[![Python](https://img.shields.io/badge/Python-3776AB?logo=python&logoColor=white)](https://www.python.org/)
[![C++](https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Arduino](https://img.shields.io/badge/Arduino-00878F?logo=arduino&logoColor=white)](https://www.arduino.cc/)

Interactive console applications for `plc-comm-slmp-cpp-minimal`.

This repository hosts the board-specific console programs that were split out of the minimal library repository so the library can stay focused on the core and high-level API.

## Included Console Targets

- `examples/w6300_evb_pico2_serial_console`
- `examples/atom_matrix_serial_console`

The W6300 target is the primary demo application. It now focuses on a richer ANSI dashboard that shows:

- live connection and route settings
- quick read / quick write actions
- watch slots and batch reads
- stress counters and latency
- system metrics such as estimated CPU load and free heap

The Atom Matrix target is intentionally minimal and keeps only basic SLMP connectivity and read/write commands.

## Dependency

The console sketches depend on the SLMP minimal library:

- PlatformIO Registry: <https://registry.platformio.org/libraries/fa-yoshinobu/slmp-connect-cpp-minimal>

PlatformIO uses the registry package in `platformio.ini`:

```ini
lib_deps =
  https://github.com/fa-yoshinobu/plc-comm-slmp-cpp-minimal.git#0.4.9
```

## Quick Start

```bash
python -m platformio run -e m5stack-atom-console
python -m platformio run -e wiznet_6300_evb_pico2
```

For repeated local builds on Windows, use the helper script so the same PlatformIO cache directory is reused:

```bat
build_console.bat
build_console.bat w6300
build_console.bat atom
```

By default the helper scripts use a short cache path on the current drive:

```text
%~d0\pio
```

If another machine needs a different cache location, set `PLATFORMIO_CORE_DIR` before running the scripts.

`cmd.exe`:

```bat
set PLATFORMIO_CORE_DIR=E:\pio-cache
build_console.bat w6300
```

PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = 'E:\pio-cache'
cmd /c build_console.bat w6300
```

For the W6300 console CLI:

```bash
python scripts/w6300_console_cli.py --help
```
