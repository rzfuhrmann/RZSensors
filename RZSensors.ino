/**
 * RZSensors
 * ESP32 sketch to measure and HTTP POST various measurements to a webservice with an ESP32 board
 * Please see the Github repository for details, roadmap - or to suggest improvements: https://github.com/rzfuhrmann/RZSensors
 * 
 * @author    Sebastian Fuhrmann <sebastian.fuhrmann@rz-fuhrmann.de>
 * @copyright (C) 2019-2021 Rechenzentrum Fuhrmann Inh. Sebastian Fuhrmann
 * @version   2021-03-20
 * @license   MIT
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Esp.h"
#include <ArduinoJson.h>
#include <Update.h>

#include "RZ_CONSTANTS.h"

const char* serverName = "http://192.168.1.66:80/tasks/monitoring/influx/esp.php";
const char* FOTA_URL = "http://192.168.1.66:80/rz/RZSensorsUpdater/update.php";
const char* VERSION = "2021-03-20";

uint32_t chipId = 0;
char hostname[20];

const int PIN_LED_RED = 5;
const int PIN_LED_GREEN = 18;
const int PIN_VOLTAGE = 33;

DynamicJsonDocument postData(1024);

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP  10

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

Adafruit_BME280 bme;
BH1750 lightMeter (0x23);

// New Temp Sensors
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallSensors(&oneWire);
DeviceAddress tempDeviceAddress; 
// /new Temps

void checkFOTA(){
  Serial.println("[FOTA] Searching for update...");

  // update server will trigger an update if version is too old or not set
  String url = String(FOTA_URL) + "?hostname=" + String(hostname) + "&cs=" + String(ESP.getSketchMD5()) + "&v=" + String(VERSION);
  Serial.println(url);

  HTTPClient http;
  http.begin(url);
  if (http.GET() == 200){
    Serial.println("[FOTA] Update available.");

    int len = http.getSize();

    Serial.print("[FOTA] Starting update, size: ");
    Serial.print(len); 
    
    Update.begin(len);
    //Update.write(upload.buf, upload.currentSize);
    //Update.writeStream();
    WiFiClient * stream = http.getStreamPtr();

    // read all data from server
    uint8_t buff[128] = { 0 };
    while(http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();

        if(size) {
            // read up to 128 byte
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            // write it to Serial
            //USE_SERIAL.write(buff, c);
            Update.write(buff, c);

            if(len > 0) {
                len -= c;
            }
        }
        delay(1);
    }
    Serial.println("[FOTA] Update end.");
    Update.end(true);
    Serial.println("[FOTA] Restarting...");
    ESP.restart();
  } else {
    Serial.println("[FOTA] No update available (or not found).");
    Serial.println(http.getString());
  }
  http.end();
}

void setup(){
    int countTempSensors = 0; 
    
    Serial.begin(115200);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_VOLTAGE, INPUT);

    // w/o this delay our ESP32s didn't came up randomly after deep sleep
    delay(400);

    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);

    Serial.println();
    Serial.println();

    Serial.println("[Sleep] Boot number: " + String(bootCount));

    
    // generate hostname
    for(int i=0; i<17; i=i+8) {
      chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    sprintf(hostname, "RZ-ESP32-%08X", chipId);

    // connect to wifi
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
      postData["device"] = hostname;
      postData["battery"]["raw"] = analogRead(PIN_VOLTAGE); 
      postData["sketch"]["cs"] = ESP.getSketchMD5(); 
      postData["sketch"]["v"] = VERSION; 
  
      // Loop?
      digitalWrite(PIN_LED_RED, HIGH);
      delay(200);
      digitalWrite(PIN_LED_RED, LOW);
      delay(200);
      
      // wifi still connected?
      if(WiFi.status()== WL_CONNECTED){
        HTTPClient http;
        
        http.begin(serverName);
        http.addHeader("Content-Type", "application/json");
        
        // collecting data and add it to measurement data
        // general debugging data of ESP32
        postData["t"] = millis();
        postData["hw"]["heap_free"] = ESP.getFreeHeap();
        postData["hw"]["heap_size"] = ESP.getHeapSize();
        postData["wifi"]["rssi"] = WiFi.RSSI(); 
        postData["wifi"]["bssid"] = WiFi.BSSIDstr(); 
        postData["wifi"]["ip"] = WiFi.localIP().toString();
        postData["wifi"]["gateway"] = WiFi.gatewayIP().toString(); 
        postData["sleep"]["bc"] = bootCount;

        // DALLAS TEMPERATURE
        dallSensors.begin();
        int numberOfDevices = dallSensors.getDeviceCount();

        if (numberOfDevices){
          Serial.print("[DS18B20] Found ");
          Serial.print(numberOfDevices);
          Serial.println(" DS18B20 sensors, requesting temperatures...");
          
          dallSensors.requestTemperatures();
          for(int i=0;i<numberOfDevices; i++){
            if(dallSensors.getAddress(tempDeviceAddress, i)){
              postData["sensors"]["temperatureMulti"][countTempSensors]["id"] = addr2str(tempDeviceAddress);
              postData["sensors"]["temperatureMulti"][countTempSensors++]["temp"] = dallSensors.getTempC(tempDeviceAddress);
            } else {
              // Ghost device, maybe wiring wrong?
            }
          }
        } else {
          Serial.println("[DS18B20] No DS18B20 sensors found.");
        }
  
        // BME280 - TEMP, PRESSURE, HUMIDITY
        bool weHaveBMEdata = false; 
        Serial.println("[BME280] Init temp/pressure/humidity...");
        unsigned status = bme.begin(0x76);
        if (status){
          Serial.println("[BME280] Got data!");
          postData["sensors"]["temperature"] = bme.readTemperature();
          postData["sensors"]["pressure"] = bme.readPressure() / 100.0F;
          postData["sensors"]["humidity"] = bme.readHumidity();
          weHaveBMEdata = true; 

          // TEMP: new multiTemp format
          postData["sensors"]["temperatureMulti"][countTempSensors]["id"] = "bme280";
          postData["sensors"]["temperatureMulti"][countTempSensors++]["temp"] = postData["sensors"]["temperature"];
        } else {
          Serial.println("[BME280] Not available.");
        }

        // BH1750 - LIGHT
        Serial.println("[BH1750] Init light sensor...");
        if (lightMeter.begin()){
          postData["sensors"]["light"] = lightMeter.readLightLevel();
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
            // if a BME (and its measurements) is available at this device, we can use it so set env data
            ccs.setEnvironmentalData(bme.readHumidity(), bme.readTemperature());
          }
          while(!ccs.available());
          int i = 0; 
          delay(500);
          // only one measurement would not be accurate, therefore doing 10 and using the worst case (highest CO2 concentration)
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
            postData["sensors"]["co2"] = CO2max;
            postData["sensors"]["tvoc"] = TVOCmax;
          }
        } else {
          Serial.println("[CSS811] Not available.");
        }

        // build JSON
        char buffer[1024];
        serializeJson(postData, buffer);
        Serial.print("[HTTP] Payload: "); 
        Serial.println(buffer);
        int httpResponseCode = http.POST(buffer);
        
        Serial.print("[HTTP] Response code: ");
        Serial.println(httpResponseCode);

        // blink 2x on success
        digitalWrite(PIN_LED_RED, HIGH);
        delay(150);
        digitalWrite(PIN_LED_RED, LOW);
        delay(200);
        digitalWrite(PIN_LED_RED, HIGH);
        delay(150);
        digitalWrite(PIN_LED_RED, LOW);
        delay(200);

        // ... and 3x on error
        if (httpResponseCode != 200){
          digitalWrite(PIN_LED_RED, HIGH);
          delay(150);
          digitalWrite(PIN_LED_RED, LOW);
          delay(200);
        }
          
        // Free resources
        http.end();

        // check for update in 1/5 cases
        if (random(10) % 4 == 0) checkFOTA(); 
      } else {
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

char *addr2str(DeviceAddress deviceAddress){
    static char res[18];
    static char *hex = "0123456789ABCDEF";
    uint8_t i, j;

    for (i=0, j=0; i<8; i++){
         res[j++] = hex[deviceAddress[i] / 16];
         res[j++] = hex[deviceAddress[i] & 15];
    }
    res[j] = '\0';

    return res;
}

void loop(){}
