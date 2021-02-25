#include <WiFi.h>
#include <HTTPClient.h>
#include "Esp.h"
#include <ArduinoJson.h>

#include "RZ_CONSTANTS.h"

const char* serverName = "http://192.168.1.66:80/tasks/monitoring/influx/esp.php";

uint32_t chipId = 0;
char hostname[20];

const int PIN_LED_RED = 5;
const int PIN_LED_GREEN = 18;
const int PIN_VOLTAGE = 33;


DynamicJsonDocument testDocument(1024);

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10       /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

#include "Adafruit_CCS811.h"
Adafruit_CCS811 ccs;

#include <Adafruit_BME280.h>
#include <BH1750.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
BH1750 lightMeter (0x23);

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_LED_RED, OUTPUT);      // set the LED pin mode
    pinMode(PIN_LED_GREEN, OUTPUT);      // set the LED pin mode
    pinMode(PIN_VOLTAGE, INPUT);

    delay(400);

    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);

    Serial.println();
    Serial.println();

    Serial.println("[Sleep] Boot number: " + String(bootCount));

    // We start by connecting to a WiFi network
    // generate hostname
    for(int i=0; i<17; i=i+8) {
      chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
  
    sprintf(hostname, "RZ-ESP32-%08X", chipId);
    Serial.print("[WiFi] Setting hostname to ");
    Serial.println(hostname); 
    WiFi.setHostname(hostname);
    Serial.print("[WiFi] Connecting to ");
    Serial.println(RZ_WIFI_SMARTHOME_SSID);
    WiFi.begin(RZ_WIFI_SMARTHOME_SSID, RZ_WIFI_SMARTHOME_PASS);
    WiFi.setHostname(hostname);

    int wifi_tries = 0; 
    while (WiFi.status() != WL_CONNECTED && wifi_tries <= 10) {
        delay(500);
        Serial.print(".");
        wifi_tries++;
    }
    Serial.println("");

    if (WiFi.status() != WL_CONNECTED){
      Serial.println("[WiFi] No chance to connect after a couple of retries. Skipping measurement.");
    } else {
      Serial.print("[WiFi] Connected. IP address: ");
      Serial.println(WiFi.localIP());
  
      // JSON try
      testDocument["device"] = hostname;
      testDocument["battery"]["raw"] = analogRead(PIN_VOLTAGE); 
      testDocument["sketch"]["cs"] = ESP.getSketchMD5(); 
  
      // Loop?
      digitalWrite(PIN_LED_RED, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(200);                       // wait for a second
      digitalWrite(PIN_LED_RED, LOW);    // turn the LED off by making the voltage LOW
      delay(200);                       // wait for a second
      
      //Check WiFi connection status
      if(WiFi.status()== WL_CONNECTED){
        HTTPClient http;
        
        // Your Domain name with URL path or IP address with path
        http.begin(serverName);
  
        // Specify content-type header
        http.addHeader("Content-Type", "application/json");
        
        // Send HTTP POST request
        testDocument["t"] = millis();
        testDocument["hw"]["heap_free"] = ESP.getFreeHeap();
        testDocument["hw"]["heap_size"] = ESP.getHeapSize();
        testDocument["wifi"]["rssi"] = WiFi.RSSI(); 
        testDocument["wifi"]["bssid"] = WiFi.BSSIDstr(); 
        testDocument["wifi"]["ip"] = WiFi.localIP().toString();
        testDocument["wifi"]["gateway"] = WiFi.gatewayIP().toString(); 
        testDocument["sleep"]["bc"] = bootCount;
  
        // BME280 - TEMP, PRESSURE, HUMIDITY
        bool weHaveBMEdata = false; 
        Serial.println("[BME280] Init temp/pressure/humidity...");
        unsigned status = bme.begin(0x76);
        if (status){
          Serial.println("[BME280] Got data!");
          testDocument["sensors"]["temperature"] = bme.readTemperature();
          testDocument["sensors"]["pressure"] = bme.readPressure() / 100.0F;
          testDocument["sensors"]["humidity"] = bme.readHumidity();
          weHaveBMEdata = true; 
        } else {
          Serial.println("[BME280] Not available.");
        }

        // BH1750 - LIGHT
        Serial.println("[BH1750] Init light sensor...");
        if (lightMeter.begin()){
          testDocument["sensors"]["light"] = lightMeter.readLightLevel();
          Serial.println("[BH1750] Available and got data.");
        } else {
          Serial.println("[BH1750] Not available.");
        }
  
        // CSS811 - CO2/TVOC
        Serial.println("[CSS811] Trying to start CO2 sensor...");
        bool weHaveCO2data = false;
        int CO2max = 0; int TVOCmax = 0; 
        if(ccs.begin()){
          if (weHaveBMEdata){
            ccs.setEnvironmentalData(bme.readHumidity(), bme.readTemperature());
          }
          while(!ccs.available());
          int i = 0; 
          delay(500);
          while (i < 10){
            delay(200);
            if(!ccs.readData()){
              weHaveCO2data = true; 
              if (ccs.geteCO2() > CO2max) CO2max = ccs.geteCO2();
              if (ccs.getTVOC() > TVOCmax) TVOCmax = ccs.getTVOC();
            } else {
              Serial.println("[CSS811] ERROR!");
            }
            i++;
          }
          if (weHaveCO2data){
            testDocument["sensors"]["co2"] = CO2max;
            testDocument["sensors"]["tvoc"] = TVOCmax;
          }
        } else {
          Serial.println("[CSS811] Not available.");
        }
  
        char buffer[1024];
    
        serializeJson(testDocument, buffer);
        Serial.print("[HTTP] Payload: "); 
        Serial.println(buffer);
        int httpResponseCode = http.POST(buffer);
        
        Serial.print("[HTTP] Response code: ");
        Serial.println(httpResponseCode);
  
        digitalWrite(PIN_LED_RED, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(150);                       // wait for a second
        digitalWrite(PIN_LED_RED, LOW);    // turn the LED off by making the voltage LOW
        delay(200);                       // wait for a second
        digitalWrite(PIN_LED_RED, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(150);                       // wait for a second
        digitalWrite(PIN_LED_RED, LOW);    // turn the LED off by making the voltage LOW
        delay(200);   
  
        if (httpResponseCode != 200){
          digitalWrite(PIN_LED_RED, HIGH);   // turn the LED on (HIGH is the voltage level)
          delay(150);                       // wait for a second
          digitalWrite(PIN_LED_RED, LOW);    // turn the LED off by making the voltage LOW
          delay(200);   
        }
          
        // Free resources
        http.end();
      }
      else {
        Serial.println("WiFi Disconnected");
      }
    }

    // Deepsleep config
    Serial.println("[Sleep] Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) + " Seconds");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

    Serial.println("[Sleep] Going to sleep now - bye!");
    digitalWrite(PIN_LED_GREEN, LOW);
    bootCount++;
    delay(400);
    Serial.flush(); 
    esp_deep_sleep_start();
}

void loop(){}
