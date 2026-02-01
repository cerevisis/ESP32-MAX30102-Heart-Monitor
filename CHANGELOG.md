# Changelog

All notable changes to the Vital Sense Heart Monitor project will be documented in this file.

## [1.2.0] - 2026-01-30
### Added
- **Web Serial Support**: Implemented direct USB-to-Browser telemetry for ultra-low latency monitoring.
- **Batched IR Telemetry**: Samples recorded at 50Hz and transmitted in batches of 5 (10Hz) to reduce network overhead by 80%.
- **Secure Context Workaround**: Integrated UI guidance for enabling Web Serial on local IP addresses via browser flags.
- **Heap Protection**: Added `WS_MAX_QUEUED_MESSAGES` limit (15) to prevent crashes during connection instability.

### Changed
- **Modular Refactoring**: Decoupled the monolithic `loop()` into `readSensor()`, `handleWatchdog()`, and `broadcastTelemetry()`.
- **Transmission Decoupling**: Pulse data (high frequency) and Metrics (BPM/SpO2, 500ms) are now sent as separate JSON streams for better reliability.

## [1.1.0] - 2026-01-29
### Added
- **Real-time PPG Chart**: Added a live waveform chart using Chart.js.
- **I2C Auto-Recovery**: Firmware now detects sensor disconnection and attempts to re-initialize the MAX30102.
- **Finger Detection State Machine**: Improved "Idle" vs "Reading" detection with hysteresis logic.
- **Dual-Speed Telemetry**: Decoupled Serial and WebSocket pulse rates. Serial now transmits at 50Hz individual frames for maximum USB smoothness, while WebSockets use 10Hz batching for wireless stability.

### Fixed
- **BPM Stability**: Implemented refractory period and improved averaging to eliminate "double peaks".
- **Saturation Fix**: Lowered LED power to `0x24` (7mA) to resolve the `262143` ADC saturation issue.

## [1.0.0] - Initial Release
### Added
- Basic MAX30102 sensor reading.
- WebSocket streaming of BPM and SpO2 values.
- Foundation of the web dashboard.
