#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "CommunicationService.h"

#define BUTTON_PIN D6
#define LED1 D3
#define LED2 D5
#define LED3 D7

const char* apSSID = "ESP8266-AP";
const char* apPassword = "123456789";

ESP8266WebServer server(80);
SoftwareSerial mySerial(D7, D6, false);
CommunicationService communicationService(mySerial, 115200);

const uint32_t STOP_DURATION = 15000;
uint32_t stopTime = 0;
bool isStopped = false;
volatile bool buttonPressed = false;
volatile uint32_t lastInterruptTime = 0;

void IRAM_ATTR handleButton() {
    uint32_t interruptTime = millis();
    if (interruptTime - lastInterruptTime > 200) {
        buttonPressed = true;
        lastInterruptTime = interruptTime;

        if (!isStopped) {
            Serial.println("Button pressed! Sending STOP command...");
            communicationService.send(ToogleCommand::STOP);
        }
    }
}

void setupPins() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
}

void setupWiFi() {
    WiFi.softAP(apSSID, apPassword);
    Serial.print("Access Point Started: ");
    Serial.println(apSSID);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void setupServer() {
  server.on("/", []() {
    server.send(200, "text/html",
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<title>LED Control</title>"
      "<style>"
      "body {text-align:center; font-family:Arial;}"
      "button {font-size:20px; padding:10px;}"
      "</style>"
      "</head>"
      "<body>"
      "<h1>LED Control via Web</h1>"
      "<button onclick=\"stopLEDs()\">Stop LEDs for 15 seconds</button>"  // Виправлено
      "<button onclick=\"simulateButtonRemote()\">Press Remote Button</button>"
      "<script>"
      "function stopLEDs() {"
      "  fetch('/stopLEDs').then(response => response.text()).then(data => {"
      "    console.log(data);"
      "    alert(data);"
      "  });"
      "}"
      "function simulateButtonRemote() {"
      "  fetch('/simulateRemote').then(response => response.text()).then(data => {"
      "    console.log(data);"
      "    alert(data);"
      "  });"
      "}"
      "</script>"
      "</body>"
      "</html>"
    );
  });

    server.on("/stopLEDs", []() {
        buttonPressed = true;
        communicationService.send(ToogleCommand::STOP);
        server.send(200, "text/plain", "LEDs will stop for 15 seconds.");
    });

    server.begin();
    Serial.println("Server started.");

    server.on("/simulateRemote", []() {
        buttonPressed = true;
        communicationService.send(ToogleCommand::ON);
        server.send(200, "text/plain", "Simulated remote button press.");
    });
    
}

void handleButtonPress() {
    if (buttonPressed && !isStopped) {
        isStopped = true;
        stopTime = millis();
        digitalWrite(LED1, LOW);
        digitalWrite(LED2, LOW);
        digitalWrite(LED3, LOW);
        Serial.println("Button pressed! Stopping for 15 seconds...");
        buttonPressed = false;
    }
}

void handleTimer() {
    if (isStopped && (millis() - stopTime >= STOP_DURATION)) {
        isStopped = false;
        Serial.println("Timer finished! Resuming...");
        communicationService.send(ToogleCommand::ON);
    }
}

void updateLEDs() {
    static uint8_t ledState = 0;
    static uint32_t lastSwitchTime = 0;
    const uint32_t switchInterval = 500;

    if (!isStopped && millis() - lastSwitchTime >= switchInterval) {
        lastSwitchTime = millis();
        ledState = (ledState + 1) % 3;
        digitalWrite(LED1, ledState == 0 ? HIGH : LOW);
        digitalWrite(LED2, ledState == 1 ? HIGH : LOW);
        digitalWrite(LED3, ledState == 2 ? HIGH : LOW);
        Serial.print("New State: ");
        Serial.println(ledState);
    }
}

void setup() {
    Serial.begin(9600);
    setupPins();
    setupWiFi();
    setupServer();
    communicationService.init();

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);
}

void loop() {
    server.handleClient();
    handleButtonPress();
    handleTimer();
    updateLEDs();

    communicationService.onReceive([](ToogleCommand command) {
        Serial.print("Received command: ");
        Serial.println((int)command);

        if (command == ToogleCommand::STOP) {
            isStopped = true;
            stopTime = millis();
            digitalWrite(LED1, LOW);
            digitalWrite(LED2, LOW);
            digitalWrite(LED3, LOW);
            Serial.println("Received STOP command! Stopping LEDs...");
        }
        if (command == ToogleCommand::ON) {
            isStopped = false;
            Serial.println("Received ON command! Resuming LEDs...");
        }
    });
}
