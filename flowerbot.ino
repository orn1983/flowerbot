#include <DHT.h>

#define DHTPIN 9
#define DHTTYPE DHT11
#define SENSORPOWER 8
#define SOILREAD A0
#define FLOATREAD 10 
#define WATERLED A5
#define PUMP 11
#define MINUTE 60000 // A minute is 60,000 milliseconds

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
  // initialize analog input
  pinMode(SOILREAD, INPUT);
  // initialize dht sensor
  dht.begin();
  pinMode(FLOATREAD, INPUT_PULLUP);
  // Write pump pin to low, just in case
  digitalWrite(PUMP, LOW);
  // Serial debug output
  Serial.begin(9600);
	delay(2000);
  Serial.println("Board initialized");
	delay(10000);
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
  if (sd == 0)
    Serial.println("DRY");
  else if (sd == 1)
    Serial.println("MOIST");
  else
    Serial.println("WET");
}

bool checkWater() {
  return (digitalRead(FLOATREAD) != HIGH);
}

bool timeToAct(unsigned long lastAction, unsigned long currentTime, unsigned long interval) {
  // Function to calculate whether or not it is time to act based on state of
  // millisecond timers. We need this because millis() rolls over every ~50 days, and this
  // handles that safely.
	return (currentTime - lastAction >= interval);
}

void pumpWater(unsigned long milliseconds) {
  // TODO: write busy on LCD
  bool led_state = false;
  unsigned long last_mod = 0;
  unsigned long current_mod;
  unsigned long startloop = millis();
  digitalWrite(PUMP, HIGH);
  Serial.print("Starting pump at ");
  Serial.print(millis());
  Serial.print(" until ");
  Serial.println(startloop+milliseconds);
  // Blink WATERLED roughly every 100ms until we are done pumping
  while (millis() - startloop < milliseconds) {
    Serial.print(millis()-startloop);
    Serial.print(" < ");
    Serial.println(milliseconds);
    current_mod = millis() % 100;
    if (current_mod < last_mod)
    {
      led_state = !led_state;
      digitalWrite(WATERLED, led_state);
      Serial.print("Setting water led to ");
      Serial.println(led_state);
    }
    last_mod = current_mod;
  }
  Serial.println("Stopping pump");
  digitalWrite(PUMP, LOW);
  digitalWrite(WATERLED, LOW);
}

void loop() {
  // Initialize the variables we need
  static unsigned long waterInterval = 2 * 60 * MINUTE;
  static unsigned long probeInterval = 5 * MINUTE;
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
      Serial.println("Water supply empty");
      // Write on LCD that water supply is empty
    } else {
      digitalWrite(WATERLED, LOW);
      Serial.println("Water supply OK");
    }
    if (timeToAct(lastWater, currentTime, waterInterval)) {
      if (sd == 0) {
        Serial.println("Need to water. Checking supply");
        // Check water supply again, just in case
        waterEmpty = checkWater();
        if (!waterEmpty) {
          Serial.println("Supply OK to water. Turning on pump.");
          pumpWater(2000);
          lastWater = millis();
        }
      }
      else if (sd == 1) {
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
      Serial.print("I watered at ");
      Serial.print(lastWater);
      Serial.print(" and it is now ");
      Serial.println(currentTime);
    }
  }
}
