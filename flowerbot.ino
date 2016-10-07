#include <DHT.h>

#define DHTPIN 9
#define DHTTYPE DHT11
#define SENSORPOWER 8
#define SOILREAD A0
#define FLOATREAD 10 
#define WATERLED A5
#define PUMP 11
#define MINUTE 60000 // A minute is 60,000 milliseconds
#define HOUR 3600000 // An hour is 3,600,000 milliseconds

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

  // Make sure we don't start pumping JUST as the board is plugged in.
	delay(2000);
  Serial.println("Board initialized");
	delay(8000);
}

int readSoil() {
  // Read soil moisture from the soil sensor.
  // Returns 0 for dry, 1 for moist and 2 for wet
	static int soil_humidity;
  // Turn on the sensor for soil moisture
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
	// Read data from air temperature/moisture sensor. Returns an airData struct.
	// Expects to have access to a globally scoped DHT object
  struct airData d;
  d.humidity = dht.readHumidity();
  d.temperature = dht.readTemperature();
  // Effective temperature is provided by computeHeadIndex. false means we want celsius.
  d.eff_temperature = dht.computeHeatIndex(d.temperature, d.humidity, false);
  return d;
}

void printAirData(struct airData ad) {
	// Print all information from air temperature/moisture sensor on Serial.
	// Needs rewrite after we connect LCD
  Serial.print("Air humidity: ");
  Serial.print(ad.humidity);
  Serial.print("%\t Air temperature: ");
  Serial.print(ad.temperature);
  Serial.print("°C\t Effective air temperature: ");
  Serial.print(ad.eff_temperature);
  Serial.println("°C");
}

void printSoilData(int sd) {
	// Print soil humidity on Serial. Soil data is received by function as an integer
	// integer values 0 = dry, 1 = moist, 2 = wet
  Serial.print("Soil humidity: ");
  if (sd == 0)
    Serial.println("DRY");
  else if (sd == 1)
    Serial.println("MOIST");
  else
    Serial.println("WET");
}

bool waterIsEmpty() {
  // Returns true if float switch is open
  return (digitalRead(FLOATREAD) != HIGH);
}

bool timeToAct(unsigned long last_action, unsigned long current_time, unsigned long interval) {
  // Function to calculate whether or not it is time to act based on state of
  // millisecond timers. We need this because millis() rolls over every ~50 days, and this
  // handles that safely.
  // If any time exceeding interval has passed between last_action and current_time, this function
  // will return true
	return (current_time - last_action >= interval);
}

void pumpWater(unsigned long milliseconds) {
  // Turn on the water pump for 'milliseconds' milliseconds. Also blink the red LED
  // while the pump is active, to give some sort of visual confirmation of what is
  // going on.
  // TODO: Replace Serial output with printing 'Watering...' on LCD when LCD is connected
  Serial.print("Starting pump for ");
  Serial.print(milliseconds);
  Serial.print(" milliseconds");
  digitalWrite(PUMP, HIGH);
  blinkWaterLED(milliseconds);
  digitalWrite(PUMP, LOW);
  Serial.println("Stopping pump");
}

void blinkWaterLED(unsigned long milliseconds) {
  // Helper function for pumpWater. Blink the red LED for 'milliseconds' milliseconds.
  // Return when timer expires.
  bool led_state = false;
  unsigned long last_mod = 0;
  unsigned long current_mod;
  unsigned long loop_start = millis();
  // Blink WATERLED roughly every 100ms until we are done pumping
  while (millis() - loop_start < milliseconds) {
    // Debug output -- will be completely removed
    Serial.print(millis()-loop_start);
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
  // Make sure we turn off the LED before returning
  digitalWrite(WATERLED, LOW);
}

void notifyEmptyWater(bool state) {
  // Notify user that the water reservoir is empty
  // state variable true to notify user, or false to remove notification
  if (state == true) {
    digitalWrite(WATERLED, HIGH);
    Serial.println("Water supply empty");
    // Write on LCD that water supply is empty
  } else {
    digitalWrite(WATERLED, LOW);
    Serial.println("Water supply OK");
    // Remove text about low water from LCD
  }
}

void loop() {
  // Initialize the variables we need
  static unsigned long watering_interval = 2 * HOUR;
  static unsigned long probing_interval = 5 * MINUTE;
  // Initialize the last_watering and last_probing variables in the past to make sure we start out by probing
  // and watering, if conditions dictate.
  static unsigned long last_watering = 0 - watering_interval;
  static unsigned long last_probing = 0 - probing_interval;
  static unsigned long current_time;
  static struct airData air_data;
  static int soil_data;
  static int waterEmpty = true;

	// Get current time
	current_time = millis();
  // Read the data
  if (timeToAct(last_probing, current_time, probing_interval)) {
    Serial.println("Taking readings...");
    air_data = readAirData();
    soil_data = readSoil();
    waterEmpty = waterIsEmpty();
    printAirData(air_data);
    printSoilData(soil_data);
    last_probing = millis();
    if (waterEmpty) {
      notifyEmptyWater(true);
    } else {
      notifyEmptyWater(false);
    }
    if (timeToAct(last_watering, current_time, watering_interval)) {
      if (soil_data == 0) {
        Serial.println("Need to water. Checking supply");
        // Check water supply again, just in case
        waterEmpty = waterIsEmpty();
        if (!waterEmpty) {
          Serial.println("Supply OK to water. Turning on pump.");
          // Turn on pump for 2 seconds. Magic number for base phase only. Will be user configurable.
          pumpWater(2000);
          // Update last watering time.
          last_watering = millis();
        }
      }
      else if (soil_data == 1) {
        // Skrifa á skjá að jarðvegur sé millirakur
        Serial.println("Soil is moist. No need to water.");
      }
      else {
        // Skrifa á skjá að jarðvegur sé mjög rakur
        Serial.println("Soil is very wet. No need to water.");
      }
    }
    else {
      // Debug output -- will be removed. Maybe add time since last watering when
      // LCD is connected?
      Serial.println("I watered too recently. Waiting...");
      Serial.print("I watered at ");
      Serial.print(last_watering);
      Serial.print(" and it is now ");
      Serial.println(current_time);
    }
  }
}
