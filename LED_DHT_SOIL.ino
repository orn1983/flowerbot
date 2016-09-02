#include <DHT.h>

#define LED1 13
#define LED2 12
#define LED3 11
#define DHTPIN 2
#define DHTTYPE DHT11
#define SENSORPOWER 8
#define SOILREAD A0

DHT dht(DHTPIN, DHTTYPE);

struct airData {
  float humidity;
  float temperature;
  float eff_temperature;
};

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize outputs.
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(SENSORPOWER, OUTPUT);
  pinMode(A0, INPUT);

  // initialize sensor
  Serial.begin(9600);
  dht.begin();
}

int readSoil() {
   // Turn on the soil humidity reader
  int soil_humidity = analogRead(A0);
  return soil_humidity; 
}

struct airData readAirData() {
  struct airData d;
  d.humidity = dht.readHumidity();
  d.temperature = dht.readTemperature();
  // effective temperature is provided by computeHeadIndex. false means we want celsius.
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

// the loop function runs over and over again forever
void loop() {
  // Let's take a reading. Start by turning on sensors and letting them settle
  digitalWrite(SENSORPOWER, HIGH);
  delay(3000);  
  // Read the data
  struct airData ad = readAirData();
  int sd = readSoil();
  // Done reading. Let's turn off our sensors and print the data
  digitalWrite(SENSORPOWER, LOW);
  printAirData(ad);
  printSoilData(sd);
  delay(5000);
}
