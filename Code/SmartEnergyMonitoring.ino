#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

hd44780_I2Cexp lcd;

#define ACS712_PIN          34
#define ZMPT101B_PIN        35
#define ACS712_SENSITIVITY  0.185
#define ADC_REF_VOLTAGE     3.3
#define ADC_RESOLUTION      4096.0
#define SAMPLES             1000
#define TARIFF_PER_KWH      6.5

float voltageCalibration = 0.53;
int offsetACS712   = 2048;
int offsetZMPT101B = 2048;

float voltage   = 0.0;
float current   = 0.0;
float power     = 0.0;
float energyKWh = 0.0;
float units     = 0.0;
float cost      = 0.0;

unsigned long lastTime     = 0;
unsigned long displayTimer = 0;
int           displayPage  = 0;

void lcdRow(int row, String text) {
  while ((int)text.length() < 16) text += " ";
  text = text.substring(0, 16);
  lcd.setCursor(0, row);
  lcd.print(text);
}

void calibrateOffsets() {
  lcdRow(0, "Calibrating...  ");
  lcdRow(1, "Please Wait...  ");
  long sumACS = 0, sumZMPT = 0;
  for (int i = 0; i < 500; i++) {
    sumACS  += analogRead(ACS712_PIN);
    sumZMPT += analogRead(ZMPT101B_PIN);
    delay(1);
  }
  offsetACS712   = sumACS  / 500;
  offsetZMPT101B = sumZMPT / 500;
  Serial.print("ACS712 offset  : "); Serial.println(offsetACS712);
  Serial.print("ZMPT101B offset: "); Serial.println(offsetZMPT101B);
  delay(1000);
}

float readVoltageRMS() {
  long sumSquares = 0;
  for (int i = 0; i < SAMPLES; i++) {
    long raw    = analogRead(ZMPT101B_PIN);
    long offset = raw - offsetZMPT101B;
    sumSquares += offset * offset;
    delayMicroseconds(50);
  }
  float rms  = sqrt((float)sumSquares / SAMPLES);
  float vrms = (rms / ADC_RESOLUTION) * ADC_REF_VOLTAGE * voltageCalibration * 1000.0;
  return vrms;
}

float readCurrentRMS() {
  long sumSquares = 0;
  for (int i = 0; i < SAMPLES; i++) {
    long raw    = analogRead(ACS712_PIN);
    long offset = raw - offsetACS712;
    sumSquares += offset * offset;
    delayMicroseconds(50);
  }
  float rms        = sqrt((float)sumSquares / SAMPLES);
  float voltageRMS = (rms / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
  float currentRMS = voltageRMS / ACS712_SENSITIVITY;
  if (currentRMS < 0.05) currentRMS = 0.0;
  return currentRMS;
}

void showPage0() {
  lcdRow(0, "V=" + String(voltage, 1) + "V");
  lcdRow(1, "I=" + String(current, 3) + "A");
}

void showPage1() {
  lcdRow(0, "P=" + String(power, 1) + "W");
  lcdRow(1, "Units=" + String(units, 4));
}

void showPage2() {
  lcdRow(0, "E=" + String(energyKWh, 4) + "kWh");
  lcdRow(1, "Cost=Rs." + String(cost, 4));
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(100000);

  int status = lcd.begin(16, 2);
  if (status) {
    Serial.print("LCD Error: ");
    Serial.println(status);
    hd44780::fatalError(status);
  }

  lcd.backlight();
  lcd.clear();

  pinMode(ACS712_PIN,   INPUT);
  pinMode(ZMPT101B_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  lcdRow(0, "Smart Energy    ");
  lcdRow(1, "Monitor         ");
  delay(2000);
  lcd.clear();

  calibrateOffsets();
  lcd.clear();

  Serial.println("V(V) | I(A) | P(W) | Units(kWh) | Cost(Rs)");
  lastTime     = millis();
  displayTimer = millis();
}

void loop() {
  voltage = readVoltageRMS();
  current = readCurrentRMS();
  power   = voltage * current;

  unsigned long now  = millis();
  float elapsedHours = (now - lastTime) / 3600000.0;
  lastTime           = now;
  energyKWh         += (power * elapsedHours) / 1000.0;
  units              = energyKWh;
  cost               = units * TARIFF_PER_KWH;

  Serial.print(voltage, 1);   Serial.print(" V | ");
  Serial.print(current, 3);   Serial.print(" A | ");
  Serial.print(power, 1);     Serial.print(" W | ");
  Serial.print(units, 5);     Serial.print(" Units | Rs.");
  Serial.println(cost, 4);

  if (millis() - displayTimer >= 3000) {
    displayTimer = millis();
    displayPage  = (displayPage + 1) % 3;
    lcd.clear();
    delay(50);
  }

  switch (displayPage) {
    case 0: showPage0(); break;
    case 1: showPage1(); break;
    case 2: showPage2(); break;
  }

  delay(500);
}
