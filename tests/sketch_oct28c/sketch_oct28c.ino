// Button test: shows the live state of pins 1-5. No WiFi, no MQTT.
const int pins[5] = {1, 2, 3, 4, 5};

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) pinMode(pins[i], INPUT_PULLUP);
}

void loop() {
  for (int i = 0; i < 5; i++) {
    Serial.print("pin ");
    Serial.print(pins[i]);
    Serial.print(digitalRead(pins[i]) == LOW ? "=PRESSED  " : "=up       ");
  }
  Serial.println();
  delay(200);
}