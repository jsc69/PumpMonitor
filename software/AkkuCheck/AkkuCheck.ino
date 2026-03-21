const int batteryPin = 36; 
const int ledPin = 2;       // Die interne LED beim Wemos D1 Mini 32
const float factor = 1.867; // Dein 130k/150k Teiler

void setup() {
  Serial.begin(115200);
  pinMode(batteryPin, INPUT);
  pinMode(ledPin, OUTPUT);
  analogSetAttenuation(ADC_11db); 
  
  Serial.println("Messung gestartet - LED blinkt bei jedem Durchlauf...");
}

void loop() {
  // 1. LED einschalten
  digitalWrite(ledPin, HIGH); 

  // 2. Messung durchführen
  int count=100;
  int raws = 0;
  for (int i = 0; i < count; i++) {
    raws += analogRead(batteryPin);
  } 
  int rawValue = raws / count;
  float voltage = rawValue / 648.4; //=> factor = (rwaValue * 3.3 / 4095)

  // 3. Werte ausgeben
  Serial.print("");
  Serial.print(rawValue);
  Serial.print(", ");
  Serial.print(voltage);
  Serial.println(" V");

  // 4. Kurze Pause mit leuchtender LED (100ms)
  delay(100);

  // 5. LED ausschalten und auf die nächste Messung warten
  digitalWrite(ledPin, LOW); 
  delay(900); // Insgesamt 1 Sekunde Intervall
}