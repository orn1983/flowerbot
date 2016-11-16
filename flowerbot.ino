#include <DHT.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

#define DHTPIN 9
#define DHTTYPE DHT11
#define SENSORPOWER 8
#define SOILREAD A0
#define FLOATREAD 10 
#define WATERLED A5
#define PUMP 11
#define BACKLIGHT 12
#define PLUSBTN 15
#define MINUSBTN 16
#define MODEBTN 17
#define SECOND 1000
#define MINUTE 60000
#define HOUR 3600000
#define BTNDELAY 300
#define DEGREESYMBOL (char)223
#define BACKLIGHT_TIMEOUT 10000
#define RUNNING_MODE_ADDR 0
#define MODE_SETTINGS_ADDR 2

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

struct airData {
  float humidity;
  float temperature;
  float eff_temperature;
};

struct buttonData {
  bool plus;
  bool minus;
  bool mode;
};

struct modeDefinition {
  float pot_size;
  int humidity;
  int dry_hours;
};

// We use a global variable to represent the backlight for quicker tests
bool waterled_on = false;
bool backlight_on = true;
unsigned long backlight_expiry;

// Flowerbot settings
int running_mode = 0;

struct modeDefinition mode_settings[5] = {
  { 1, 1, 1 },
  { 1, 2, 10 },
  { 1, 2, 3 },
  { 1, 0, 3 },
  { 1, 1, 5 },
};

// 3.36 from 5.085
// the setup function runs once when you press reset or power the board
void setup() {
  // initialize LEDs.
  pinMode(WATERLED, OUTPUT);
  pinMode(BACKLIGHT, OUTPUT);
  // initialize 5v for sensors
  pinMode(SENSORPOWER, OUTPUT);
  // initialize 5v for pump
  pinMode(PUMP, OUTPUT);
  // initialize analog input
  pinMode(SOILREAD, INPUT);
  // Initialize button pins
  pinMode(PLUSBTN, INPUT);
  pinMode(MINUSBTN, INPUT);
  pinMode(MODEBTN, INPUT);
  // initialize dht sensor
  dht.begin();
  pinMode(FLOATREAD, INPUT_PULLUP);
  // Write pump pin to low, just in case
  digitalWrite(PUMP, LOW);
  // Serial debug output
  Serial.begin(9600);

  // Print bootup message
  turnBacklightOn();
  lcd.begin(16,2);
  lcd.print("Flowerbot v0.1");

  // Make sure we don't start pumping JUST as the board is plugged in.
  delay(2000);
  lcd.clear();
  Serial.println("Board initialized");
//  delay(8000);
  setBacklightExpiry();
  // Load user settings from EEPROM
  EEPROM.get(RUNNING_MODE_ADDR, running_mode);
  EEPROM.get(MODE_SETTINGS_ADDR, mode_settings);
}

char *soilDataToString(int soil_data) {
  if (soil_data == 0)
    return "Dry";
  else if (soil_data == 1)
    return "Damp";
  else if (soil_data == 2)
    return "Wet";
  else
    return "????";
}

void updatePotSize(int change) {
  // Update the pot size for the current setting. 1 = increase, -1 = decrease

  float *pot_size = &(mode_settings[running_mode].pot_size);
  float delta;
  if (*pot_size < 2.0)
    delta = change * 0.1;
  else if (*pot_size == 2.0) {
    Serial.println("Changing from 2.0");
    if (change == -1)
      delta = -0.1;
    else
      delta = 0.5;
  }
  else if (*pot_size < 5)
    delta = change * 0.5;
  else
    delta = change * 1;

  *pot_size = constrain(*pot_size + delta, 0.1, 20);
}

int readSoil() {
  // Read soil moisture from the soil sensor.
  // Returns 0 for dry, 1 for damp and 2 for wet
  static int soil_humidity;
  // Turn on the sensor for soil moisture
  digitalWrite(SENSORPOWER, HIGH);
  // Let it settle
  delay(500);  
  // Take the reading
  soil_humidity = analogRead(SOILREAD);
  // Turn it off
  Serial.print("Soil humidity: ");
  Serial.println(soil_humidity);
  digitalWrite(SENSORPOWER, LOW);
  if (soil_humidity >= 700)
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

struct buttonData readButtonData() {
  struct buttonData buttons;
  buttons.plus = digitalRead(PLUSBTN);
  buttons.minus = digitalRead(MINUSBTN);
  buttons.mode = digitalRead(MODEBTN);
  return buttons;
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
    Serial.println("DAMP");
  else
    Serial.println("WET");
}

bool waterIsEmpty() {
  // Returns true if float switch is open
  return (digitalRead(FLOATREAD) == HIGH);
}

bool timeToAct(unsigned long last_action, unsigned long interval) {
  // Function to calculate whether or not it is time to act based on state of
  // millisecond timers. We need this because millis() rolls over every ~50 days, and this
  // handles that safely.
  // Returns true if last_action+interval is in the past 
  return (millis() - last_action >= interval);
}

void pumpWater(unsigned long milliseconds) {
  // Turn on the water pump for 'milliseconds' milliseconds. Also blink the red LED
  // while the pump is active, to give some sort of visual confirmation of what is
  // going on.
  notifyBusy("Watering...");
  turnBacklightOn();
  // Give user time to abort!
  delay(2000);
  Serial.print("Starting pump for ");
  Serial.print(milliseconds);
  Serial.println(" milliseconds");
  digitalWrite(PUMP, HIGH);
  blinkWaterLED(milliseconds);
  digitalWrite(PUMP, LOW);
  Serial.println("Stopping pump");
  clearBusy();
  setBacklightExpiry();
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
    current_mod = millis() % 100;
    if (current_mod < last_mod) {
      led_state = !led_state;
      digitalWrite(WATERLED, led_state);
    }
    last_mod = current_mod;
  }
  // Make sure we turn off the LED before returning
  digitalWrite(WATERLED, LOW);
}

void notifyEmptyWater(bool state) {
  // Notify user that the water reservoir is empty
  // state variable true to notify user, or false to remove notification
  char message[9];
  if (state == true) {
    turnWaterLedOn();
    turnBacklightOn();
    setBacklightExpiry();
    strncpy(message, "NO WATER", 9);
    // Write on LCD that water supply is empty
    // Length of field is 11 because that's the size of the field that we
    // are overwriting
    lcdPrint(0, 0, 11, message);
  }
  else if (waterled_on)
  {
    turnWaterLedOff();
    setBacklightExpiry();
    notifySettings(false);
  }
}

void notifyAirState(struct airData ad) {
  char message[9];
  snprintf(message, 9, "%2d%% %2d%cC", (int)ad.humidity, (int)ad.temperature, DEGREESYMBOL);
  lcdPrint(0, 1, 8, message);
}

void notifyBusy(char reason[]) {
  lcd.clear();
  lcd.print("Please wait...");
  lcd.setCursor(0, 1);
  lcd.print(reason);
}

void clearBusy() {
  lcd.clear();
}

void notifySettings(bool configuration_active) {
  char settings[12];
  // Arduino's snprintf does not support float formatting, so we
  // need to split the number up into two parts
  float pot_size = mode_settings[running_mode].pot_size;
  int left_side = (int)pot_size;
  // We know that 0.1 is the maximum degree of accuracy, so we just
  // multiply by 10
  int right_side = (pot_size - left_side) * 10;

  if (right_side == 0)
    snprintf(settings, 12, "Mode %d %2d L", running_mode + 1, left_side);
  else
    snprintf(settings, 12, "Mode %d %d.%dL", running_mode + 1, left_side, right_side);

  lcdPrint(0, 0, 11, settings);

  if (configuration_active) {
    char description[17];
    char humidity[5];
    int dry_hours = mode_settings[running_mode].dry_hours;
    // Copy string to variable in order to make it lowercase
    strncpy(humidity, soilDataToString(mode_settings[running_mode].humidity), 5);
    humidity[0] = tolower(humidity[0]);
    snprintf(description, 17, "%dh after %s", dry_hours, humidity);
    lcdPrint(0, 1, 16, description);
    free(humidity);
  }
}

void notifySoilState(char state[]) {
  lcdPrint(12, 0, 4, state);
}

void notifyLastWateringTime(unsigned long last_watering_time, unsigned long current_time) {
  unsigned long delta_seconds = (current_time - last_watering_time) / 1000;
  char time[8];
  secondsToHHMM(delta_seconds, time);
  // Display the time in the appropriate LCD position, offset from the right (pos. 16)
  lcdPrint(16-(strlen(time)), 1, 7, time);
}

void secondsToHHMM(unsigned long seconds, char* time) {
  int minutes = (seconds / 60) % 60;
  int hours = (seconds / 3600) % 24;
  int days = (seconds / 86400);
  if (days <= 0)
    snprintf(time, 6, "%02d:%02d", hours, minutes);
  else 
    snprintf(time, 8, "%d:%02d:%02d", days, hours, minutes);
}

void lcdPrint(int col_start, int row_start, int field_length, char text[]) {
  lcd.setCursor(col_start, row_start);
  int text_length = strlen(text);
  char formatted_text [field_length+1];
  strncpy(formatted_text, text, field_length);
  // Fill the remaining slots with spaces
  for (int i = text_length; i <= field_length; i++) {
    formatted_text[i] = ' '; 
  }
  formatted_text[field_length] = '\0';
  lcd.setCursor(col_start, row_start);
  lcd.print(formatted_text);
}

bool backlightExpired() {
  return (timeToAct(backlight_expiry, BACKLIGHT_TIMEOUT));
}

void turnWaterLedOn() {
  digitalWrite(WATERLED, HIGH);
  waterled_on = true;
}

void turnWaterLedOff() {
  digitalWrite(WATERLED, LOW);
  waterled_on = false;
}

void turnBacklightOn() {
  digitalWrite(BACKLIGHT, HIGH);
  backlight_on = true;
}

void turnBacklightOff() {
  digitalWrite(BACKLIGHT, LOW);
  backlight_on = false;
}

void setBacklightExpiry() {
  backlight_expiry = millis();
}

void enterConfigureMode() {
  unsigned long last_button_read = millis();
  struct buttonData buttons;
  int configuration_timeout = 15 * SECOND;
  unsigned long configuration_expiry = configuration_timeout + millis();
  notifySettings(true);

  while (millis() <= configuration_expiry) {
    if (timeToAct(last_button_read, BTNDELAY)) {
      buttons = readButtonData();
      if (buttons.mode || buttons.minus || buttons.plus) {
        last_button_read = millis();
        configuration_expiry = last_button_read + configuration_timeout;

        if (buttons.mode) {
          running_mode += 1;
          running_mode = running_mode % 5;
        }
        else if (buttons.minus) {
          updatePotSize(-1);
        }
        else {
          updatePotSize(1);
        }
        // Update LCD
        notifySettings(true);
      }
    }
  }
  // Update EEPROM if applicable
  EEPROM.put(RUNNING_MODE_ADDR, running_mode);
  EEPROM.put(MODE_SETTINGS_ADDR, mode_settings);
}

void loop() {
  // Initialize the variables we need
  static unsigned long watering_interval = HOUR;
  static unsigned long probing_interval = 5 * MINUTE;
  // Initialize the last_watering and last_probing variables in the past to make sure we 
  // start out by probing and watering, if conditions dictate.
  static unsigned long last_watering = 0 - watering_interval;
  static unsigned long last_timer_notify = 0 - SECOND;
  static unsigned long last_waterlevel_notify = 0 - (10 * SECOND);
  static unsigned long last_probing = 0 - probing_interval;
  static char soil_state[5];
  static struct airData air_data;
  static struct buttonData buttons;
  static int soil_data;
  static int waterEmpty = true;

  // Read the data
  if (timeToAct(last_probing, probing_interval)) {
    Serial.println("Taking readings...");
    notifyBusy("Reading sensors...");
    last_probing = millis();
    air_data = readAirData();
    soil_data = readSoil();
    waterEmpty = waterIsEmpty();
    clearBusy();
    printAirData(air_data);
    printSoilData(soil_data);
    if (timeToAct(last_watering, watering_interval)) {
      if (soil_data == 0) {
        Serial.println("Need to water. Checking supply");
        // Check water supply again, just in case
        waterEmpty = waterIsEmpty();
        if (!waterEmpty) {
          Serial.println("Supply OK to water. Turning on pump.");
          // Turn on pump for 2 seconds. Magic number for base phase only. Will be user configurable.
          pumpWater(4000);
          // Update last watering time.
          last_watering = millis();
        }
      }
      else if (soil_data == 1) {
        Serial.println("Soil is damp. No need to water.");
      }
      else {
        Serial.println("Soil is very wet. No need to water.");
      }
      strncpy(soil_state, soilDataToString(soil_data), 5);
    }
    else {
      // Debug output -- will be removed. Maybe add time since last watering when
      // LCD is connected?
      Serial.println("I watered too recently. Waiting...");
      Serial.print("I watered at ");
      Serial.print(last_watering);
      Serial.print(" and it is now ");
      Serial.println(millis());
    }
    notifyAirState(air_data);
    notifySoilState(soil_state);
    notifySettings(false);
    notifyEmptyWater(waterEmpty);
  }
  
  buttons = readButtonData();
  if (buttons.mode || buttons.minus || buttons.plus) {
    turnBacklightOn();
    if (buttons.mode) {
      enterConfigureMode();
      clearBusy();
      notifySettings(false);
      notifyLastWateringTime(last_watering, millis());
      notifyAirState(air_data);
      notifySoilState(soil_state);
    }
    setBacklightExpiry();
  }

  if (timeToAct(last_timer_notify, SECOND)) {
    notifyLastWateringTime(last_watering, millis());
    last_timer_notify = millis();
  }

  if (timeToAct(last_waterlevel_notify, 5*SECOND)) {
    notifyEmptyWater(waterIsEmpty());
    last_waterlevel_notify = millis();
  }

  if (backlight_on && backlightExpired()) {
    turnBacklightOff();
  }
}
