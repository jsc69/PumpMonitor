#define SENSOR_PIN 34
const float VCC = 3.3;
const int ADC_MAX = 4095;

// Dein ermittelter Faktor (Anpassung an Poti-Stellung)
float calibrationFactor = 5.35; 

void setup() {
  Serial.begin(115200);
  // Kurze Pause, damit sich die Spannungen auf dem Modul stabilisieren
  delay(2000); 
  Serial.println("Messung gestartet (Dynamischer Offset-Modus)");
}

void loop() {
  // Wir messen 10 Mal und mitteln das Ergebnis für das Display
  float sumCurrent = 0;
  int numAverages = 10;

  for (int i = 0; i < numAverages; i++) {
    sumCurrent += getSCTCurrent(40); // 40ms = 2 Perioden bei 50Hz
  }
  
  float finalCurrent = sumCurrent / numAverages;

  Serial.print("Strom: ");
  Serial.print(finalCurrent, 3);
  Serial.println(" A");
  
  delay(100);
}

float getSCTCurrent(int duration) {
  unsigned long sumSquare = 0;
  long sumRaw = 0;
  unsigned int sampleCount = 0;
  unsigned long startTime = millis();
  
  // 1. Erfassung der Rohwerte
  while (millis() - startTime < duration) {
    int rawValue = analogRead(SENSOR_PIN);
    sumRaw += rawValue;
    sumSquare += (unsigned long)rawValue * rawValue;
    sampleCount++;
  }
  
  if (sampleCount == 0) return 0;

  // 2. DYNAMISCHE NULLPUNKT-BERECHNUNG
  // Wir berechnen den Mittelwert GENAU dieser 40ms. 
  // Das fängt jegliches Wandern der Null-Linie sofort ab.
  float currentMean = (float)sumRaw / sampleCount;
  
  // 3. RMS BERECHNUNG (Varianz-Formel)
  // RMS = sqrt( E[X^2] - (E[X])^2 )
  float meanSquare = (float)sumSquare / sampleCount;
  float rmsRaw = sqrt(max(0.0f, meanSquare - (currentMean * currentMean)));
  
  // 4. Skalierung
  float current = (rmsRaw * (VCC / ADC_MAX)) * calibrationFactor;

  // 5. NOISE GATE
  // Falls trotz Mathe noch 10-50mA "zappeln", schneiden wir das hier ab.
  if (current < 0.08) return 0.0;
  
  return current;
}