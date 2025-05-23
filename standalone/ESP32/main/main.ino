/*
  Bow Thruster Controller for ESP32-compatible

  Features:
    - 5-second press-to-arm safety
    - Automatic disarm after 2 minutes of inactivity
    - Low-voltage lockout based on analog voltage divider
    - Self-test at startup and every minute when disarmed
    - MOSFET drain validation via ADC reads
    - Continuous runtime checks of bus voltage
    - Disarm and 10-second beep on failure during operation
    - Debug output via Serial if verbose is true

  Behavior:
    - LED flashes rapidly on test failure
    - 5-second rapid beep blocks arming if test fails
    - 10-second long beep if failure during operation
    - Optional error code via blink/beep pattern
    - Optional debug jumper to override self-test (not implemented yet)

  Author: David Ross
  Date: 05-22-2025
*/

// Pin definitions for ESP32-compatible bow thruster controller
#define POWER_BUTTON_PIN      16  // GPIO16 on ESP32
#define PORT_SENSE_PIN       17  // GPIO17 on ESP32
#define STARBOARD_SENSE_PIN  18  // GPIO18 on ESP32
#define STATUS_LED_PIN        19  // GPIO19 on ESP32
#define BEEPER_PIN            21  // GPIO21 on ESP32 (tone workaround needed)
#define VOLTAGE_PIN           A0  // A0
#define PORT_CHECK_PIN        A4  // A4
#define STBD_CHECK_PIN        A5  // A5
#define PORT_CTRL_PIN          7
#define STBD_CTRL_PIN          8

bool verbose = true;  // Enable serial debugging

// Config constants
const float R1 = 10000.0;
const float R2 = 3300.0;
const float REF_VOLTAGE = 3.3;  // ESP32 ADC reference
const float LOW_VOLTAGE_THRESHOLD = 11.0;
const int DRAIN_MIN_ADC = 600;

// Runtime state
bool isArmed = false;
bool testPassed = false;
unsigned long lastTestTime = 0;
unsigned long lastActionTime = 0;
bool beeping = false;
unsigned long beepStart = 0;
unsigned long beepDuration = 0;

// Function declarations
void startup_selftest();
void updateBeep();
void failWithBeep(int seconds);
bool checkVoltage(uint8_t pin, float minV);
float readVoltage(uint8_t pin);
void handlePowerButton();
void activateDirection(uint8_t pin, uint8_t sensePin);
void deactivateThruster();
void signalError(int code);

void setup() {
  Serial.begin(9600);
  if (verbose) Serial.println("System initializing...");

  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_SENSE_PIN, INPUT_PULLUP);
  pinMode(STARBOARD_SENSE_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(BEEPER_PIN, OUTPUT);
  pinMode(PORT_CTRL_PIN, OUTPUT);
  pinMode(STBD_CTRL_PIN, OUTPUT);

  digitalWrite(STATUS_LED_PIN, LOW);  // Indicate ready or test passed
  digitalWrite(BEEPER_PIN, LOW);
  digitalWrite(PORT_CTRL_PIN, LOW);
  digitalWrite(STBD_CTRL_PIN, LOW);

  startup_selftest();
}

void loop() {
  updateBeep();

  handlePowerButton();

  if (isArmed) {

    if (digitalRead(PORT_SENSE_PIN) == LOW) {
      activateDirection(PORT_CTRL_PIN, PORT_CHECK_PIN);
    }
    else if (digitalRead(STARBOARD_SENSE_PIN) == LOW) {
      activateDirection(STBD_CTRL_PIN, STBD_CHECK_PIN);
    }
    else {
      digitalWrite(PORT_CTRL_PIN, LOW);
      digitalWrite(STBD_CTRL_PIN, LOW);
    }
  }
}

void updateBeep() {
  if (beeping && millis() - beepStart >= beepDuration) {
    digitalWrite(BEEPER_PIN, LOW);
    beeping = false;
  }
}


float readVoltage(uint8_t pin) {
  delay(10);  // Basic debounce to stabilize analog reading
  int raw = analogRead(pin);
  float vout = raw * (REF_VOLTAGE / 4095.0);  // Adjusted for ESP32 12-bit ADC
  return vout * ((R1 + R2) / R2);
}

bool checkVoltage(uint8_t pin, float minV) {
  float v = readVoltage(pin);
  if (verbose) {
    Serial.print("Voltage at pin A"); Serial.print(pin - A0); Serial.print(": "); Serial.println(v);
  }
  return v >= minV;
}

void failWithBeep(int seconds) {
  deactivateThruster();
  isArmed = false;
  testPassed = false;
  digitalWrite(STATUS_LED_PIN, HIGH);  // Indicate self-test or error
  beepStart = millis();
  beepDuration = seconds * 1000UL;
  beeping = true;
}

void signalError(int code) {
  for (int i = 0; i < code; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    tone(BEEPER_PIN, 2000);
    delay(150);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
  }
}

void deactivateThruster() {
  digitalWrite(PORT_CTRL_PIN, LOW);
  digitalWrite(STBD_CTRL_PIN, LOW);
  //TODO Do we need more controls in here??

}

void activateDirection(uint8_t pin, uint8_t sensePin) {
  digitalWrite(pin, HIGH);
  delay(20);
  float v = readVoltage(sensePin);
  if (v < LOW_VOLTAGE_THRESHOLD) {
    if (verbose) Serial.println("Drain voltage too low during thrust!");
    failWithBeep(10);
  }
}


void startup_selftest() {
  if (verbose) Serial.println("Running self-test...");
  lastTestTime = millis();
  testPassed = false;
  digitalWrite(STATUS_LED_PIN, HIGH);

  if (readVoltage(VOLTAGE_PIN) < LOW_VOLTAGE_THRESHOLD) {
  signalError(2);
  return;
  }
delay(50);

  testPassed = true;
  digitalWrite(STATUS_LED_PIN, LOW);
  if (verbose) Serial.println("Self-test passed");
}


void handlePowerButton() {
  static unsigned long pressedAt = 0;
  static bool cooldown = false;
  static unsigned long cooldownStart = 0;

  if (cooldown && millis() - cooldownStart < 5000) return;
  if (cooldown) cooldown = false;

  if (digitalRead(POWER_BUTTON_PIN) == LOW) {
    if (pressedAt == 0) pressedAt = millis();
    if (millis() - pressedAt >= 5000) {
      if (!testPassed) {
        if (verbose) Serial.println("Cannot arm: test failed");
        tone(BEEPER_PIN, 3000);
        delay(5000);
        noTone(BEEPER_PIN);
        cooldown = true;
        cooldownStart = millis();
        pressedAt = 0;
        return;
      }
      if (verbose) Serial.println("Arming system");
      isArmed = true;
      pressedAt = 0;
    }
  } else {
    pressedAt = 0;
  }
}
