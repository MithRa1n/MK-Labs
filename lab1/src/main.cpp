#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define LED1 D6
#define LED2 D4
#define LED3 D7
#define BUTTON_PIN D3

const char* apSSID = "ESP8266_AP";
const char* apPassword = "12345678";

ESP8266WebServer server(80);

volatile bool buttonHeld = false;
volatile unsigned long buttonPressStart = 0;
unsigned long interval = 200;
int ledState = 0;

void IRAM_ATTR handleButtonPress() {
    buttonPressStart = millis();
}

void logStatus() {
    Serial.print("[INFO] Current Interval: ");
    Serial.print(interval);
    Serial.println(" ms");
}

void updateLEDs() {
    static unsigned long previousMillis = 0;

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        ledState = (ledState + 1) % 3;

        digitalWrite(LED1, ledState == 0 ? HIGH : LOW);
        digitalWrite(LED2, ledState == 1 ? HIGH : LOW);
        digitalWrite(LED3, ledState == 2 ? HIGH : LOW);

        Serial.print("[LED] New State: ");
        Serial.println(ledState);
    }
}

void checkButton() {
    if (buttonPressStart > 0 && millis() - buttonPressStart >= 1000) {
        buttonHeld = true;
        buttonPressStart = 0;
    }

    if (buttonHeld) {
        buttonHeld = false;
        interval += 200;
        if (interval > 2000) interval = 500;
        Serial.println("[BUTTON] Hold detected. Speed changed.");
        logStatus();
    }
}

void processWebRequests() {
    server.handleClient();
}

void setupHardware() {
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

    Serial.begin(9600);
    Serial.println("[SYSTEM] Initializing hardware...");
}

void setupWiFiServer() {
    WiFi.softAP(apSSID, apPassword);
    Serial.println("[WiFi] Access Point Started!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", []() { server.send(200, "text/html", "<h2>ESP8266 LED Control</h2>"); });
    server.on("/changeInterval", []() {
        interval += 200;
        if (interval > 2000) interval = 200;
        Serial.println("[WEB] Button clicked. Speed changed.");
        logStatus();
        server.send(204);
    });
    server.begin();
    Serial.println("[WEB] Server started.");
}

void setup() {
    setupHardware();
    setupWiFiServer();
    logStatus();
}

void loop() {
    checkButton();
    updateLEDs();
    processWebRequests();
}