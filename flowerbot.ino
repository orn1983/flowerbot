#include <DHT.h>

#define DHTPIN 9
#define DHTTYPE DHT11
#define SENSORPOWER 8
#define SOILREAD A0
#define FLOATREAD 10 
#define WATERLED A5
#define PUMP 13
#define ACTUALPUMP 11

DHT dht(DHTPIN, DHTTYPE);

struct airData {
  float humidity;
  float temperature;
  float eff_temperature;
};

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize LEDs.
  pinMode(WATERLED, OUTPUT);
  // initialize 5v for sensors
  pinMode(SENSORPOWER, OUTPUT);
	// initialize 5v for pump
  pinMode(PUMP, OUTPUT);
	pinMode(ACTUALPUMP, OUTPUT);
  // initialize analog input
  pinMode(SOILREAD, INPUT);
  // initialize dht sensor
  dht.begin();
  pinMode(FLOATREAD, INPUT_PULLUP);
  // Serial debug output
  Serial.begin(9600);
}

int readSoil() {
  // Returns 0 for dry, 1 for moist and 2 for wet
  // Turn on the sensor for soil moisture
	static int soil_humidity;
  digitalWrite(SENSORPOWER, HIGH);
	// Let it settle
	delay(500);  
	// Take the reading
  soil_humidity = analogRead(SOILREAD);
	// Turn it off
  digitalWrite(SENSORPOWER, LOW);
  if (soil_humidity >= 800)
    return 0;
  else if (soil_humidity >= 500)
    return 1;
  else
    return 2;
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
  Serial.print("°C\t Effective air temperature: ");
  Serial.print(ad.eff_temperature);
  Serial.println("°C");
}

void printSoilData(int sd) {
  Serial.print("Soil humidity: ");
  Serial.println(sd);
}

bool checkWater() {
  return (digitalRead(FLOATREAD) != HIGH);
}

bool timeToAct(unsigned long lastAction, unsigned long currentTime, unsigned long interval) {
  Serial.print(currentTime);
  Serial.print(" - ");
  Serial.print(lastAction);
  Serial.print(" >= ");
  Serial.println(interval);
	return ((unsigned long)(currentTime - lastAction >= interval));
}

void loop() {
  // Initialize the variables we need
  static unsigned long waterInterval = 60000;
  static unsigned long probeInterval = 10000;
  // Initialize the lastWater and lastProbe variables in the past to make sure we start out by probing
  // and watering, if applicable
  static unsigned long lastWater = 0 - waterInterval;
  static unsigned long lastProbe = 0 - probeInterval;
  static unsigned long currentTime;
  static struct airData ad;
  static int sd;
  static int ledstate = 0;
  static int waterEmpty = true;

	// Get current time
	currentTime = millis();
  // Read the data
  if (timeToAct(lastProbe, currentTime, probeInterval)) {
    Serial.println("Taking readings...");
    ad = readAirData();
    sd = readSoil();
    waterEmpty = checkWater();
    printAirData(ad);
    printSoilData(sd);
    lastProbe = millis();
    if (waterEmpty) {
      digitalWrite(WATERLED, HIGH);
      // Skrifa á skjá að það vanti vatn
    } else {
      digitalWrite(WATERLED, LOW);
    }
    if (timeToAct(lastWater, currentTime, waterInterval)) {
      Serial.println("Checking if watering is needed...");
      if (sd >= 800) {
        // Check water supply again, just in case
        waterEmpty = checkWater();
        if (waterEmpty == false) {
          Serial.println("Need to water. Turning on pump.");
          digitalWrite(PUMP, HIGH);
    //			digitalWrite(ACTUALPUMP, HIGH);
          delay(6000);
          digitalWrite(PUMP, LOW);
    //			digitalWrite(ACTUALPUMP, LOW);
          lastWater = millis();
        }
      }
      else if (sd >= 500) {
        // Skrifa á skjá að jarðvegur sé millirakur
        Serial.println("Soil is moist. No need to water.");
      }
      else {
        // Skrifa á skjá að jarðvegur sé mjög rakur
        Serial.println("Soil is very wet. No need to water.");
      }
    }
    else {
      Serial.println("I watered too recently. Waiting...");
    }
  }
  delay(1000);
}
