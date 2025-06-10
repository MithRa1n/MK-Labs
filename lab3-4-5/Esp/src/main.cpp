#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TCS34725.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>


#define HTTP_OK 200
#define HTTP_NO_CONTENT 204
#define HTTP_NOT_FOUND 404
#define HTTP_INTERNAL_ERROR 500


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define I2C_SDA D6
#define I2C_SCL D5
#define OLED_ADDRESS 0x3C
#define TCS34725_ADDRESS 0x29

#define BAUND_RATE 115200

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_600MS, TCS34725_GAIN_1X);
ESP8266WebServer server(80);

uint8_t lastSave = 0;
const uint8_t interval = 5000;

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* t = gmtime(&now);
  char buf[25];
  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
          t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
  sendCorsHeaders();
  server.send(HTTP_OK, "text/plain", "Smart Color Logger is running.");
}

void handleAllMeasurements() {
  if (!LittleFS.exists("/colors.csv")) {
    sendCorsHeaders();
    server.send(HTTP_NOT_FOUND, "application/json", "{\"error\":\"File not found\"}");
    return;
  }

  File file = LittleFS.open("/colors.csv", "r");
  if (!file) {
    sendCorsHeaders();
    server.send(HTTP_INTERNAL_ERROR, "application/json", "{\"error\":\"Read error\"}");
    return;
  }

  String json = "[";
  bool first = true;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("ID")) continue;

    uint8_t idIndex = line.indexOf(',');
    uint8_t rIndex = line.indexOf(',', idIndex + 1);
    uint8_t gIndex = line.indexOf(',', rIndex + 1);
    uint8_t bIndex = line.indexOf(',', gIndex + 1);

    String id = line.substring(0, idIndex);
    String r = line.substring(idIndex + 1, rIndex);
    String g = line.substring(rIndex + 1, gIndex);
    String b = line.substring(gIndex + 1, bIndex);
    String createdAt = line.substring(bIndex + 1);

    if (!first) json += ",";
    first = false;

    json += "{\"id\":" + id + ",\"red\":" + r + ",\"green\":" + g + ",\"blue\":" + b + ",\"createdAt\":\"" + createdAt + "\"}";
  }

  json += "]";
  file.close();

  sendCorsHeaders();
  server.send(HTTP_OK, "application/json", json);
}

void handleOptionsRequest() {
  sendCorsHeaders();
  server.send(HTTP_NO_CONTENT);
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptionsRequest();
  } else {
    sendCorsHeaders();
    server.send(HTTP_NOT_FOUND, "application/json", "{\"error\":\"Endpoint not found\"}");
  }
}

void setup() 
{
  Serial.begin(BAUND_RATE);
  Wire.begin(I2C_SDA, I2C_SCL);

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (!tcs.begin()) {
    display.println("TCS34725 not found");
    display.display();
    while (true);
  }

  if (!LittleFS.begin()) {
    Serial.println("FS error");
    display.println("FS error");
    display.display();
    while (true);
  }

  if (!LittleFS.exists("/colors.csv")) {
    File file = LittleFS.open("/colors.csv", "w");
    file.println("ID,Red,Green,Blue,CreatedAt");
    file.close();
  }
  LittleFS.remove("/colors.csv");
  File file = LittleFS.open("/colors.csv", "w");
  file.println("ID,Red,Green,Blue,CreatedAt");
  file.close();

  WiFi.softAP("zalupka12", "postav10");
  IPAddress myIP = WiFi.softAPIP();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("AP: ColorLogger");
  display.println(myIP.toString());
  display.display();

  Serial.println("WiFi AP started. IP:");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/api/measurements", handleAllMeasurements);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  uint32_t currentMillis = millis();
  if (currentMillis - lastSave >= interval) {
    lastSave = currentMillis;

    static uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("R:%d\nG:%d\nB:%d", r, g, b);
    display.display();

    int nextId = 1;
    File file = LittleFS.open("/colors.csv", "r");
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.startsWith("ID")) continue;
      nextId++;
    }
    file.close();

    String timestamp = getTimestamp();
    String line = String(nextId) + "," + r + "," + g + "," + b + "," + timestamp;

    file = LittleFS.open("/colors.csv", "a");
    if (file) {
      file.println(line);
      file.close();
      Serial.println("Saved: " + line);
    }
  }
}