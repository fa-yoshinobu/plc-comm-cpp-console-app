# T-RSS3 Live Verification Evidence

This record contains maintainer evidence for communication performed by the T-RSS3 verification console. It intentionally omits PC-specific filesystem paths and USB port names.

## 2026-07-18: MELSEC iQ-R with RJ71C24-R4 CH1 over RS-485

### Scope and target contract

- Target PLC: `R120PCPU` with `RJ71C24-R4`, CH1.
- Endpoint: T-RSS3 onboard H2 two-wire RS-485 interface.
- Serial settings: `19200 / 8E1`, no flow control, manual RS-485 direction hooks.
- Protocol: `melsec:iq-r`, C4 binary Format 5, sum check disabled, direct host-station route.
- Firmware source: `plc-comm-cpp-console-app` commit `0ac9a5f` using the registry target with `slmp-connect-cpp-minimal@3.1.0` and `mcprotocol-serial-cpp@3.1.0`.
- Read scope: CPU model and `D100`, one word. No PLC write was sent.

### Accepted evidence

- [x] Runtime status reported ESP32-S3, 8 MiB flash, no PSRAM, `rs485-onboard`, `19200 / 8E1`, no flow control, manual direction hooks, and effective UART values matching the request.
- [x] CPU-model read returned `R120PCPU` with model code `0x4844`.
- [x] Single-word read of `D100` returned `0` with normal completion.
- [x] Three sequential stability batches read `D100` ten times at one-second intervals. All 30 reads passed, all values were `0`, and no timeout or transport reset occurred.
- [x] No write or restoration action was required.

### Setup findings and disposition

- Initial timeouts and module framing/command-error indications occurred before the physical wiring was corrected. The operator identified the wiring as the cause. The corrected connection then completed the accepted reads and 30/30 stability run, so those setup attempts are not classified as protocol or library failures.
- One result emitted after a T-RSS3 reset showed `interface=rs232-onboard`; it is excluded from RS-485 evidence. The firmware starts with its onboard RS-232 interface selected after a reset, so an operator must apply `mc preset iqr-rs485` again before an RS-485 test.
- The T-RSS3 onboard RS-232 path with `RJ71C24-R2` was not retested because the required physical wiring was unavailable. Its status for this board-specific session is `not tested`, not pass, fail, or unsupported. This does not reopen the independent Windows/POSIX RS-232 evidence already accepted by `mcprotocol-serial-cpp`.

### Final disposition

The T-RSS3 onboard RS-485 path is accepted for the tested `R120PCPU` and `RJ71C24-R4` CH1 configuration. No implementation change is required from this live batch, and no active release-blocking item remains from it.
