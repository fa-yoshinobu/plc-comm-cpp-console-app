# Console Examples

Each board application owns its source files in its own directory so the reusable C++ library repositories remain small and their CI does not acquire firmware-only files.

## Included examples

- `t_rss3_plc_verification_console`: Wi-Fi SLMP plus RS-232/RS-485 MC serial read verification on the LILYGO T-RSS3 ESP32-S3.
- `stamplc_plc_verification_console`: M5Stack StamPLC application with LCD status pages, RGB state indication, A/B/C navigation, Wi-Fi SLMP, and onboard RS-485 verification.
- `atom_matrix_serial_console`: compact M5Stack Atom SLMP console.
- `w6300_evb_pico2_serial_console`: W6300 SLMP dashboard and console.

## Build

```bash
pio run -e t-rss3-verification-console
pio run -e t-rss3-verification-console-local
pio run -e stamplc-verification-console
pio run -e stamplc-verification-console-local
pio run -e m5stack-atom-console
pio run -e wiznet_6300_evb_pico2
```

See [the T-RSS3 verification guide](../T_RSS3_VERIFICATION.md) or [the StamPLC verification guide](../STAMPLC_VERIFICATION.md) before wiring or running hardware checks.
