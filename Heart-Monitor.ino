#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"  // Includes algorithm for calculating heart rate

// WiFi Credentials
const char *ssid = "56k or Broke";
const char *password = "L@yla25!";

MAX30105 particleSensor;

// Heart Rate variables
const byte RATE_SIZE = 10;  // Increased for better averaging
byte rates[RATE_SIZE];      // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;  // Time at which the last beat occurred

float beatsPerMinute;
int beatAvg = 0;
float lastSpO2 = 0;
unsigned long dynamicNotifyInterval = 33; // Default 30Hz
unsigned long wsMsgCount = 0;

// Web Server and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define debug Serial

#include "web_index.h"

void notifyClients(long irValue) {
  if (ws.count() > 0) {
    char json[96];
    snprintf(json, sizeof(json), "{\"bpm\":%d,\"spo2\":%.1f,\"ir\":%ld}", beatAvg, lastSpO2, irValue);
    ws.textAll(json);
    wsMsgCount++;
  }
}

void setup() {
  debug.begin(115200);
  debug.println("Initializing Vital Sense...");

  // WiFi Setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debug.print(".");
  }
  debug.println("");
  debug.println("WiFi connected.");
  debug.println("IP address: ");
  debug.println(WiFi.localIP());

  // Sensor Setup
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    debug.println("MAX30102 not found. Check wiring!");
    while (1)
      ;
  }

  // Power: 0x24 (approx 7mA - moderate for stability), Avg: 4, Mode: 2 (Red+IR), Rate: 200Hz, PW: 411, ADC: 4096
  particleSensor.setup(0x24, 4, 2, 200, 411, 4096);

  particleSensor.wakeUp();
  debug.println("Sensor Initialized (200Hz, Stable Power).");

  // Web Server Setup
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        debug.printf("WS: Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT: {
        uint16_t reason = 0;
        if (arg != NULL && len >= 2) {
          reason = (data[0] << 8) | data[1];
        }
        debug.printf("WS: Client #%u disconnected (Reason: %u)\n", client->id(), reason);
        break;
      }
      case WS_EVT_DATA: {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
          data[len] = 0;
          String msg = (char*)data;
          if (msg.startsWith("{\"freq\":")) {
            int freq = msg.substring(8, msg.length() - 1).toInt();
            if (freq >= 1 && freq <= 100) {
              dynamicNotifyInterval = 1000 / freq;
              debug.printf("WS: Frequency set to %d Hz (%d ms)\n", freq, dynamicNotifyInterval);
            }
          }
        }
        break;
      }
      case WS_EVT_PONG:
        debug.printf("WS: Client #%u pong\n", client->id());
        break;
      case WS_EVT_ERROR:
        debug.printf("WS: Client #%u error(%u): %s\n", client->id(), *((uint16_t *)arg), (char *)data);
        break;
    }
  });
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
  delay(1);

  particleSensor.check();  // Check sensor for new data
  static long currentIR = 0; // Static to persist last real reading

  while (particleSensor.available()) {
    long irValue = particleSensor.getFIFOIR();
    long redValue = particleSensor.getFIFORed();
    currentIR = irValue;  // We use the latest sample for threshold logic

    if (checkForBeat(irValue) == true) {
      if (lastBeat == 0) {
        lastBeat = millis();  // First beat: start the clock
        debug.println(" [Heart Pulse Detected!]");
      } else {
        long delta = millis() - lastBeat;
        if (delta > 250) {
          lastBeat = millis();
          beatsPerMinute = 60 / (delta / 1000.0);

          if (beatsPerMinute < 220 && beatsPerMinute > 40) {
            bool acceptValue = false;
            if (beatAvg == 0) {
              acceptValue = true;
            } else {
              if (beatsPerMinute > (beatAvg * 0.7) && beatsPerMinute < (beatAvg * 1.3)) {
                acceptValue = true;
              }
            }

            if (acceptValue) {
              rates[rateSpot++] = (byte)beatsPerMinute;
              rateSpot %= RATE_SIZE;
              beatAvg = 0;
              int count = 0;
              for (byte x = 0; x < RATE_SIZE; x++) {
                if (rates[x] > 0) {
                  beatAvg += rates[x];
                  count++;
                }
              }
              if (count > 0) beatAvg /= count;
            }
          }
        }
      }
    }

    // Basic SpO2 update
    static unsigned long lastSpO2Time = 0;
    if (irValue > 10000 && millis() - lastSpO2Time > 1000) {
      float R = (float)redValue / (float)irValue;
      lastSpO2 = 104.0 - 17.0 * R;
      if (lastSpO2 > 100) lastSpO2 = 100;
      lastSpO2Time = millis();
    }

    particleSensor.nextSample();  // Advance to next sample in FIFO
  }

  // Watchdog & Finger Detection
  static unsigned long lastCommTime = millis();
  static unsigned long fingerOffTime = 0;
  static unsigned long lastNoFingerPrint = 0;

  int samplesRead = particleSensor.check();
  if (samplesRead > 0) {
    lastCommTime = millis();
  } else if (millis() - lastCommTime > 5000) {
    debug.println(">>> I2C Communication Lost! Attempting recovery...");
    if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
      particleSensor.setup(0x24, 4, 2, 200, 411, 4096);
      particleSensor.wakeUp();
    }
    lastCommTime = millis();
  }

  if (currentIR < 10000) {
    if (fingerOffTime == 0) fingerOffTime = millis();
    if (millis() - fingerOffTime > 1000) {        // 1 second debounce
      if (millis() - lastNoFingerPrint > 2000) {  // Only print every 2s
        if (currentIR == 0) debug.println("! Wait for sensor...");
        else {
          debug.print(" Idle (IR=");
          debug.print(currentIR);
          debug.println(")");
        }
        lastNoFingerPrint = millis();
      }

      beatAvg = 0;
      beatsPerMinute = 0;
      lastSpO2 = 0;
      lastBeat = 0;
      for (byte x = 0; x < RATE_SIZE; x++) rates[x] = 0;
      rateSpot = 0;
    }
  } else {
    fingerOffTime = 0;
    if (beatAvg == 0) {
      static unsigned long lastWaitPrint = 0;
      if (millis() - lastWaitPrint > 1000) {
        debug.println(" [Finger detected, waiting for pulse lock...]");
        lastWaitPrint = millis();
      }
    }
  }

  static unsigned long lastNotifyTime = 0;
  static unsigned long lastSerialPrint = 0;
  unsigned long notifyInterval = (currentIR > 10000) ? dynamicNotifyInterval : 500;
  
  if (millis() - lastNotifyTime > notifyInterval) {
    notifyClients(currentIR);
    lastNotifyTime = millis();
  }
  if (millis() - lastSerialPrint > 500) { // Slower serial logging
    lastSerialPrint = millis();
    debug.print(" IR[");
    debug.print(currentIR);
    debug.print("]");
    debug.print(" BPM=");
    debug.print(beatAvg);
    debug.print(" (Raw=");
    debug.print(beatsPerMinute);
    debug.print("), SpO2=");
    debug.print(lastSpO2);
    debug.print(" | Heap: ");
    debug.print(ESP.getFreeHeap());
    debug.println("");
  }

  // WS Transmission Telemetry
  static unsigned long lastWsStats = 0;
  if (millis() - lastWsStats > 1000) {
    if (ws.count() > 0) {
      debug.printf(">>> WS Status: %d clients, Rate: %lu msgs/sec | Heap: %d\n", ws.count(), wsMsgCount, ESP.getFreeHeap());
      // Print one sample JSON for verification
      char sample[96];
      snprintf(sample, sizeof(sample), "{\"bpm\":%d,\"spo2\":%.1f,\"ir\":%ld}", beatAvg, lastSpO2, currentIR);
      debug.printf(">>> Sample Payload: %s\n", sample);
    }
    wsMsgCount = 0;
    lastWsStats = millis();
  }
}