/*
  v5 MATERIAL MOOD BOARD -> Vespera  |  CASA0014, Arduino MKR1010
  ---------------------------------------------------------------
  Six material samples float on hidden push buttons. Pressing a
  material changes Vespera's light to suit it.

  Changes from v4:
    - Pond scene replaced by a material mood board (6 interactive tiles)
    - Idle state: Vespera shows the board's palette, 2 columns per material
    - Pressing a tile shows that material's light scene
    - After 60 s with no press, Vespera returns to the idle palette
    - Material colours stored once in palette[6][3]

       Button 1 (pin 1) : Fluted wood   -> brown / dark groove columns
    Button 2 (pin 2) : Grey oak      -> light grey / slate rows (high contrast)
    Button 3 (pin 3) : Blue glass    -> cobalt with aqua patches
    Button 4 (pin 4) : Oak block     -> yellow stripes, moving sideways
    Button 5 (pin 5) : Silver mosaic -> silver / cream / black mosaic, shifting + glint
    Button 6 (pin 0) : Fabric        -> terracotta / linen columns
    No press for 60 s                -> idle palette, 2 columns per material

  Vespera: topic student/CASA0014/luminaire/<lightId>
           12 columns x 6 rows = 72 LEDs, numbered down each column
  Wiring:  each button between its pin and GND (diagonal legs), INPUT_PULLUP
           pin 6 avoided - wired to the MKR1010's built-in LED
  Secrets: arduino_secrets.h is not in the repo - see README
  AI use:  see README
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"

const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1884;
String lightId = "15";
String mqtt_topic = "student/CASA0014/luminaire/" + lightId;

const int num_leds = 72;
const int payload_size = num_leds * 3;
byte RGBpayload[payload_size];
byte lastSent[payload_size];
bool forceSend = true;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

const int NUM_BUTTONS = 6;
const int buttonPins[NUM_BUTTONS] = {1, 2, 3, 4, 5, 0};
enum Mode { FLUTED_WOOD, GREY_OAK, BLUE_GLASS, OAK_BLOCK, SILVER_MOSAIC, FABRIC };
const int IDLE = -1;
const char* modeNames[NUM_BUTTONS] = {"Fluted wood", "Grey oak", "Blue glass", "Oak block", "Silver mosaic", "Fabric"};

// Main colour of each material - used by the idle palette
const byte palette[NUM_BUTTONS][3] = {
  {140,  60,  15},   // fluted wood - brown
  {190, 200, 225},   // grey oak    - cool grey
  {  0,  60, 255},   // blue glass
  {255, 190,  20},   // oak block   - yellow
  {210, 215, 230},   // silver mosaic
  {200,  40,  15}    // fabric      - terracotta
};
const int idleOrder[NUM_BUTTONS] = {OAK_BLOCK, SILVER_MOSAIC, FABRIC, FLUTED_WOOD, GREY_OAK, BLUE_GLASS};

const unsigned long DEBOUNCE_MS = 30;
int lastReading[NUM_BUTTONS];
int stableState[NUM_BUTTONS];
unsigned long lastChange[NUM_BUTTONS];

const unsigned long FRAME_MS = 100;
const unsigned long IDLE_TIMEOUT = 60000;   // 1 minute
unsigned long lastFrameTime = 0;
unsigned long lastPressTime = 0;
unsigned long frame = 0;
int currentMode = IDLE;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) { }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastReading[i] = HIGH;
    stableState[i] = HIGH;
    lastChange[i] = 0;
  }

  startWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);
  reconnectMQTT();

  Serial.println("Ready - showing the board palette");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) startWifi();
  if (!client.connected()) reconnectMQTT();
  client.loop();

  checkButtons();

  // No press for a minute -> back to the palette
  if (currentMode != IDLE && millis() - lastPressTime > IDLE_TIMEOUT) {
    currentMode = IDLE;
    Serial.println("Back to palette");
  }

  // Non-blocking frame timer, so buttons stay responsive while animating
  if (millis() - lastFrameTime >= FRAME_MS) {
    lastFrameTime = millis();
    drawCurrentMode();
    publishIfChanged();
    frame++;
  }
}

void checkButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(buttonPins[i]);
    if (reading != lastReading[i]) {
      lastChange[i] = millis();
      lastReading[i] = reading;
    }
    // Debounce: accept a change only once it has been stable for DEBOUNCE_MS
    if (millis() - lastChange[i] > DEBOUNCE_MS && reading != stableState[i]) {
      stableState[i] = reading;
      if (stableState[i] == LOW) selectMode(i);
    }
  }
}

void selectMode(int mode) {
  currentMode = mode;
  lastPressTime = millis();
  frame = 0;
  Serial.print("Pressed -> ");
  Serial.println(modeNames[mode]);
}

void drawCurrentMode() {
  switch (currentMode) {
    case IDLE:          drawPalette();      break;
    case FLUTED_WOOD:   drawFlutedWood();   break;
    case GREY_OAK:      drawGreyOak();      break;
    case BLUE_GLASS:    drawBlueGlass();    break;
    case OAK_BLOCK:     drawOakBlock();     break;
    case SILVER_MOSAIC: drawSilverMosaic(); break;
    case FABRIC:        drawFabric();       break;
  }
}

// Idle: 12 columns, 2 per material, in button order
void drawPalette() {
  for (int i = 0; i < num_leds; i++) {
    int m = colOf(i) / 2;
    setPixel(i, palette[m][0], palette[m][1], palette[m][2]);
  }
}

// --- oak block ---
void drawOakBlock() {
  const int stripeWidth = 6;
  for (int i = 0; i < num_leds; i++) {
    int band = ((i + frame) / stripeWidth) % 2;
    if (band == 0) setPixel(i, 255, 190, 20);
    else           setPixel(i, 255, 130, 0);
  }
}

// --- fluted wood ---
void drawFlutedWood() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 140, 60, 15);
    else                   setPixel(i, 50, 20, 5);
  }
}

// --- grey oak ---
void drawGreyOak() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 230, 230, 235);
    else                   setPixel(i, 30, 35, 50);
  }
}

// --- blue glass ---
void drawBlueGlass() {
  for (int i = 0; i < num_leds; i++) {
    if (i % 12 < 3) setPixel(i, 0, 70, 255);
    else            setPixel(i, 0, 190, 255);
  }
}

// --- silver mosaic: three tones in mosaic squares, shifting diagonally ---
void drawSilverMosaic() {
  const int cell = 2;
  int shift = frame / 4;
  for (int i = 0; i < num_leds; i++) {
    int tone = (colOf(i) / cell + rowOf(i) / cell + shift) % 3;
    if (tone == 0)      setPixel(i, 250, 250, 210);   // white marble
    else if (tone == 1) setPixel(i, 0, 0, 0);   // textured silver check silver again
    else                setPixel(i, 80, 80, 90);      // brushed steel
    if ((i * 7 + frame) % 37 == 0) setPixel(i, 255, 255, 255);   // glint
  }
}

// --- fabric ---
void drawFabric() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 255, 40, 15);
    else                   setPixel(i, 255, 200, 170);   // terracotta - check again
  }
}

// LEDs are numbered down each column: 0-5 = column 0, 6-11 = column 1, ...
int colOf(int i) { return i / 6; }
int rowOf(int i) { return i % 6; }

void setPixel(int pixel, byte r, byte g, byte b) {
  if (pixel < 0 || pixel >= num_leds) return;
  RGBpayload[pixel * 3 + 0] = r;
  RGBpayload[pixel * 3 + 1] = g;
  RGBpayload[pixel * 3 + 2] = b;
}

// Publish only when the frame has changed, so static patterns are sent once
void publishIfChanged() {
  if (!client.connected()) return;
  if (!forceSend && memcmp(RGBpayload, lastSent, payload_size) == 0) return;
  if (client.publish(mqtt_topic.c_str(), RGBpayload, payload_size)) {
    memcpy(lastSent, RGBpayload, payload_size);
    forceSend = false;
  } else {
    Serial.println("Publish failed");
  }
}

void startWifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(SECRET_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(5000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void reconnectMQTT() {
  while (!client.connected()) {
    String clientId = "board-mkr1010-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT as ");
    Serial.println(clientId);
    if (client.connect(clientId.c_str(), SECRET_MQTTUSER, SECRET_MQTTPASS)) {
      Serial.println("MQTT connected");
      forceSend = true;
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" - retrying in 5 s");
      delay(5000);
    }
  }
}
