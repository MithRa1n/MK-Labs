#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "CommunicationService.h"
#include <WebSocketsServer.h> 

#define BUTTON_PIN D3
#define LED1 D5
#define LED2 D2
#define LED3 D1

const char* apSSID = "ESP8266-AP";
const char* apPassword = "123456789";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
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

void sendLEDState() {
    String message = String(digitalRead(LED1)) + "," +
                     String(digitalRead(LED2)) + "," +
                     String(digitalRead(LED3));
    webSocket.broadcastTXT(message);
}

void setupWiFi() {
    WiFi.softAP(apSSID, apPassword);
    Serial.println("Access Point Started: " + String(apSSID));
    Serial.println("IP Address: " + WiFi.softAPIP().toString());
}

void setupWebSocket() {
    webSocket.begin();
    sendLEDState();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        if (type == WStype_TEXT) {
            String command = String((char*)payload);
            if (command == "TOGGLE") {
                buttonPressed = true;
            }
        }
    });
    Serial.println("WebSocket server started.");
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
          ".led {width: 50px; height: 50px; display: inline-block; border-radius: 50%; margin: 10px;}"
          "</style>"
          "</head>"
          "<body>"
          "<h1>LED Control via Web</h1>"
          "<button onclick=\"stopLEDs()\">Stop LEDs for 15 seconds</button>"
          "<button onclick=\"simulateButtonRemote()\">Press Remote Button</button>"
          "<div>"
          "<div id='led1' class='led' style='background-color:gray;'></div>"
          "<div id='led2' class='led' style='background-color:gray;'></div>"
          "<div id='led3' class='led' style='background-color:gray;'></div>"
          "</div>"
          "<script>"
          "var ws = new WebSocket('ws://' + location.hostname + ':81/');"
          "ws.onmessage = function(event) {"
          "  var states = event.data.split(',');"
          "  document.getElementById('led1').style.backgroundColor = states[0] == '1' ? 'green' : 'gray';"
          "  document.getElementById('led2').style.backgroundColor = states[1] == '1' ? 'red' : 'gray';"
          "  document.getElementById('led3').style.backgroundColor = states[2] == '1' ? 'red' : 'gray';"
          "};"
          "function stopLEDs() { fetch('/stopLEDs'); }"
          "function simulateButtonRemote() { fetch('/simulateRemote'); }"
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

    server.on("/simulateRemote", []() {
        communicationService.send(ToogleCommand::ON);
        server.send(200, "text/plain", "Simulated remote button press.");
    });

    server.begin();
    Serial.println("Server started.");
}
void handleButtonPress() {
    if (buttonPressed && !isStopped) {
        isStopped = true;
        stopTime = millis();
        digitalWrite(LED1, LOW);
        digitalWrite(LED2, LOW);
        digitalWrite(LED3, LOW);
        sendLEDState();
        Serial.println("Button pressed! Stopping for 15 seconds...");
        buttonPressed = false;
    }
}

void handleTimer() {
    if (isStopped && (millis() - stopTime >= STOP_DURATION)) {
        isStopped = false;
        Serial.println("Timer finished! Resuming...");
        communicationService.send(ToogleCommand::ON);
        sendLEDState();
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
        sendLEDState();
        Serial.print("New State: ");
        Serial.println(ledState);
    }
}

void setup() {
    Serial.begin(9600);
    setupPins();
    setupWiFi();
    setupWebSocket();
    setupServer();
    communicationService.init();

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);
}

void loop() {
    server.handleClient();
    webSocket.loop();
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