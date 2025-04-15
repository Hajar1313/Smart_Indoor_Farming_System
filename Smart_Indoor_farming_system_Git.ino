#define BLYNK_TEMPLATE_ID    "TMPL5ipTRAu_8"
#define BLYNK_TEMPLATE_NAME  "Smart indoor farming system"
#define DEFAULT_BLYNK_AUTH   "3Om1xAn_Y-5CpHcDgMxS_9YzRigD_b5D"

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <BlynkSimpleEsp32.h>
#include <EEPROM.h>

#define LDR_PIN            D2
#define SOIL_MOISTURE_PIN  A1
#define PUMP_PIN           D11
#define LED_PIN            D12 

const unsigned long MS_IN_HOUR = 3600000UL;
const unsigned long ONE_DAY_MS = 24UL * MS_IN_HOUR;

Preferences preferences;
String savedSSID, savedPass, savedBlynkAuth;
WiFiManager wm;
WiFiManagerParameter custom_blynk("auth", "Blynk Auth Token", "", 40);
BlynkTimer timer;

unsigned long lightOnStartTime = 0;
unsigned long totalLightOnTimeToday = 0;
unsigned long lastUpdateTime = 0;
unsigned long dayStartTime = 0;
bool isLightOn = false;
bool isManualMode = false;

float moistureThreshold;
float dailyLightHours = 12.0;
unsigned long dailyLightDuration = dailyLightHours * MS_IN_HOUR;

void updateDailyLightDuration() {
  dailyLightDuration = (unsigned long)(dailyLightHours * MS_IN_HOUR);
}

void controlDevices(int soilMoisture, int lightStatus, unsigned long dt) {
  digitalWrite(PUMP_PIN, soilMoisture > moistureThreshold ? HIGH : LOW);

  if (totalLightOnTimeToday < dailyLightDuration) {
    if (lightStatus == 0) { //Natural Light
      if (isLightOn) {
        digitalWrite(LED_PIN, LOW);
        Serial.print("Light OFF");
        isLightOn = false;
      }
      totalLightOnTimeToday += dt;
    } else {
      if (!isLightOn) {
        digitalWrite(LED_PIN, HIGH);
        isLightOn = true;
        Serial.print("Light ON ");
        lightOnStartTime = millis();
      }
      totalLightOnTimeToday += dt;
    }
  } else if (isLightOn) {
    digitalWrite(LED_PIN, LOW);
    Serial.print("Light OFF");
    isLightOn = false;
  }
}

void sendSensorData() {
  if ((millis() - dayStartTime) >= ONE_DAY_MS) {
    totalLightOnTimeToday = 0;
    dayStartTime = millis();
  }

  unsigned long currentTime = millis();
  unsigned long dt = currentTime - lastUpdateTime;
  lastUpdateTime = currentTime;

  int rawSoil = analogRead(SOIL_MOISTURE_PIN);
  int soilMoisture = constrain(map(rawSoil, 4095, 1129, 0, 100), 0, 100);

  int lightStatus = digitalRead(LDR_PIN);

  controlDevices(soilMoisture, lightStatus, dt);

  Blynk.virtualWrite(V0, soilMoisture);
  Blynk.virtualWrite(V2, (lightStatus == 0) ? "Natural Light" : "Dark");

  Serial.print("Moisture: "); Serial.print(soilMoisture);
  Serial.print(" | Light: "); Serial.print((lightStatus == 0) ? "Natural Light" : "Dark");
  Serial.print(" | LightTime (s): "); Serial.println(totalLightOnTimeToday / 1000);
}

BLYNK_WRITE(V10) {
  moistureThreshold = param.asFloat();
  Serial.print("Moisture Threshold: "); Serial.println(moistureThreshold);
}

BLYNK_WRITE(V11) {
  dailyLightHours = param.asFloat();
  updateDailyLightDuration();
  Serial.print("Light Hours: "); Serial.println(dailyLightHours);
}

BLYNK_WRITE(V5) {
  if (isManualMode) {
    String s = param.asStr();
    moistureThreshold = s.toFloat();
    Serial.print("Manual Moisture Threshold set to: ");
    Serial.println(moistureThreshold);
    Blynk.virtualWrite(V10, moistureThreshold);
  } else {
    Serial.println("Not in manual mode.");
  }
}

BLYNK_WRITE(V6) {
  if (isManualMode) {
    String s = param.asStr();
    dailyLightHours = s.toFloat();
    updateDailyLightDuration();
    Serial.print("Manual Light Threshold set to: ");
    Serial.println(dailyLightHours);
    Blynk.virtualWrite(V11, dailyLightHours);
  } else {
    Serial.println("Not in manual mode.");
  }
}

struct PlantSettings {
  const char* name;
  float moistureThreshold;
  float lightHours;
};

PlantSettings plants[] = {
  {"cucumber", 65, 9},
  {"eggplant", 65, 9},
  {"lettuce", 75, 11},
  {"spinach", 75, 11},
  {"tomato", 65, 16}
};

BLYNK_WRITE(V4) {
  int plantIndex = param.asInt();
  if (plantIndex >= 0 && plantIndex < 5) {
    isManualMode = false;
    PlantSettings p = plants[plantIndex];
    moistureThreshold = p.moistureThreshold;
    dailyLightHours = p.lightHours;
    updateDailyLightDuration();
    Serial.printf("Plant: %s | Moisture: %.1f | Light: %.1f\n", p.name, p.moistureThreshold, p.lightHours);
    Blynk.virtualWrite(V10, moistureThreshold);
    Blynk.virtualWrite(V11, dailyLightHours);
  } else if (plantIndex == 5) {
    isManualMode = true;
    Serial.println("Manual settings selected. Enter values in V5 (light) and V6 (moisture).");
    Blynk.virtualWrite(V10, moistureThreshold);
    Blynk.virtualWrite(V11, dailyLightHours);
  } else {
    Serial.println("Invalid plant selection");
  }
}
BLYNK_WRITE(V15) {  // Pump Switch
  int state = param.asInt();
  Serial.println("Pump Switch Triggered: " + String(state));
  digitalWrite(PUMP_PIN, state == 1 ? HIGH : LOW);
}

BLYNK_WRITE(V16) {  // LED Switch
  int state = param.asInt();
  Serial.println("LED Switch Triggered: " + String(state));
  digitalWrite(LED_PIN, state == 1 ? HIGH : LOW);
}

BLYNK_WRITE(V21) {  // Reset Wi-Fi & Blynk Credentials
  Serial.println("Resetting Wi-Fi & Blynk credentials...");
  Preferences prefs;
  prefs.begin("config", false);
  prefs.clear();  
  prefs.end();
  WiFi.disconnect(true, true);
  delay(1000);
  ESP.restart();
}

BLYNK_WRITE(V13) {  // Reset Light & Moisture Thresholds
  if (param.asInt() == 1) {
    moistureThreshold = 50;
    dailyLightHours = 12.0;
    updateDailyLightDuration();
    Blynk.virtualWrite(V10, moistureThreshold);
    Blynk.virtualWrite(V11, dailyLightHours);
    Serial.println("Defaults restored.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PUMP_PIN, OUTPUT);   digitalWrite(PUMP_PIN, HIGH);
  pinMode(LED_PIN, OUTPUT);    digitalWrite(LED_PIN, LOW);
  pinMode(LDR_PIN, INPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  EEPROM.begin(512);

  preferences.begin("config", false);
  savedSSID = preferences.getString("ssid", "");
  savedPass = preferences.getString("pass", "");
  savedBlynkAuth = preferences.getString("blynk", "");
  preferences.end();

  wm.addParameter(&custom_blynk);

  if (savedSSID == "" || savedBlynkAuth == "") {
    wm.autoConnect("ESP32_ConfigAP", "configpass");
    savedBlynkAuth = String(custom_blynk.getValue());
    preferences.begin("config", false);
    preferences.putString("ssid", WiFi.SSID());
    preferences.putString("pass", WiFi.psk());
    preferences.putString("blynk", savedBlynkAuth);
    preferences.end();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
  }

  Blynk.config(savedBlynkAuth.c_str());
  unsigned long startTime = millis();
  while (!Blynk.connected() && millis() - startTime < 30000) {
    Blynk.run();
    delay(100);
  }

  if (!Blynk.connected()) {
    preferences.begin("config", false);
    preferences.putString("blynk", "");
    preferences.end();
    ESP.restart();
  }

  lastUpdateTime = millis();
  dayStartTime = millis();
  timer.setInterval(10000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}
