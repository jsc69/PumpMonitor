#define PIN_BUTTON_DOWN 27  // Button 1
#define PIN_BUTTON_UP   25  // Button 2

#define LED PIN_BUTTON_UP


void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED, OUTPUT);
  
  Serial.begin(115200);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(LED, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
  Serial.println("ON");
  delay(1000);                      // wait for a second
  digitalWrite(LED, LOW);   // change state of the LED by setting the pin to the LOW voltage level
  Serial.println("OFF");
  delay(1000);                      // wait for a second
}
