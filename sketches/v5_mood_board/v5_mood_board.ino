/*
  v5 MATERIAL MOOD BOARD -> Vespera  |  CASA0014, Arduino MKR1010
  ---------------------------------------------------------------
  Six material samples sit on hidden push buttons. Pressing a
  material changes Vespera's light to suit it.

  Changes from v4:
    - Pond scene replaced by a material mood board (6 interactive tiles)
    - Idle state: Vespera shows the board's palette, 2 columns per material,
      drifting slowly sideways
    - Pressing a tile shows that material's light scene
    - After 60 s with no press, Vespera returns to the idle palette
    - Material colours stored once in palette[6][3]
    - Final samples: blue glass dropped, cork added, colours matched to the real samples

    Button 1 (pin 1) : Cork          -> orange-brown with dark speckles
    Button 2 (pin 2) : Warm wood     -> wood-orange grain columns
    Button 3 (pin 3) : Fluted wood   -> pale birch rows with groove shadows
    Button 4 (pin 4) : Oak           -> yellow / warm-white stripes, moving
    Button 5 (pin 5) : Glass mosaic  -> yellow-white / yellow-green / black, shifting + glint
    Button 6 (pin 0) : Fabric        -> white / charcoal woven checker
    No press for 60 s                -> idle palette

  Vespera: topic student/CASA0014/luminaire/15
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
enum Mode { CORK, WARM_WOOD, FLUTED_WOOD, OAK, GLASS_MOSAIC, FABRIC };
const int IDLE = -1;
const char* modeNames[NUM_BUTTONS] = {"Cork", "Warm wood", "Fluted wood", "Oak", "Glass mosaic", "Fabric"};

// Main colour of each material, in button order - used by the idle palette
const byte palette[NUM_BUTTONS][3] = {
  {190,  75,  15},   // cork         - dark orange-brown
  {240, 130,  45},   // warm wood    - wood orange
  {255, 200, 120},   // fluted wood  - pale birch
  {255, 190,  20},   // oak          - yellow
  {160, 220,  30},   // glass mosaic - yellow-green
  {230, 230, 225}    // fabric       - white
};

// Order the materials appear across Vespera in the idle state
const int idleOrder[NUM_BUTTONS] = {OAK, FABRIC, GLASS_MOSAIC, FLUTED_WOOD, WARM_WOOD, CORK};

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
    case IDLE:         drawPalette();     break;
    case CORK:         drawCork();        break;
    case WARM_WOOD:    drawWarmWood();    break;
    case FLUTED_WOOD:  drawFlutedWood();  break;
    case OAK:          drawOak();         break;
    case GLASS_MOSAIC: drawGlassMosaic(); break;
    case FABRIC:       drawFabric();      break;
  }
}

// Idle: the six material colours drift sideways, 2 columns each
void drawPalette() {
  int shift = (frame / 5) % 12;            // one column every 0.5 s
  for (int i = 0; i < num_leds; i++) {
    int c = (colOf(i) + shift) % 12;
    int m = idleOrder[c / 2];
    setPixel(i, palette[m][0], palette[m][1], palette[m][2]);
  }
}

// --- cork: orange-brown with dark speckles ---
void drawCork() {
  for (int i = 0; i < num_leds; i++) {
    if ((i * 37) % 7 == 0) setPixel(i, 60, 20, 0);      // dark speckle
    else                   setPixel(i, 190, 75, 15);    // cork
  }
}

// --- warm wood: vertical grain columns ---
void drawWarmWood() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 240, 130, 45);   // wood orange
    else                   setPixel(i, 255, 175, 95);   // lighter grain
  }
}

// --- fluted wood: horizontal flutes, like the sticks ---
void drawFlutedWood() {
  for (int i = 0; i < num_leds; i++) {
    if (rowOf(i) % 2 == 0) setPixel(i, 255, 200, 120);  // pale birch
    else                   setPixel(i, 140, 80, 30);    // groove shadow
  }
}

// --- oak: yellow and warm-white stripes, moving sideways ---
void drawOak() {
  const int stripeWidth = 6;
  for (int i = 0; i < num_leds; i++) {
    int band = ((i + frame) / stripeWidth) % 2;
    if (band == 0) setPixel(i, 205, 100, 20);           // yellow
    else           setPixel(i, 255, 240, 200);          // warm white
  }
}

// --- glass mosaic: three tones in 2x2 squares, shifting diagonally, with glints ---
void drawGlassMosaic() {
  const int cell = 2;
  int shift = frame / 4;
  for (int i = 0; i < num_leds; i++) {
    int tone = (colOf(i) / cell + rowOf(i) / cell + shift) % 3;
    if (tone == 0)      setPixel(i, 255, 240, 170);     // yellow-white
    else if (tone == 1) setPixel(i, 230, 250, 10);      // yellow-green
    else                setPixel(i, 0, 0, 0);           // black
    if ((i * 7 + frame) % 37 == 0) setPixel(i, 255, 255, 255);   // glint
  }
}

// --- fabric: fine woven checker, white and charcoal ---
void drawFabric() {
  for (int i = 0; i < num_leds; i++) {
    if ((colOf(i) + rowOf(i)) % 2 == 0) setPixel(i, 230, 230, 225);
    else                                 setPixel(i, 10, 10, 15);
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
