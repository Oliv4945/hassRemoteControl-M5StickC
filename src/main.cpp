#include <Arduino.h>
#include <M5StickC.h>
#include <WiFi.h>
#include <driver/adc.h>
#include <esp_http_client.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

#include "config.h"

// Constants
const uint16_t color_red = 0x1F << 11;
const uint16_t color_green = 0x3F << 5;
const uint16_t color_blue = 0x1F;

// Declarations
void manageCoulombMeter();
void myDeepSleep(gpio_num_t PIN);
void triggerAutomation(String automation_name);

void setup() {
  M5.begin(true, true, true);  // Lcd, power, serial
  M5.Lcd.setTextSize(1);
  M5.Axp.ScreenBreath(9);
  pinMode(M5_BUTTON_HOME, INPUT_PULLUP);

  Serial.print("\n------------------------\nGetVbatData: ");
  Serial.println(M5.Axp.GetVbatData() * 1.1 / 1000);
  Serial.print("GetCoulombchargeData: ");
  Serial.println(M5.Axp.GetCoulombchargeData());
  Serial.print("GetCoulombData: ");
  Serial.println(M5.Axp.GetCoulombData());
  Serial.print("GetCoulombData2: ");
  Serial.println(M5.Axp.GetCoulombData2());
  Serial.print("GetCoulombdischargeData: ");
  Serial.println(M5.Axp.GetCoulombdischargeData());
  Serial.print("GetIchargeData: ");
  Serial.println(M5.Axp.GetIchargeData() / 2);
  Serial.print("GetVusbinData: ");
  Serial.println(M5.Axp.GetVusbinData() * 1.7 / 1000);
  Serial.print("GetVinData: ");
  Serial.println(M5.Axp.GetVinData() * 1.7 / 1000);
  Serial.print("GetVapsData: ");
  Serial.println(M5.Axp.GetVapsData() * 1.4 / 1000);
}

void loop() {
  while (digitalRead(BUTTON_A_PIN) == LOW)
    ;
  triggerAutomation("hyppoclock_on");
  manageCoulombMeter();
  M5.Lcd.fillScreen(BLACK);
  myDeepSleep((gpio_num_t)BUTTON_A_PIN);
}

void myDeepSleep(gpio_num_t PIN) {
  // Rewrite M5.Axp.DeepSleep() in order to enable ext0 wakep up.
  esp_sleep_enable_ext0_wakeup(PIN, LOW);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
  esp_wifi_stop();
  adc_power_off();
  M5.Axp.SetSleep();
  esp_deep_sleep_start();
}

void triggerAutomation(String automation_name) {
  unsigned long timeout;
  WiFiClient client;

  String content = "{\"entity_id\": \"automation." + automation_name + "\"}";

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Connexion");

  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  timeout = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - timeout > 20000) {
      M5.Lcd.setCursor(0, 10);
      M5.Lcd.setTextColor(color_red);
      M5.Lcd.println("Connexion WiFi impossible");
      delay(3000);
      return;
    }
    delay(100);
  }

  if (!client.connect(http_host, http_port)) {
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.setTextColor(color_red);
    M5.Lcd.println("Serveur non joignable");
    delay(3000);
    return;
  }

  client.print(String("POST ") + http_url + " HTTP/1.1\r\n" + "Host: " +
               http_host + "\r\n" + "Content-Type: application/json\r\n" +
               "Content-Length: " + content.length() + "\r\n" +
               "Authorization: " + http_token + "\r\n" +
               "Connection: close\r\n\r\n" + content);

  timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 2000) {
      client.stop();
      M5.Lcd.setCursor(0, 10);
      M5.Lcd.setTextColor(color_red);
      M5.Lcd.println("Pas de reponse serveur");
      delay(3000);
      return;
    }
  }

  content = client.readStringUntil('\r');
  if (content == "HTTP/1.1 200 OK") {
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.printf("%.3f mAh", M5.Axp.GetCoulombData2());
    M5.Lcd.setCursor(0, 50);
    M5.Lcd.printf("%.3f V", (M5.Axp.GetVbatData() * 1.1 / 1000));
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.setTextColor(color_green);
    M5.Lcd.println("Potame est allume !");
    delay(800);
  } else {
    M5.Lcd.setTextColor(color_red);
    M5.Lcd.println("Mauvaise reponse serveur");
    delay(3000);
  }
}

void manageCoulombMeter() {
  // Reset coulomb meter when fully charged
  if (((M5.Axp.GetIchargeData() / 2) == 0) &&
      ((M5.Axp.GetVusbinData() * 1.7 / 1000) > 4.5) &&
      ((M5.Axp.GetVbatData() * 1.1 / 1000) > 4.1)) {
    M5.Axp.ClearCoulombcounter();
    Serial.println("Reset coulomb meter");
  }
}