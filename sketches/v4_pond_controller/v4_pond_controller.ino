/*
  Vespera Pond Controller - v4 (Iteration 2)
  CASA0014 Connected Environments, UCL - Arduino MKR WiFi 1010

  Six push buttons hidden under a paper pond scene. Each button sends a
  different light pattern to the Vespera installation over MQTT.

    Button 1 (pin 1)  Boat          moving yellow / blue stripes
    Button 2 (pin 2)  Flowers       pink / green
    Button 3 (pin 3)  Koi fish      orange with cream patches
    Button 4 (pin 4)  Lily pads     dark / pastel green columns
    Button 5 (pin 5)  Water         dark / light blue columns (start-up state)
    Button 6 (pin 0)  Purple lotus  dark / light purple columns

  Changes from v3: 6th button, two-tone column colours, cream koi, 100 ms frames.

  Vespera: topic student/CASA0014/luminaire/<lightId>
           12 columns x 6 rows = 72 LEDs, numbered down each column
           payload = 72 x RGB = 216 bytes, whole light sent each time
  Wiring:  each button between its pin and GND (diagonal legs), INPUT_PULLUP
           pin 6 avoided - it is wired to the MKR1010's built-in LED
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
enum Mode { BOAT, FLOWERS, FISH, LILYPADS, WATER, PURPLE_LOTUS };
const char* modeNames[NUM_BUTTONS] = {"Boat", "Flowers", "Koi fish", "Lily pads", "Water", "Purple lotus"};

const unsigned long DEBOUNCE_MS = 30;
int lastReading[NUM_BUTTONS];
int stableState[NUM_BUTTONS];
unsigned long lastChange[NUM_BUTTONS];

const unsigned long FRAME_MS = 100;
unsigned long lastFrameTime = 0;
unsigned long frame = 0;
int currentMode = WATER;

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

  Serial.println("Ready - press a button on the pond");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) startWifi();
  if (!client.connected()) reconnectMQTT();
  client.loop();

  checkButtons();

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
  frame = 0;
  Serial.print("Button pressed -> ");
  Serial.println(modeNames[mode]);
}

void drawCurrentMode() {
  switch (currentMode) {
    case BOAT:         drawBoat();        break;
    case FLOWERS:      drawFlowers();     break;
    case FISH:         drawFish();        break;
    case LILYPADS:     drawLilyPads();    break;
    case WATER:        drawWater();       break;
    case PURPLE_LOTUS: drawPurpleLotus(); break;
  }
}

// Stripes one column (6 LEDs) wide, shifted one LED each frame
void drawBoat() {
  const int stripeWidth = 6;
  for (int i = 0; i < num_leds; i++) {
    int band = ((i + frame) / stripeWidth) % 2;
    if (band == 0) setPixel(i, 255, 190, 0);
    else           setPixel(i, 0, 70, 255);
  }
}

void drawFlowers() {
  for (int i = 0; i < num_leds; i++) {
    if ((i / 4) % 2 == 0) setPixel(i, 255, 60, 150);
    else                  setPixel(i, 40, 180, 60);
  }
}

void drawFish() {
  for (int i = 0; i < num_leds; i++) {
    if (i % 12 < 3) setPixel(i, 255, 230, 180);
    else            setPixel(i, 255, 100, 20);
  }
}

void drawLilyPads() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 40, 160, 70);
    else                   setPixel(i, 120, 220, 140);
  }
}

void drawWater() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 0, 30, 120);
    else                   setPixel(i, 80, 180, 255);
  }
}

void drawPurpleLotus() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 90, 0, 160);
    else                   setPixel(i, 200, 140, 255);
  }
}

// LEDs are numbered down each column: 0-5 = column 0, 6-11 = column 1, ...
int colOf(int i) { return i / 6; }

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
    String clientId = "pond-mkr1010-" + String(random(0xffff), HEX);
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
