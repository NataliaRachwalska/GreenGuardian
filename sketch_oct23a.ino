#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <time.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "ISTS-2.4GHz-4F40E4"
#define WIFI_PASSWORD "3AGKxmW4Mr7v"

#define API_KEY "AIzaSyCIypfnQHq_mm_1Oap9HWDlGdnJxGL8V58"

#define DATABASE_URL "greenguardiandb-default-rtdb.europe-west1.firebasedatabase.app"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int Humidity_1 = 0;
bool signupOK = false;
int LightIntensity_1 = 0; 
const float GAMMA = 0.7;  
const float RL10 = 50;
const int NUM_SENSORS = 2; 

#define SOIL_MOISTURE_PIN_NO1 36 
#define LIGHT_INTENSITY_PIN_NO1 25

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 100

const int SOIL_MOISTURE_PINS[] = {SOIL_MOISTURE_PIN_NO1};
const int LIGHT_INTENSITY_PINS[] = {LIGHT_INTENSITY_PIN_NO1};

void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Pobieranie czasu NTP");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
}

String getFormattedTime() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M-%S", &timeinfo);
  return String(buffer);
}

void goToDeepSleep() {
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start(); 
}

void setup(){
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  setupTime();
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")){
    signupOK = true;
  } else {
    Serial.printf("Błąd rejestracji: %s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true); 
}

float calculateLux(int lightIntensity) {
  float voltage = lightIntensity / 4095.0 * 5.0;
  float resistance = 2000.0 * voltage / (1.0 - voltage / 5.0);
  return pow(RL10 * 1e3 * pow(10.0, GAMMA) / resistance, (1.0 / GAMMA));
}

void loop() {
  if (Firebase.ready() && signupOK) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      int humidity = analogRead(SOIL_MOISTURE_PINS[i]);
      int lightIntensity = analogRead(LIGHT_INTENSITY_PINS[i]);
      float lux = calculateLux(lightIntensity);
      String currentTime = getFormattedTime();

      String humidityPath = "/Humiditysensor" + String(i + 1) + "/" + currentTime;
      String lightPath = "/LightSensor" + String(i + 1) + "/" + currentTime;

      if (Firebase.RTDB.setInt(&fbdo, humidityPath + "/Humidity", humidity)) {
        Serial.println("Dane wilgotności z czujnika " + String(i + 1) + " wysłane do Firebase.");
      } else {
        Serial.println("Błąd wysyłania danych wilgotności z czujnika " + String(i + 1) + ".");
      }

      if (Firebase.RTDB.setFloat(&fbdo, lightPath + "/lux", lux)) {
        Serial.println("Dane natężenia światła z czujnika " + String(i + 1) + " wysłane do Firebase.");
      } else {
        Serial.println("Błąd wysyłania danych natężenia światła z czujnika " + String(i + 1) + ".");
      }
    }
    goToDeepSleep();
  }
}
