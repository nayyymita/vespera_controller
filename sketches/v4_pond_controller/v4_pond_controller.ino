/*
  v4 POND CONTROLLER -> Vespera  |  CASA0014, Arduino MKR1010
  -----------------------------------------------------------------
  Changes from v3 (trial 1):
    - 6th button on pin 0: Purple lotus
    - Grid helpers: Vespera is 12 columns x 6 rows, LEDs numbered DOWN each column
      (LED 0-5 = column 0, LED 6-11 = column 1 ...) -> colOf(), rowOf(), distFromCentre()
    - Brightness helpers: breath() and setPixelScaled() for breathing / ripple effects
    - Faster frames (100 ms) so fades look smoother

    Button 1 (pin 1) : Boat         -> moving yellow / blue stripes   (DONE - worked example)
    Button 2 (pin 2) : Flowers      -> pink / green blocks            (static; bloom recipe below)
    Button 3 (pin 3) : Koi fish     -> orange with cream patches      (static; swim recipe below)
    Button 4 (pin 4) : Lily pads    -> dark / pastel green columns    (static; breathing recipe below)
    Button 5 (pin 5) : Water        -> dark / light blue columns      (static; ripple recipe below)
    Button 6 (pin 0) : Purple lotus -> dark / light purple columns    (static)

  Wiring: each button -> one leg on its pin, the DIAGONAL leg on GND (INPUT_PULLUP, no resistors)
  Not pin 6: on the MKR1010 it is wired to the built-in LED, which fights the pull-up.

  AI acknowledgement: framework, helpers and Boat example drafted with Claude (Anthropic);
  animations 2-6 written by me - see README.
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"

// ---------- MQTT / Vespera ----------
const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1884;
String lightId = "15";
String mqtt_topic = "student/CASA0014/luminaire/" + lightId;

const int num_leds = 72;                  // 12 columns x 6 rows
const int payload_size = num_leds * 3;
byte RGBpayload[payload_size];            // the frame we are building
byte lastSent[payload_size];              // the frame Vespera is currently showing
bool forceSend = true;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ---------- Buttons ----------
const int NUM_BUTTONS = 6;
const int buttonPins[NUM_BUTTONS] = {1, 2, 3, 4, 5, 0};
enum Mode { BOAT, FLOWERS, FISH, LILYPADS, WATER, PURPLE_LOTUS };
const char* modeNames[NUM_BUTTONS] = {"Boat", "Flowers", "Koi fish", "Lily pads", "Water", "Purple lotus"};

const unsigned long DEBOUNCE_MS = 30;
int lastReading[NUM_BUTTONS];
int stableState[NUM_BUTTONS];
unsigned long lastChange[NUM_BUTTONS];

// ---------- Animation timing ----------
const unsigned long FRAME_MS = 100;       // 10 frames a second; raise to 150 if Lumi lags
unsigned long lastFrameTime = 0;
unsigned long frame = 0;                  // counts up every frame; animations use it to move
int currentMode = WATER;                  // the pond starts calm

// ==================================================================
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

  if (millis() - lastFrameTime >= FRAME_MS) {
    lastFrameTime = millis();
    drawCurrentMode();
    publishIfChanged();
    frame++;
  }
}

// ==================================================================
// BUTTONS (unchanged from v3)
// ==================================================================
void checkButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(buttonPins[i]);
    if (reading != lastReading[i]) {
      lastChange[i] = millis();
      lastReading[i] = reading;
    }
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

// ==================================================================
// PATTERNS
// ==================================================================
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

// --- WORKED EXAMPLE: yellow / blue stripes, one column wide, moving sideways ---
void drawBoat() {
  const int stripeWidth = 6;                         // 6 LEDs = exactly one column
  for (int i = 0; i < num_leds; i++) {
    int band = ((i + frame) / stripeWidth) % 2;
    if (band == 0) setPixel(i, 255, 190, 0);         // straw-hat yellow
    else           setPixel(i, 0, 70, 255);          // pond blue
  }
}

// --- YOUR TURN: LILY PADS breathing (start with this one - easiest) ---
// Recipe:
//   1. Before the for-loop: get a brightness that rises and falls:
//        byte lvl = breath(frame, 40);     // 40 frames = 4 s per breath at 100 ms
//   2. Inside the loop: setPixelScaled(i, 40, 180, 70, lvl);
//   Try: never fully dark -> use a floor, e.g. 60 + lvl * 195 / 255
//   Try: rows 0-2 one green, rows 3-5 a lighter green (use rowOf(i))
void drawLilyPads() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 40, 160, 70);   // dark pad green
    else                   setPixel(i, 120, 220, 140); // light pastel green
  }
}

// --- YOUR TURN: WATER ripples spreading out from the centre ---
// Idea: every column "breathes", but outer columns breathe a little LATER than
// the middle ones -> the bright band looks like it travels outwards.
// Recipe (inside the loop):
//   int d = distFromCentre(i);                      // 1 (middle) ... 11 (edges)
//   byte lvl = breath(frame + 100 - d * 4, 30);     // bigger d = later
//   setPixelScaled(i, 0, 90, 255, lvl);
//   Try changing the 4 (ripple spacing) and the 30 (speed).
//   Try: dark-blue background instead of black -> add a floor like the lily pads.
void drawWater() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 0, 30, 120);    // dark blue
    else                   setPixel(i, 80, 180, 205);  // light blue
  }
}

// --- YOUR TURN: KOI swimming across Vespera ---
// Recipe:
//   int head = frame % 12;                // which column the fish's head is in
//   int tail = (head + 11) % 12;          // the column just behind it (wraps around)
//   inside the loop:
//     if colOf(i) == head  -> orange (255, 80, 0)
//     else if colOf(i) == tail -> white (255, 255, 255)
//     else -> dark water (0, 20, 60)
//   Try: slow it down with (frame / 2) % 12
//   Try: a second fish going the other way: (11 - head)
void drawFish() {
  for (int i = 0; i < num_leds; i++) {
    if (i % 12 < 3) setPixel(i, 255, 230, 180);      // cream patch
    else            setPixel(i, 255, 100, 20);       // koi orange
  }
}

// --- YOUR TURN: FLOWERS blooming from the centre ---
// Recipe:
//   int radius = (frame / 3) % 13;        // grows 0 -> 12, then starts again
//   inside the loop:
//     if distFromCentre(i) <= radius -> petal pink (255, 60, 150)
//     else                           -> leaf green (40, 180, 60)
//   Try: fade the pink in with setPixelScaled instead of switching instantly.
void drawFlowers() {
  for (int i = 0; i < num_leds; i++) {
    if ((i / 4) % 2 == 0) setPixel(i, 255, 60, 150); // static placeholder
    else                  setPixel(i, 40, 180, 60);
  }
}

// --- YOUR TURN: PURPLE LOTUS (button 6) - your own design ---
// Ideas: purple breathing with a white "flower centre" in columns 5 and 6,
// or purple petals opening like the Flowers bloom but slower.
void drawPurpleLotus() {
  for (int i = 0; i < num_leds; i++) {
    if (colOf(i) % 2 == 0) setPixel(i, 120, 0, 160);    // dark purple
    else                   setPixel(i, 200, 140, 255); // light purple
  }
}

// ==================================================================
// GRID + BRIGHTNESS HELPERS (new in v4)
// ==================================================================
int colOf(int i) { return i / 6; }                   // 0 (left) ... 11 (right)
int rowOf(int i) { return i % 6; }                   // 0 (top)  ... 5 (bottom)

// How far an LED's column is from the middle of Vespera:
// 1 = the two centre columns (5 and 6), 11 = the outer edges.
int distFromCentre(int i) {
  int d = colOf(i) * 2 - 11;                         // -11 ... +11
  return (d < 0) ? -d : d;
}

// A brightness 0-255 that rises and falls smoothly, one full breath every `period` frames.
byte breath(unsigned long f, int period) {
  float phase = (f % period) / (float)period;        // 0.0 -> 1.0
  float level = (1.0 - cos(phase * 2.0 * PI)) / 2.0; // 0 -> 1 -> 0
  return (byte)(level * 255);
}

// Like setPixel, but dims the colour: level 0 = off, 255 = full colour.
void setPixelScaled(int pixel, byte r, byte g, byte b, byte level) {
  setPixel(pixel, (r * level) / 255, (g * level) / 255, (b * level) / 255);
}

// ==================================================================
// VESPERA HELPERS (unchanged from v3)
// ==================================================================
void setPixel(int pixel, byte r, byte g, byte b) {
  if (pixel < 0 || pixel >= num_leds) return;
  RGBpayload[pixel * 3 + 0] = r;
  RGBpayload[pixel * 3 + 1] = g;
  RGBpayload[pixel * 3 + 2] = b;
}

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

// ==================================================================
// CONNECTIVITY (unchanged from v3)
// ==================================================================
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
