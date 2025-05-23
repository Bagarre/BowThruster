/*
  Bow Thruster Controller for Arduino Metro Mini2 (ATmega328P)

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

  Author: David Ross (Metro Mini2 port)
  Original: 05-22-2025
*/

// Pin Definitions
#define POWER_BUTTON_PIN     2
#define STATUS_LED_PIN       3
#define PORT_BUTTON_PIN      4
#define STBD_BUTTON_PIN 5
#define BEEPER_PIN           6
#define PORT_CTRL_PIN      7
#define STBD_CTRL_PIN      8
#define BT_VOLTAGE_PIN      A0
#define PORT_VOLTAGE_PIN    A1
#define STBD_VOLTAGE_PIN    A2

// Debugging
bool verbose = true;

// Constants
const float R1 = 10000.0;
const float R2 = 4700.0;
const float REF_VOLTAGE = 5.0;
const float LOW_VOLTAGE_THRESHOLD = 11.0;
const float MAX_VOLTAGE_THRESHOLD = 15.0;
const float

const float MAX_THRUST_TIME = 5000.0;
const float LOCKOUT_TIME = 2000.0;
const float PORT_2_STBD_DELAY = 2000.0;

// System State
unsigned long lastActionTime = 0;
unsigned long powerButtonStartTime = 0;
unsigned long stbdPressTime = 0;
unsigned long portPressTime = 0;
unsigned long lastPortReleaseTime = 0;
unsigned long lastStbdReleaseTime = 0;
unsigned long lockOutTime = 0;

bool isArmed = false;
bool powerButtonHeld = false;
bool powerStatusFlipped = false;
bool portActive = false;
bool stbdActive = false;
bool portWasPressed = false;
bool stbdWasPressed = false;
bool voltageLow = false;

// Beeper State
bool beeping = false;
unsigned long beepStart = 0;
unsigned long beepDuration = 0;

void setup() {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STBD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_CTRL_PIN, OUTPUT);
  pinMode(STBD_CTRL_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(BEEPER_PIN, OUTPUT);

  digitalWrite(PORT_CTRL_PIN, LOW);
  digitalWrite(STBD_CTRL_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);
  digitalWrite(BEEPER_PIN, LOW);

  Serial.begin(9600);
}

void loop() {
  unsigned long now = millis();
  updateBeep();

  if (!checkVoltage(BT_VOLTAGE_PIN)) {
//TODO pass pin, over, under and handle alarm.
    deactivateThruster("Low Battery disarm");
    isArmed = false;
    delay(100);
  }

  handlePowerButton(now);
  handleAutoDisarm(now);

  if (isArmed) {
    handleThrustControl(now);
  }
}

void handlePowerButton(unsigned long now) {
  if (digitalRead(POWER_BUTTON_PIN) == LOW) {
    lastActionTime = now;
    if (!powerButtonHeld) {
      powerButtonStartTime = now;
      powerButtonHeld = true;
    }
    if (!isArmed && now - powerButtonStartTime >= 5000 && !powerStatusFlipped) {
      if (verbose) Serial.println("System armed");
      startBeep(500);
      isArmed = true;
      powerStatusFlipped = true;
      powerButtonHeld = false;
      analogWrite(STATUS_LED_PIN, 255);
    } else if (isArmed && now - powerButtonStartTime >= 2000 && !powerStatusFlipped) {
      if (verbose) Serial.println("System disarmed");
      deactivateThruster("manual disarm");
      startBeep(500);
      isArmed = false;
      powerStatusFlipped = true;
      powerButtonHeld = false;
      analogWrite(STATUS_LED_PIN, 0);
    } else if (!isArmed && !powerStatusFlipped) {
      flashStatusLED(now);
    }
  } else {
    powerButtonHeld = false;
    powerStatusFlipped = false;
    powerButtonStartTime = 0;
    analogWrite(STATUS_LED_PIN, isArmed ? 255 : 0);
  }
}

void handleAutoDisarm(unsigned long now) {
  if (isArmed && (now - lastActionTime > 120000)) {
    if (verbose) Serial.println("Auto-disarming after 2 minutes");
    isArmed = false;
    powerButtonHeld = false;
    powerButtonStartTime = 0;
    analogWrite(STATUS_LED_PIN, 0);
    deactivateThruster("auto disarm");
    startBeep(500);
  }
}

void handleThrustControl(unsigned long now) {
  bool portPressed = digitalRead(PORT_BUTTON_PIN) == LOW;
  bool stbdPressed = digitalRead(STBD_BUTTON_PIN) == LOW;
 //TODO check drains to make sure they go hi
 //TODO safety first

  if (portPressed && stbdPressed) {
    deactivateThruster("Both Port and Starboard buttons pressed");
    return;
  }

  if (now - lockOutTime < LOCKOUT_TIME) {
    deactivateThruster("Locked Out for CoolDown");
    portWasPressed = false;
    stbdWasPressed = false;
    return;
  }

  if ((portActive && now - portPressTime > MAX_THRUST_TIME) ||
      (stbdActive && now - stbdPressTime > MAX_THRUST_TIME)) {
    lockOutTime = now;
    deactivateThruster("Time Exceeded");
    return;
  }

  if (portPressed && !portWasPressed && now - lastStbdReleaseTime > PORT_2_STBD_DELAY) {
    if (verbose) Serial.println("Port button pressed");
    startBeep(50);
    portWasPressed = true;
    portPressTime = now;
    lastActionTime = now;
    activateDirection("port");
  } else if (!portPressed && portWasPressed) {
    if (verbose) Serial.println("Port button released");
    deactivateThruster("Port button released");
    portWasPressed = false;
    portPressTime = 0;
    lastActionTime = now;
    lastPortReleaseTime = now;
  }

  if (stbdPressed && !stbdWasPressed && now - lastPortReleaseTime > PORT_2_STBD_DELAY) {
    if (verbose) Serial.println("Starboard button pressed");
    startBeep(50);
    activateDirection("starboard");
    stbdWasPressed = true;
    stbdPressTime = now;
    lastActionTime = now;
  } else if (!stbdPressed && stbdWasPressed) {
    if (verbose) Serial.println("Starboard button released");
    deactivateThruster("Starboard button released");
    stbdWasPressed = false;
    stbdPressTime = 0;
    lastActionTime = now;
    lastStbdReleaseTime = now;
  }
}


void activateDirection(String dir) {
  //TODO Safety checks
  // Use A1/A2 to ensure opposite direction isn't active.
  // Once activated, ensure readings on A1/A2 are correct.
  // Call deactivateThruster() on conflict.
  if (verbose) {
    Serial.print("Activating direction: "); Serial.println(dir);
  }
  if (dir.equals("port")) {
    digitalWrite(STBD_CTRL_PIN, LOW);
    digitalWrite(PORT_CTRL_PIN, HIGH);
    portActive = true;
    stbdActive = false;
  } else if (dir.equals("starboard")) {
    digitalWrite(PORT_CTRL_PIN, LOW);
    digitalWrite(STBD_CTRL_PIN, HIGH);
    portActive = false;
    stbdActive = true;
  } else {
    deactivateThruster("Unknown direction: " + dir);
  }
}


void deactivateThruster(String source) {
  if (verbose) {
    Serial.print("Deactivating directions from: "); Serial.println(source);
    //TODO Verify via A1/A2 this is correct

    if (portActive) Serial.println("Port was active");
    if (stbdActive) Serial.println("Starboard was active");
  }
  digitalWrite(PORT_CTRL_PIN, LOW);
  digitalWrite(STBD_CTRL_PIN, LOW);
  //TODO Verify A1/A2 that both are LOW

  portActive = false;
  stbdActive = false;
}

void flashStatusLED(unsigned long now) {
  if (!isArmed) {
    int flash = (now / 500) % 2;
    analogWrite(STATUS_LED_PIN, flash ? 150 : 0);
  }
}

void updateBeep() {
  if (beeping && millis() - beepStart >= beepDuration) {
    digitalWrite(BEEPER_PIN, LOW);
    beeping = false;
  }
}

void startBeep(unsigned long duration) {
  if (verbose) {
    Serial.print("Beep for: "); Serial.println(duration);
  }
  digitalWrite(BEEPER_PIN, HIGH);
  beepStart = millis();
  beepDuration = duration;
  beeping = true;
}


bool checkVoltage(uint8_t pin){
// TODO This should be given the pin, over, under
// TODO return true/false if it's not in that range.
  unsigned long now = millis();
  int raw = analogRead(pin);
  float vout = raw * (REF_VOLTAGE / 1023.0);
  float vbat = vout * ((R1 + R2) / R2);

  voltageLow = (vbat < LOW_VOLTAGE_THRESHOLD);
  if (voltageLow) {
    if (verbose) Serial.println("Low voltage detected");
    int flash = (now / 200) % 2;
    digitalWrite(STATUS_LED_PIN, flash ? HIGH : LOW);
    return false;
  }
  return true;
}
