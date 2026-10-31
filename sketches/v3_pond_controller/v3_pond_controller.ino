/*
  v3 POND CONTROLLER -> Vespera  |  CASA0014, Arduino MKR1010 (Iteration 1)
  -----------------------------------------------------------------
  A paper-art pond scene with 5 hidden push buttons. Pressing an element
  in the scene plays a matching light pattern on Vespera.

    Button 1 (pin 1) : Boat      -> moving yellow / blue stripes  (DONE - worked example)
    Button 2 (pin 2) : Flowers   -> pink / green                  (placeholder - animate it yourself)
    Button 3 (pin 3) : Koi fish  -> orange / white                (placeholder - animate it yourself)
    Button 4 (pin 4) : Lily pads -> greens                        (placeholder - animate it yourself)
    Button 5 (pin 5) : Water     -> pond blue (start-up state)    (placeholder - animate it yourself)

  Vespera interface (from the CASA0014 "Updating Vespera" workshop):
    topic   : student/CASA0014/luminaire/<lightId>
    payload : 216 raw bytes = 72 LEDs x (R,G,B), whole light sent every time

  Wiring: each button has one leg on its pin and the other leg on GND.
  INPUT_PULLUP means the pin reads HIGH normally and LOW when pressed,
  so no resistors are needed.

  Folder must be named "v3_pond_controller". Needs arduino_secrets.h alongside.
  Libraries: WiFiNINA, PubSubClient.

  AI acknowledgement: framework drafted with Claude (Anthropic) - see README.
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"

// ---------- COPY THESE FROM YOUR WORKING WORKSHOP SKETCH ----------
const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1884;     // CONFIRM against your workshop sketch
String lightId = "15";              // your allocated Vespera number
// ------------------------------------------------------------------

String mqtt_topic = "student/CASA0014/luminaire/" + lightId;

const int num_leds = 72;
const int payload_size = num_leds * 3;
byte RGBpayload[payload_size];      // the frame we are building
byte lastSent[payload_size];        // the frame Vespera is currently showing
bool forceSend = true;              // make sure the very first frame is sent

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ---------- Buttons ----------
const int NUM_BUTTONS = 5;
const int buttonPins[NUM_BUTTONS] = {1, 2, 3, 4, 5};
enum Mode { BOAT, FLOWERS, FISH, LILYPADS, WATER };
const char* modeNames[NUM_BUTTONS] = {"Boat", "Flowers", "Koi fish", "Lily pads", "Water"};

const unsigned long DEBOUNCE_MS = 30;   // ignore contact bounce shorter than this
int lastReading[NUM_BUTTONS];
int stableState[NUM_BUTTONS];
unsigned long lastChange[NUM_BUTTONS];

// ---------- Animation timing ----------
const unsigned long FRAME_MS = 150;     // ~6-7 frames a second; gentle on the shared broker
unsigned long lastFrameTime = 0;
unsigned long frame = 0;                // counts up; animations use it to move
int currentMode = WATER;                // the pond starts calm

// ==================================================================
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) { }   // don't hang if no USB

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastReading[i] = HIGH;
    stableState[i] = HIGH;
    lastChange[i] = 0;
  }

  startWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);                    // 216-byte payload + topic fits comfortably
  reconnectMQTT();

  Serial.println("Ready - press a button on the pond");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) startWifi();
  if (!client.connected()) reconnectMQTT();
  client.loop();

  checkButtons();

  // Non-blocking timer: draw and send a frame every FRAME_MS,
  // so buttons stay responsive while animations run.
  if (millis() - lastFrameTime >= FRAME_MS) {
    lastFrameTime = millis();
    drawCurrentMode();
    publishIfChanged();
    frame++;
  }
}

// ==================================================================
// BUTTONS
// ==================================================================
void checkButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(buttonPins[i]);

    if (reading != lastReading[i]) {          // something changed - restart the timer
      lastChange[i] = millis();
      lastReading[i] = reading;
    }

    // Only accept the change once it has been steady for DEBOUNCE_MS
    if (millis() - lastChange[i] > DEBOUNCE_MS && reading != stableState[i]) {
      stableState[i] = reading;
      if (stableState[i] == LOW) {            // LOW = pressed (INPUT_PULLUP)
        selectMode(i);
      }
    }
  }
}

void selectMode(int mode) {
  currentMode = mode;
  frame = 0;                                  // restart the animation from the beginning
  Serial.print("Button pressed -> ");
  Serial.println(modeNames[mode]);
}

// ==================================================================
// PATTERNS
// ==================================================================
void drawCurrentMode() {
  switch (currentMode) {
    case BOAT:     drawBoat();     break;
    case FLOWERS:  drawFlowers();  break;
    case FISH:     drawFish();     break;
    case LILYPADS: drawLilyPads(); break;
    case WATER:    drawWater();    break;
  }
}

// --- WORKED EXAMPLE: moving yellow / blue stripes (the rower's hat and the water) ---
// Each LED works out which stripe it is in. Adding "frame" shifts every LED
// one step along each tick, so the stripes appear to travel around Vespera.
void drawBoat() {
  const int stripeWidth = 6;                         // LEDs per stripe
  for (int i = 0; i < num_leds; i++) {
    int band = ((i + frame) / stripeWidth) % 2;      // 0 or 1, alternating every 6 LEDs
    if (band == 0) {
      setPixel(i, 255, 190, 0);                      // straw-hat yellow
    } else {
      setPixel(i, 0, 70, 255);                       // pond blue
    }
  }
}

// --- PLACEHOLDER: static pink / green blocks ---
// YOUR TURN: make the flowers "bloom" - e.g. pink brightness rising and
// falling with frame, or pink spreading outwards from the middle LEDs.
void drawFlowers() {
  for (int i = 0; i < num_leds; i++) {
    if ((i / 4) % 2 == 0) setPixel(i, 255, 60, 150); // lotus pink
    else                  setPixel(i, 40, 180, 60);  // leaf green
  }
}

// --- PLACEHOLDER: static orange with white patches ---
// YOUR TURN: make a koi "swim" - e.g. a short orange+white block that moves
// along the LEDs (use frame like drawBoat) over a dark blue background.
void drawFish() {
  for (int i = 0; i < num_leds; i++) {
    if (i % 12 < 3) setPixel(i, 255, 255, 255);      // white patch
    else            setPixel(i, 255, 80, 0);         // koi orange
  }
}

// --- PLACEHOLDER: static two-tone green ---
// YOUR TURN: make the lily pads "float" - e.g. a slow breathing brightness.
void drawLilyPads() {
  for (int i = 0; i < num_leds; i++) {
    if ((i / 6) % 2 == 0) setPixel(i, 40, 160, 70);  // dark pad green
    else                  setPixel(i, 120, 220, 140); // light pad green
  }
}

// --- PLACEHOLDER: calm solid blue ---
// YOUR TURN: add ripples - e.g. a lighter blue band that drifts slowly.
void drawWater() {
  for (int i = 0; i < num_leds; i++) {
    setPixel(i, 0, 40, 160);
  }
}

// ==================================================================
// VESPERA HELPERS
// ==================================================================
void setPixel(int pixel, byte r, byte g, byte b) {
  if (pixel < 0 || pixel >= num_leds) return;        // stay inside the array
  RGBpayload[pixel * 3 + 0] = r;
  RGBpayload[pixel * 3 + 1] = g;
  RGBpayload[pixel * 3 + 2] = b;
}

// Only publish when the frame differs from what Vespera already shows.
// Static patterns are therefore sent once, not 7 times a second.
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
// CONNECTIVITY (same pattern as the workshop sketch)
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
      forceSend = true;                       // resend the current frame after a reconnect
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" - retrying in 5 s");
      delay(5000);
    }
  }
}
