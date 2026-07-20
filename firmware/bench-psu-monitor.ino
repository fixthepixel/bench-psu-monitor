#include <Wire.h>
#include <Adafruit_INA3221.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// ---------- Pins ----------
#define I2C_SDA 21
#define I2C_SCL 22
#define CAL_BUTTON_PIN 0     // BOOT button -> zero calibration
#define WIFI_BUTTON_PIN 23   // long-press -> WiFi config portal
#define PWR_LED_PIN 2
#define INA3221_ADDR 0x40
#define CH1 0

const float SHUNT_R_CH1 = 0.01;

Adafruit_INA3221 ina3221;
Preferences prefs;

AsyncWebServer server(80);   // kept only to host the /ws endpoint
AsyncWebSocket ws("/ws");

float zeroOffset_mA = 0;

unsigned long lastStream = 0;
const unsigned long STREAM_INTERVAL_MS = 100;

unsigned long calPressStart = 0;
bool calHeld = false;
const unsigned long CAL_HOLD_MS = 2000;

unsigned long wifiPressStart = 0;
bool wifiHeld = false;
const unsigned long WIFI_HOLD_MS = 3000;

static bool serverStarted = false;

void doCalibration() {
  Serial.println("Calibrating zero offset...");
  delay(200);
  zeroOffset_mA = ina3221.getCurrentAmps(CH1) * 1000.0;
  prefs.putFloat("ch1_offset", zeroOffset_mA);
  Serial.print("Saved offset: ");
  Serial.println(zeroOffset_mA);
}

void checkCalButton() {
  bool pressed = (digitalRead(CAL_BUTTON_PIN) == LOW);
  if (pressed && !calHeld) { calPressStart = millis(); calHeld = true; }
  if (pressed && calHeld && (millis() - calPressStart >= CAL_HOLD_MS)) {
    doCalibration();
    while (digitalRead(CAL_BUTTON_PIN) == LOW) delay(10);
    calHeld = false;
  }
  if (!pressed) calHeld = false;
}

void startConfigPortal() {
  Serial.println("Starting WiFi config portal (AP: INA3221-Setup)...");
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  bool ok = wm.startConfigPortal("INA3221-Setup");
  if (ok) {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Config portal timed out, rebooting.");
    delay(1000);
    ESP.restart();
  }
}

void checkWifiButton() {
  bool pressed = (digitalRead(WIFI_BUTTON_PIN) == LOW);
  if (pressed && !wifiHeld) { wifiPressStart = millis(); wifiHeld = true; }
  if (pressed && wifiHeld && (millis() - wifiPressStart >= WIFI_HOLD_MS)) {
    while (digitalRead(WIFI_BUTTON_PIN) == LOW) delay(10);
    wifiHeld = false;
    startConfigPortal();
  }
  if (!pressed) wifiHeld = false;
}

void setupWebSocket() {
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("WS client connected: %u\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("WS client disconnected: %u\n", client->id());
    }
  });
  server.addHandler(&ws);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(CAL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PWR_LED_PIN, OUTPUT);

  digitalWrite(PWR_LED_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ina3221.begin(INA3221_ADDR, &Wire)) {
    Serial.println("INA3221 not found.");
    while (1) delay(1000);
  }
  ina3221.setShuntResistance(CH1, SHUNT_R_CH1);

  prefs.begin("ina3221", false);
  zeroOffset_mA = prefs.getFloat("ch1_offset", 0.0);
  Serial.print("Loaded zero offset: ");
  Serial.println(zeroOffset_mA);

  WiFi.mode(WIFI_STA);
  WiFi.begin();
  Serial.println("Attempting saved WiFi connection...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    setupWebSocket();
    serverStarted = true;
  } else {
    Serial.println("No WiFi yet. Hold GPIO23 for 3s to configure.");
  }

  Serial.println("Hold BOOT (GPIO0) 2s to calibrate zero (CH1 unloaded).");

  digitalWrite(PWR_LED_PIN, HIGH);
}

void loop() {
  checkCalButton();
  checkWifiButton();

  if (WiFi.status() == WL_CONNECTED && !serverStarted) {
    setupWebSocket();
    serverStarted = true;
    Serial.print("WS server live at ws://");
    Serial.print(WiFi.localIP());
    Serial.println("/ws");
  }

  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED && (now - lastStream >= STREAM_INTERVAL_MS)) {
    lastStream = now;

    float busVoltage_V = ina3221.getBusVoltage(CH1);
    float rawCurrent_mA = ina3221.getCurrentAmps(CH1) * 1000.0;
    float current_mA = fabs(rawCurrent_mA - zeroOffset_mA);
    float power_mW = busVoltage_V * (current_mA / 1000.0) * 1000.0;

    char json[96];
    snprintf(json, sizeof(json),
             "{\"v\":%.3f,\"i\":%.2f,\"p\":%.1f,\"ts\":%lu}",
             busVoltage_V, current_mA, power_mW, (unsigned long)(millis()));
    ws.textAll(json);
  }

  static unsigned long lastPrint = 0;
  if (now - lastPrint >= 2000) {
    lastPrint = now;
    float busVoltage_V = ina3221.getBusVoltage(CH1);
    float current_mA = fabs(ina3221.getCurrentAmps(CH1) * 1000.0 - zeroOffset_mA);
    Serial.print(busVoltage_V, 3);
    Serial.print("V\t");
    Serial.print(current_mA, 2);
    Serial.println("mA");
  }
}