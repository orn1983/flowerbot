#include <DHT.h>

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
  // initialize LEDs.
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  // initialize 5v for sensors
  pinMode(SENSORPOWER, OUTPUT);
  // initialize analog input
  pinMode(A0, INPUT);

  // initialize sensor
  dht.begin();
  // Serial debug output
  Serial.begin(9600);
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

void lightLED(int ledstate) {
  /* LEDs start on PIN 11 and end on 13.
     We will use ledstate 0 for off, 1 for first LED, 2 for second LED and 3 for third.
     To control them, we only need to add 10.
  */
  ledstate += 10;
  Serial.print("Ledstate is ");
  Serial.println(ledstate);
  for (int i=11; i <= 13; i++) {
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

// the loop function runs over and over again forever
void loop() {
  // initialize the variables we need
  static struct airData ad;
  static int sd;
  static int ledstate = 0;

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
    lightLED(1);
  else if (sd >= 500)
    lightLED(2);
  else
    lightLED(3);

  delay(2000);
}
