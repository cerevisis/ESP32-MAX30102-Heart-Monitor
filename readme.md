# Vital Sense: Advanced MAX30102 Heart Monitor `v1.2.0`

A high-performance Heart Rate and SpO2 monitoring system for the ESP32, featuring a real-time web dashboard with dual-transport telemetry (WebSocket + Web Serial).

[View Changelog](file:///c:/Users/educk/Documents/Arduino/synced-projects/MAX30102HeartRate/Heart-Monitor/CHANGELOG.md)

![Dashboard Preview](https://via.placeholder.com/800x400?text=Vital+Sense+Dashboard+Preview)

## ✨ Features

- **Live PPG Waveform**: 50Hz real-time pulse visualization with ultra-smooth batching.
- **Advanced Health Metrics**:
  - **HRV (RMSSD)**: Real-time Heart Rate Variability calculation (ms).
  - **Signal Confidence**: 0-100% reliability score based on pulse strength and stability.
  - **SpO2**: Accurate oxygen saturation tracking.
  - **Perfusion Index (PI)**: Measurement of peripheral blood flow/signal strength.
  - **Die Temperature**: Integrated sensor temperature monitoring.
- **Dual-Transport Telemetry**:
  - **Wireless**: Stable WebSockets with heap protection.
  - **Wired**: Ultra-low latency **Web Serial (USB)** directly to your browser (60fps+).
- **Intelligent Algorithm**:
  - Dynamic noise filtering and jump rejection.
  - 10-sample rolling average for stable BPM.
  - Auto-recovery for I2C and sensor "finger off" states.
- **Premium Dashboard**:
  - Modern CSS Glassmorphism design with Dark Mode.
  - Real-time Chart.js integration with optimized scaling.
  - Responsive layout for desktop and tablet.

## 🚀 Getting Started

### Prerequisites
- **ESP32** (DevKit V1 or similar)
- **MAX30102** or **MAX30105** sensor
- Arduino IDE with the following libraries:
  - `SparkFun MAX3010x Pulse and Proximity Sensor Library`
  - `ESPAsyncWebServer`
  - `AsyncTCP`

### Installation
1. Clone this repository.
2. Open `Heart-Monitor.ino`.
3. Set your WiFi credentials in the `WiFi Credentials` section.
4. Upload to your ESP32.
5. Open the IP address shown in the Serial Monitor (e.g., `http://192.168.1.50`).

### 🔌 Using Web Serial (Insecure Origin Setup)
Web Serial is a powerful feature but is restricted to **Secure Contexts** (HTTPS or localhost) by default. Since the ESP32 typically serves via a local IP (`http://...`), you must manually whitelist it in your browser:

1.  **Open Browser Flags**:
    *   **Chrome**: Paste `chrome://flags/#unsafely-treat-insecure-origin-as-secure` into the address bar.
    *   **Edge**: Paste `edge://flags/#unsafely-treat-insecure-origin-as-secure` into the address bar.
2.  **Enable the Flag**:
    *   Find the **"Insecure origins treated as secure"** setting and set it to **Enabled**.
3.  **Whitelist your IP**:
    *   Add your ESP32's address to the text box (e.g., `http://192.168.1.xx`).
    *   *Tip: You can add multiple IPs separated by commas.*
4.  **Relaunch**: Click the **Relaunch** button at the bottom of the browser.
5.  **Connect**: On the dashboard, click **"Connect via USB"** and select your ESP32's COM port.

## 🛠️ Hardware Wiring
- **VIN** -> 3.3V
- **GND** -> GND
- **SDA** -> GPIO 21 (ESP32)
- **SCL** -> GPIO 22 (ESP32)

## 📜 License
MIT License - See [LICENSE](LICENSE) for details.
