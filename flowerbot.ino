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
#define BACKLIGHT_TIMEOUT 30000
#define PROBING_INTERVAL 60000
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
  bool mode;
  bool minus;
  bool plus;
};

struct modeDefinition {
  float pot_size;
  int humidity;
  int dry_hours;
};

// We use a global variable to represent the backlight for quicker tests
// i.e. so we don't have to read the pins
bool waterled_on = false;
bool backlight_on = true;
unsigned long backlight_expiry;

// Flowerbot settings
int running_mode = 0;

struct modeDefinition mode_settings[5] = {
  { 2, 0, 6 },
  { 2, 0, 24 },
  { 2, 0, 1 },
  { 2, 1, 24 },
  { 2, 2, 72 }
};

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

  // Print bootup message
  setBacklightState(HIGH);
  lcd.begin(16,2);
  //lcd.print("Flowerbot v0.1");
  lcdPrint(0, 0, 16, "Flowerbot v0.2");
  lcdPrint(0, 1, 16, "Starting...");

  // Wait to allow people to read bootup message
  delay(2000);
  lcd.clear();
  setBacklightExpiry();
  struct buttonData bd = readButtonData();

  // Skip loading from EEPROM if mode and plus are depressed while starting.
  // This can function as a factory reset, should the user enter configuration
  if (!bd.mode && !bd.plus) {
    // Load user settings from EEPROM
    EEPROM.get(RUNNING_MODE_ADDR, running_mode);
    EEPROM.get(MODE_SETTINGS_ADDR, mode_settings);
  }
  else {
    lcdPrint(0, 0, 16, "Loading defaults");
    lcdPrint(0, 1, 16, "Config. to save");
    delay(5000);
  }
}

char *soilDataToString(int soil_state) {
  if (soil_state == 0)
    return "Dry";
  else if (soil_state == 1)
    return "Damp";
  else if (soil_state == 2)
    return "Wet";
  else
    return "????";
}

void updatePotSize(int change) {
  // Update the pot size for the current setting. change=1 == inc, -1 == dec
  float *pot_size = &(mode_settings[running_mode].pot_size);
  float delta;
  if (*pot_size < 2.0)
    delta = change * 0.1;
  // We can't use == 2.0 (it never yields true), so we use <2.5 instead
  else if (*pot_size < 2.50) {
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

int litersToMilliseconds(float liters) {
  // Rough first estimation. Needs work.
  return (int)(liters * 2500);
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
  setBacklightState(HIGH);
  // Give user time to abort!
  delay(2000);
  digitalWrite(PUMP, HIGH);
  blinkWaterLED(milliseconds);
  digitalWrite(PUMP, LOW);
  clearLCD();
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
    setWaterLedState(HIGH);
    setBacklightState(HIGH);
    // This should be run often enough for the backlight never to turn off
    setBacklightExpiry();
    strncpy(message, "NO WATER", 9);
    // Write on LCD that water supply is empty
    // Length of field is 11 because that's the size of the field that we
    // are overwriting
    lcdPrint(0, 0, 11, message);
  }
  else if (waterled_on)
  {
    setWaterLedState(LOW);
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

void clearLCD() {
  lcd.clear();
}

void notifySettings(bool configuration_active) {
  char settings[12];
  // Arduino's snprintf does not support float formatting, so we
  // need to split the number up into two parts
  float pot_size = mode_settings[running_mode].pot_size;
  // Add 0.01 to deal with float inaccuracies and make sure we're always above the int
  int left_side = (int)(pot_size+0.01);
  // We know that 0.1 is the maximum degree of accuracy, so we just multiply by 10
  int right_side = (int)round((pot_size - left_side) * 10);
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

void setWaterLedState(bool state) {
  digitalWrite(WATERLED, state);
  waterled_on = (state == HIGH);
}

void setBacklightState(bool state) {
  digitalWrite(BACKLIGHT, state);
  backlight_on = (state == HIGH);
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
  // Set last_soil_state to invalid value to indicate startup
  static int last_soil_state = 3;

  // Initialize the last_watering and last_probing variables in the past to make sure 
  // that we start out by probing and watering, should conditions dictate.
  static unsigned long last_watering = 0 - (mode_settings[running_mode].dry_hours * HOUR) + (10*SECOND);
  static unsigned long last_timer_notify = 0 - SECOND;
  static unsigned long last_waterlevel_notify = 0 - (10 * SECOND);
  static unsigned long last_probing = 0 - PROBING_INTERVAL;
  // soil_state_change is the timestamp of when the soil state changed states
  static unsigned long soil_state_change = 0 - (mode_settings[running_mode].dry_hours * HOUR) + (10*SECOND);
  // Text description of soil state
  static char soil_state_desc[5];
  static struct airData air_data;
  static struct buttonData buttons;
  static int soil_state;
  static bool water_empty = true;

  // Read the data
  if (timeToAct(last_probing, PROBING_INTERVAL)) {
    notifyBusy("Reading sensors...");
    last_probing = millis();
    air_data = readAirData();
    soil_state = readSoil();
    // last_soil_state == 3 indicates a startup, so no update to make sure to water
    // if the soil needs it, as we don't know the last time it happened.
    if (last_soil_state != soil_state && last_soil_state != 3)
      soil_state_change = millis();
    last_soil_state = soil_state;
    water_empty = waterIsEmpty();
    clearLCD();
    if (soil_state <= mode_settings[running_mode].humidity && timeToAct(soil_state_change, mode_settings[running_mode].dry_hours * HOUR)) {
      if (!water_empty) {
        pumpWater(litersToMilliseconds(mode_settings[running_mode].pot_size));
        last_watering = millis();
        // Special case for plants that should always be wet, as there will no state
        // change from wet->wet
        if (mode_settings[running_mode].humidity == 2 && soil_state == 2)
          soil_state_change = last_watering;
      }
    }
    strncpy(soil_state_desc, soilDataToString(soil_state), 5);
    notifySoilState(soil_state_desc);
    notifyAirState(air_data);
    notifySettings(false);
    notifyEmptyWater(water_empty);
  }
  
  buttons = readButtonData();
  if (buttons.mode || buttons.minus || buttons.plus) {
    setBacklightState(HIGH);
    if (buttons.mode) {
      enterConfigureMode();
      clearLCD();
      notifySettings(false);
      notifyLastWateringTime(last_watering, millis());
      notifyAirState(air_data);
      notifySoilState(soil_state_desc);
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
    setBacklightState(LOW);
  }
}
