//#define DEBUG

#include <WiFi.h>    
#include <HTTPClient.h>
#include <UrlEncode.h>
#include "credentials.h"

#ifdef DEBUG
boolean debug = true;
#else
boolean debug = false;
#endif

//const int MAIN_PUMP_PIN = 17;
const int SECOND_PUMP_PIN = 16;
const int LED_PIN = 2;

const uint64_t TIME_TO_SLEEP = 60*60;     // Time ESP32 will go to sleep in seconds (60*60 = 1 hour)

RTC_DATA_ATTR int bootCount = 0;  // Number of reboots

void setup() {
  Serial.begin(115200);
  bootCount++;
  logMessage("starting...");

  //pinMode(MAIN_PUMP_PIN, INPUT_PULLUP);
  pinMode(SECOND_PUMP_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH);

  runCheck();

  /* Now we wrap up for Deep Sleep */
  // ESP8266: ESP.deepSleep(TIME_TO_SLEEP * 1000000);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 1000000ULL);  // Set up timer as the wake up source and set sleep duration in microseconds
  logMessage("go to sleep");
  Serial.flush();                                          // Waits for the transmission of outgoing serial data to complete.
  esp_deep_sleep_start();                                  // Start the deep sleep mode
}

void setupWIFI() {
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void runCheck() {
  Serial.println("Running tests");
  if (bootCount >= 24*7) {
    sendAlive();
    bootCount = 0;
  }
  //int mainPump = digitalRead(MAIN_PUMP_PIN);
  int secondPump = digitalRead(SECOND_PUMP_PIN);
  if (!secondPump)
    sendAlert("Drainage-Notpumpe wurde aktiviert!");
  //else if (!mainPump)
  //  sendAlert("Beide Drainagepumpen sind ohne Strom!");
}

void sendAlive() {
  String message = "Der Pumpenmonitor arbeitet noch";
  logMessage("Alive senden: " + message);
  char url[255];
  char encoded[255];
  urlEncode(message).toCharArray(encoded, 255);
  sprintf(url, receivers[0].urlTemplate, receivers[0].phoneNumber, receivers[0].apiKey, encoded);
  Serial.printf("Send message: '%s' to: %s\n", encoded, receivers[0].phoneNumber);
  sendMessage(url);
}

void sendAlert(String message) {
  logMessage("Alarm senden: " + message);
  char url[255];
  char encoded[255];
  urlEncode(message).toCharArray(encoded, 255);
  for (int i=0; i<numberOfReceivers; i++) {
    sprintf(url, receivers[i].urlTemplate, receivers[i].phoneNumber, receivers[i].apiKey, encoded);
    Serial.printf("Send message: '%s' to: %s\n", encoded, receivers[i].phoneNumber);
    sendMessage(url);
  }
}

void sendMessage(char* url){
  if (WiFi.status() != WL_CONNECTED)
    setupWIFI();

  if (debug) {
    Serial.printf("Do not send %s\n", url);
    return;
  }

  HTTPClient http;
  http.begin(url);
  
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Send HTTP POST request
  int httpResponseCode = http.POST(url);
  if (httpResponseCode != 200) {
    Serial.print("Error sending the message to ");
    Serial.println(url);
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}

void loop() {
  // Nothing to do here
}

void logMessage(String s) {
  if (debug) {
    Serial.println(s);
  }
}