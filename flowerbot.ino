#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define SENSORPOWER 8
#define SOILREAD A0
#define FLOATREAD 7
#define RED 11
#define YELLOW 12
#define GREEN 13

DHT dht(DHTPIN, DHTTYPE);

struct airData {
  float humidity;
  float temperature;
  float eff_temperature;
};

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize LEDs.
  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  // initialize 5v for sensors
  pinMode(SENSORPOWER, OUTPUT);
  // initialize analog input
  pinMode(SOILREAD, INPUT);
  // initialize dht sensor
  dht.begin();
  pinMode(FLOATREAD, INPUT_PULLUP);
  // Serial debug output
  Serial.begin(9600);
}

int readSoil() {
   // Turn on the soil humidity reader
  int soil_humidity = analogRead(SOILREAD);
  return soil_humidity; 
}

struct airData readAirData() {
  struct airData d;
  d.humidity = dht.readHumidity();
  d.temperature = dht.readTemperature();
  // Effective temperature is provided by computeHeadIndex. false means we want celsius.
  d.eff_temperature = dht.computeHeatIndex(d.temperature, d.humidity, false);
  return d;
}

void printAirData(struct airData ad) {
  Serial.print("Air humidity: ");
  Serial.print(ad.humidity);
  Serial.print("%\t Air temperature: ");
  Serial.print(ad.temperature);
  Serial.print("%\t Effective air temperature: ");
  Serial.println(ad.eff_temperature);
}

void printSoilData(int sd) {
  Serial.print("Soil humidity: ");
  Serial.println(sd);
}

void lightLED(int ledstate) {
  /* To turn off all the LEDs, we just have to use a value outside of 11-13 */
  for (int i=RED; i <= GREEN; i++) {
    if (i == ledstate) {
      Serial.print("Setting to high pin ");
      Serial.println(i);
      digitalWrite(i, HIGH);
    }
    else {
      Serial.print("Setting to low pin ");
      Serial.println(i);
      digitalWrite(i, LOW);
    }
  }
}

boolean needWater() {
  return (digitalRead(FLOATREAD) != HIGH);
}

void loop() {
  // Initialize the variables we need
  static struct airData ad;
  static int sd;
  static int ledstate = 0;
  static int floatStatus;

  // Let's take a reading. Start by turning on sensors and letting them settle
  digitalWrite(SENSORPOWER, HIGH);
  delay(3000);  
  // Read the data
  ad = readAirData();
  sd = readSoil();
  // Done reading. Let's turn off our sensors and print the data
  digitalWrite(SENSORPOWER, LOW);
  printAirData(ad);
  printSoilData(sd);
  if (sd >= 800)
    lightLED(RED);
  else if (sd >= 500)
    lightLED(YELLOW);
  else
    lightLED(GREEN);

  if (needWater())
    Serial.println("Need water");
  else
    Serial.println("Have water"); 
  delay(600000);
}
