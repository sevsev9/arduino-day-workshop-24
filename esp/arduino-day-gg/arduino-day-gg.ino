#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <WebSocketsClient.h>



// WiFi credentials.
const char* ssid = "GRANDGARAGE-PUBLIC";
const char* password = "wifi4members";

// WebSocket server details.
const char* webSocketServer = "10.6.0.112";
const int port = 3000;

// Sensor Settings
#define BME280_ADDRESS 0x77
#define SEA_LEVEL 1013.25

// Delays
#define DATA_DELAY 500    //ms
#define RENDER_DELAY 100  //ms

// Last updates
long last_data_update = 0;
long last_display_update = 0;


unsigned long previousUpdate = 0;
const long interval = 1000;  // ms
bool ws_connected = false;


// Screen Settings
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1
#define SCREEN_HEIGHT 64  // px
#define SCREEN_WIDTH 128  // px
#define TEXT_SIZE 1
#define LINE_WIDTH 21  // chars

// Render Task Handle
TaskHandle_t RenderTask;

// last values
float last_tmp = 0;
float last_hmd = 0;
float last_pre = 0;

// char pointers (Strings) that will be displayed on the display
char* tmp_str;
char* pressure_str;
char* hmd_str;

// Starting pixel of every line on the display @ Text Size 1
const int16_t LINES[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

// draw and read function definitions
void read_data(void* param);
void draw_info(void* param);

// Sensor References
Adafruit_BME280 bme;
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


WebSocketsClient webSocket;

void setup() {
  // Initialize the Serial Communication with 115200 bps
  Serial.begin(115200);
  Serial.println("Starting up...");


  // Initialize Display
  if (!display.begin(SCREEN_ADDRESS, true)) {
    Serial.println(F("SSH1106 allocation failed"));
    while (1) delay(100);
  }

  // Display Setup
  display.clearDisplay();
  display.setTextSize(SH110X_WHITE);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, LINES[0]);
  display.print(F("Initialized Display!"));
  display.display();

  // BMP280 Sensor Setup
  display.setCursor(0, LINES[1]);
  display.print(F("Initializing BME280..."));

  unsigned status = bme.begin();
  if (!status) {
    display.setCursor(0, LINES[1]);
    Serial.println("BME280 init failed. Check wiring.");
    display.print(F("BME280 init failed.   "));
    display.display();
    while (1) delay(100);
  }
  display.display();

  // Connect to WiFi.
  WiFi.begin(ssid, password);
  display.setCursor(0, LINES[2]);
  display.print(F("Connecting to Wifi.."));
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  display.setCursor(0, LINES[2]);
  display.print(F("Connected to WiFi.   "));
  display.display();

  // Initialize WebSocket.
  webSocket.begin(webSocketServer, port, "/");
  webSocket.onEvent(webSocketEvent);

  // initialize the strings to be displayed
  tmp_str = (char*)calloc(24, sizeof(char));
  pressure_str = (char*)calloc(24, sizeof(char));
  hmd_str = (char*)calloc(24, sizeof(char));

  // initialize the render task
  xTaskCreatePinnedToCore(
    draw_info,    /* Function to implement the task */
    "RenderTask", /* Name of the task */
    10000,        /* Stack size in words */
    NULL,         /* Task input parameter */
    0,            /* Priority of the task */
    &RenderTask,  /* Task handle. */
    0);           /* Core where the task should run */
}

void loop() {
  read_data((void*)0);
  webSocket.loop();

  unsigned long currentMillis = millis();
  if (ws_connected && currentMillis - previousUpdate >= interval) {
    previousUpdate = currentMillis;  // Save the last time you sent the message

    // Only proceed with sending the message if WiFi is connected.
    if (WiFi.status() == WL_CONNECTED) {
      // Construct JSON string with snprintf.
      char jsonString[128];  // Ensure this buffer is large enough for your JSON string.
      snprintf(jsonString, sizeof(jsonString), "{\"location\": \"beamer\", \"temperature\": %.2f, \"humidity\": %.2f}", last_tmp, last_hmd);

      webSocket.sendTXT(jsonString);
      Serial.println("Sending message: " + String(jsonString));
    } else {
      Serial.println("WiFi not connected");
    }
  }
}

void draw_info(void* param) {
  while (1) {
    // re-draw information on the i2c display
    if (millis() > last_display_update + RENDER_DELAY) {
      display.clearDisplay();
      display.setCursor(0, LINES[0]);
      display.print(tmp_str);
      display.setCursor(0, LINES[1]);
      display.print(pressure_str);
      display.setCursor(0, LINES[2]);
      display.print(hmd_str);
      display.setCursor(0, LINES[3]);
      display.print("WS Server: ");

      if (ws_connected) {
        display.print(webSocketServer);
      } else {
        display.print(" - ");
      }
      display.display();

      last_display_update = millis();
    }

    // to conserve energy (~98.5% power usage reduction)
    delay(10);
  }
}

void read_data(void* param) {
  if (millis() - last_data_update > DATA_DELAY) {
    float temp = bme.readTemperature();
    float pre = bme.readPressure();
    float hmd = bme.readHumidity();
    sprintf(tmp_str, "Temperature: %.2f%cC", temp, 167);
    sprintf(pressure_str, "Pressure: %.2f kPa", pre / 1000);
    sprintf(hmd_str, "Humidity: %.2f%c", hmd, 37);

    last_tmp = temp;
    last_pre = pre;
    last_hmd = hmd;

    last_data_update = millis();
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected!\n");
      ws_connected = false;
      break;
    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to server\n");
      ws_connected = true;
      break;
    case WStype_TEXT:
      Serial.printf("[WebSocket] Get text: %s\n", payload);
      break;
  }
}