#include <WiFi.h>
#include <Wire.h>
#define WS_MAX_QUEUED_MESSAGES 15 // Limit to prevent heap overflow
#include <ESPAsyncWebServer.h>
#include "MAX30105.h"
#include "heartRate.h"  // Includes algorithm for calculating heart rate

#include "web_index.h"
#include "secrets.h"

MAX30105 particleSensor;

// Heart Rate variables
const byte RATE_SIZE = 10;  // Increased for better averaging
byte rates[RATE_SIZE];      // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;  // Time at which the last beat occurred

float beatsPerMinute;
int beatAvg = 0;
unsigned long wsMsgCount = 0;

float lastSpO2 = 0;
float lastPI = 0;
float lastTemp = 0;
float lastHRV = 0;
int lastConfidence = 0;
String currentStatus = "Idle";
int currentRSSI = 0;
unsigned long lastIBI = 0;
float sqDiffs[16];
int diffSpot = 0;
unsigned long dynamicNotifyInterval = 33;

long irBatch[5]; // Buffer for 5 samples
int irBatchIdx = 0;
unsigned long lastSampleTime = 0;

// Web Server and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define debug Serial

// 1. High-frequency Serial for Web Serial (Low-latency)
void sendSerialPulse(long irValue) {
  debug.printf(">DATA:{\"ir\":%ld}\n", irValue);
}

// 2. Batched WebSocket (Network efficient)
void sendPulse() {
  if (ws.count() > 0 && ws.getQueueLen() < WS_MAX_QUEUED_MESSAGES) {
    String json = "{\"ir\":[";
    for(int i=0; i<5; i++) {
      json += String(irBatch[i]);
      if(i < 4) json += ",";
    }
    json += "]}";
    ws.textAll(json);
    wsMsgCount++;
  }
}

void sendMetrics() {
  char json[256];
  snprintf(json, sizeof(json), "{\"bpm\":%d,\"spo2\":%.1f,\"pi\":%.2f,\"temp\":%.1f,\"hrv\":%.1f,\"conf\":%d,\"status\":\"%s\",\"rssi\":%d}", 
           beatAvg, lastSpO2, lastPI, lastTemp, lastHRV, lastConfidence, currentStatus.c_str(), currentRSSI);
  
  // WebSocket
  if (ws.count() > 0 && ws.getQueueLen() < WS_MAX_QUEUED_MESSAGES) {
    ws.textAll(json);
    wsMsgCount++;
  }
  // Serial
  debug.printf(">DATA:%s\n", json);
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
      case WS_EVT_DISCONNECT:
        {
          uint16_t reason = 0;
          if (arg != NULL && len >= 2) {
            reason = (data[0] << 8) | data[1];
          }
          debug.printf("WS: Client #%u disconnected (Reason: %u)\n", client->id(), reason);
          break;
        }
      case WS_EVT_DATA:
        {
          AwsFrameInfo *info = (AwsFrameInfo *)arg;
          if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            String msg = (char *)data;
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

long currentIR = 0;  // Global to share between modules

void readSensor() {
  particleSensor.check();
  static long minIR = 262143;
  static long maxIR = 0;

  while (particleSensor.available()) {
    long irValue = particleSensor.getFIFOIR();
    long redValue = particleSensor.getFIFORed();
    currentIR = irValue;

    // Track min/max for Perfusion Index (PI) calculation
    if (irValue > 10000) {
      if (irValue < minIR) minIR = irValue;
      if (irValue > maxIR) maxIR = irValue;
    }

    if (checkForBeat(irValue) == true) {
      // Calculate Perfusion Index (PI) for the finished beat
      if (maxIR > minIR && minIR > 10000) {
        long ac = maxIR - minIR;
        long dc = (maxIR + minIR) / 2;
        lastPI = ((float)ac / (float)dc) * 100.0;
      }
      // Reset for next beat
      minIR = 262143;
      maxIR = 0;

      if (lastBeat == 0) {
        lastBeat = millis();
        currentStatus = "Seeking Pulse";
        debug.println(" [Heart Pulse Detected!]");
      } else {
        long delta = millis() - lastBeat;
        if (delta > 250) {
          lastBeat = millis();
          beatsPerMinute = 60 / (delta / 1000.0);
          if (beatsPerMinute < 220 && beatsPerMinute > 40) {
            // 1. HRV Calculation (RMSSD)
            if (lastIBI > 0) {
              long diff = (long)delta - (long)lastIBI;
              sqDiffs[diffSpot++] = (float)(diff * diff);
              diffSpot %= 16;
              
              float sum = 0;
              int count = 0;
              for(int i=0; i<16; i++) {
                if(sqDiffs[i] > 0) { sum += sqDiffs[i]; count++; }
              }
              if (count > 0) lastHRV = sqrt(sum / count);
            }
            lastIBI = delta;

            // 2. Confidence Score (0-100)
            float bpmDiff = abs(beatsPerMinute - beatAvg);
            float bpmConf = (beatAvg > 0) ? (1.0 - (bpmDiff / (float)beatAvg)) : 0.8;
            if (bpmConf < 0) bpmConf = 0;
            
            float piConf = 0;
            if (lastPI > 0.5 && lastPI < 10.0) piConf = 1.0;
            else if (lastPI > 0.2) piConf = 0.5;
            
            lastConfidence = (int)((bpmConf * 0.6 + piConf * 0.4) * 100);
            if (lastConfidence > 100) lastConfidence = 100;

            // 3. Update BPM Buffer
            bool acceptValue = (beatAvg == 0) || (beatsPerMinute > (beatAvg * 0.7) && beatsPerMinute < (beatAvg * 1.3));
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
              currentStatus = "Pulse Acquired";
            }
          }
        }
      }
    }
    static unsigned long lastSpO2Time = 0;
    if (irValue > 10000 && millis() - lastSpO2Time > 1000) {
      float R = (float)redValue / (float)irValue;
      lastSpO2 = 104.0 - 17.0 * R;
      if (lastSpO2 > 100) lastSpO2 = 100;
      lastSpO2Time = millis();
    }
    particleSensor.nextSample();
  }
}

void handleWatchdog() {
  static unsigned long lastCommTime = millis();
  static unsigned long fingerOffTime = 0;
  static unsigned long lastNoFingerPrint = 0;

  if (particleSensor.check() > 0) lastCommTime = millis();
  else if (millis() - lastCommTime > 5000) {
    debug.println(">>> I2C Recovery...");
    if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
      particleSensor.setup(0x24, 4, 2, 200, 411, 4096);
      particleSensor.wakeUp();
    }
    lastCommTime = millis();
  }

  if (currentIR < 10000) {
    if (fingerOffTime == 0) fingerOffTime = millis();
    if (millis() - fingerOffTime > 1000) {
      if (millis() - lastNoFingerPrint > 2000) {
        if (currentIR == 0) debug.println("! Sensor Busy...");
        else debug.printf(" Idle (IR=%ld)\n", currentIR);
        lastNoFingerPrint = millis();
      }
      beatAvg = 0;
      beatsPerMinute = 0;
      lastSpO2 = 0;
      lastBeat = 0;
      lastHRV = 0;
      lastIBI = 0;
      lastConfidence = 0;
      currentStatus = "Idle";
      for (int i = 0; i < 16; i++) sqDiffs[i] = 0;
      for (byte x = 0; x < RATE_SIZE; x++) rates[x] = 0;
      rateSpot = 0;
    }
  } else {
    fingerOffTime = 0;
    static unsigned long lastWaitPrint = 0;
    if (beatAvg == 0 && millis() - lastWaitPrint > 1000) {
      debug.println(" [Reading Pulse...]");
      lastWaitPrint = millis();
    }
  }
}

void broadcastTelemetry() {
  static unsigned long lastPulseTime = 0;
  static unsigned long lastMetricsTime = 0;
  static unsigned long lastSerialPrint = 0;
  static unsigned long lastWsStats = 0;
  static unsigned long lastTempRead = 0;

  // 0. Periodic Sensor Tasks (Temperature)
  if (millis() - lastTempRead > 2000) {
    lastTemp = particleSensor.readTemperature();
    currentRSSI = WiFi.RSSI();
    lastTempRead = millis();
  }

  // 1. High Frequency Tasks (50Hz / 20ms)
  if (currentIR > 10000 && (millis() - lastSampleTime >= 20)) {
    lastSampleTime = millis();
    
    // Always send individual frame to Serial for USB smoothness
    sendSerialPulse(currentIR);

    // Batch for WebSocket stability
    irBatch[irBatchIdx++] = currentIR;
    if (irBatchIdx >= 5) {
      sendPulse();
      irBatchIdx = 0;
      lastPulseTime = millis();
    }
  }

  // 3. Metrics (Stable 500ms)
  if (millis() - lastMetricsTime > 500) {
    sendMetrics();
    lastMetricsTime = millis();
  }
  if (millis() - lastSerialPrint > 500) {
    lastSerialPrint = millis();
    debug.printf(" IR[%ld] BPM=%d (Raw=%.1f), SpO2=%.1f | Heap: %d\n", currentIR, beatAvg, beatsPerMinute, lastSpO2, ESP.getFreeHeap());
  }
  if (millis() - lastWsStats > 1000) {
    if (ws.count() > 0) debug.printf(">>> WS: %d clips, %lu msgs/sec | Int: %lu ms\n", ws.count(), wsMsgCount, dynamicNotifyInterval);
    wsMsgCount = 0;
    lastWsStats = millis();
  }
}

void loop() {
  ws.cleanupClients();
  readSensor();
  handleWatchdog();
  broadcastTelemetry();
  delay(1);
}