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

#include "pattern_player.h"


// Pin Definitions
#define POWER_BUTTON_PIN     2 // Pull low to enable
#define STATUS_LED_PIN       3
#define PORT_BUTTON_PIN      4 // Pull low to enable
#define STBD_BUTTON_PIN      5 // Pull low to enabl
#define BEEPER_PIN           6
#define PORT_CTRL_PIN        7 //PNP Transistor HIGH is off
#define STBD_CTRL_PIN        8 //PNP Transistor HIGH is off
#define BT_VOLTAGE_PIN      A0
#define PORT_VOLTAGE_PIN    A1 // Reads collector voltage
#define STBD_VOLTAGE_PIN    A2 // Reads collector voltage

// Debugging
bool verbose = true;

// Constants
const float R1 = 10000.0;
const float R2 = 4700.0;
const float REF_VOLTAGE = 5.0;
const float LOW_VOLTAGE_THRESHOLD = 11.0;
const float MAX_VOLTAGE_THRESHOLD = 15.0;
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

PatternPlayer ledPlayer(STATUS_LED_PIN);
PatternPlayer beepPlayer(BEEPER_PIN);

void setup() {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STBD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_CTRL_PIN, OUTPUT);
  pinMode(STBD_CTRL_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(BEEPER_PIN, OUTPUT);

  digitalWrite(PORT_CTRL_PIN, HIGH); //Disable
  digitalWrite(STBD_CTRL_PIN, HIGH); //Disable
  digitalWrite(STATUS_LED_PIN, LOW);
  digitalWrite(BEEPER_PIN, LOW);

//TODO on power up ring buzzer for 2 seconds then a quick beep.

  Serial.begin(9600);

}

void loop() {
  unsigned long now = millis();

  ledPlayer.update();
  beepPlayer.update();

  beepPlayer.start("_", 500, false);
  Serial.println("System Power Up");


  if (!checkVoltage(BT_VOLTAGE_PIN, LOW_VOLTAGE_THRESHOLD, MAX_VOLTAGE_THRESHOLD)) {
//TODO checkVoltage (pin, over, under) return true false
// TODO Trigger LED fast flash
    ledPlayer.start("...", 250 );
    deactivateThruster("Low Battery disarm");
    isArmed = false;
    delay(100);
  }

  handlePowerButton(now);
  handleAutoDisarm(now);
  handleThrustControl(now);

}

void handlePowerButton(unsigned long now) {
  if (digitalRead(POWER_BUTTON_PIN) == LOW) {
    lastActionTime = now;
    if (!powerButtonHeld) {
      powerButtonStartTime = now;
      powerButtonHeld = true;
    }
    if (!isArmed && now - powerButtonStartTime >= 5000 && !powerStatusFlipped) {
      Serial.println("System armed");
      //TODO Safety checks that A1/A2 are LOW else fail and alarm
      beepPlayer.start("_", 100);
      isArmed = true;
      powerStatusFlipped = true;
      powerButtonHeld = false;
      analogWrite(STATUS_LED_PIN, 255);
    }
    else if (isArmed && now - powerButtonStartTime >= 2000 && !powerStatusFlipped) {
      Serial.println("System disarmed");
      deactivateThruster("manual disarm");
      beepPlayer.start( "_", 500);
      isArmed = false;
      powerStatusFlipped = true;
      powerButtonHeld = false;
      analogWrite(STATUS_LED_PIN, 0);
    }
//    else if (!isArmed && !powerStatusFlipped) {
//      flashStatusLED(now);
//    }
  }
  else {
    powerButtonHeld = false;
    powerStatusFlipped = false;
    powerButtonStartTime = 0;
    analogWrite(STATUS_LED_PIN, isArmed ? 255 : 0);
  }
}

void handleAutoDisarm(unsigned long now) {
  if (isArmed && (now - lastActionTime > 120000)) {
    Serial.println("Auto-disarming after 2 minutes");
    isArmed = false;
    powerButtonHeld = false;
    powerButtonStartTime = 0;
    analogWrite(STATUS_LED_PIN, 0);
    beepPlayer.start(".....", 200);
    deactivateThruster("auto disarm");
  }
}

void handleThrustControl(unsigned long now) {
  if (isArmed) {
    bool portPressed = digitalRead(PORT_BUTTON_PIN) == LOW;
    bool stbdPressed = digitalRead(STBD_BUTTON_PIN) == LOW;
   //TODO check drains to make sure they go hi
   //TODO safety first

    if (portPressed && stbdPressed) {
      deactivateThruster("Both Port and Starboard buttons pressed");
      beepPlayer.start("..", 50);
      return;
    }

    if (now - lockOutTime < LOCKOUT_TIME) {
      deactivateThruster("Locked Out for CoolDown");
      portWasPressed = false;
      stbdWasPressed = false;
      beepPlayer.start("..", 50);
      return;
    }

    if ((portActive && now - portPressTime > MAX_THRUST_TIME) ||
        (stbdActive && now - stbdPressTime > MAX_THRUST_TIME)) {
      lockOutTime = now;
      beepPlayer.start("..", 50);
      deactivateThruster("Time Exceeded");
      return;
    }

    if (portPressed && !portWasPressed && now - lastStbdReleaseTime > PORT_2_STBD_DELAY) {
      if (verbose) Serial.println("Port button pressed");
      beepPlayer.start(".", 50);
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
      beepPlayer.start(".", 50);
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
}


/*

//void Alert( string type) {

  Initial power up
  System Armed
  System Disarmed
  Thrust time exceeded
  Bus Low Voltage
  Bus Over Voltage
  Port/Stbd sense pin not what expected



//}
*/

void activateDirection(String dir) {
  //TODO Safety checks
  // Use A1/A2 to ensure opposite direction isn't active.
  // Once activated, ensure readings on A1/A2 are correct.
  // Call deactivateThruster() on conflict.
  if (verbose) {
    Serial.print("Activating direction: "); Serial.println(dir);
  }
  if (dir.equals("port")) {
    digitalWrite(STBD_CTRL_PIN, HIGH);
//    if (!checkVoltage(BT_VOLTAGE_PIN, LOW_VOLTAGE_THRESHOLD, MAX_VOLTAGE_THRESHOLD)) {
//TODO verify its off before turning the other on

    digitalWrite(PORT_CTRL_PIN, LOW);
    portActive = true;
    stbdActive = false;
  } else if (dir.equals("starboard")) {
    digitalWrite(PORT_CTRL_PIN, HIGH);
    digitalWrite(STBD_CTRL_PIN, LOW);
    portActive = false;
    stbdActive = true;
  } else {
    deactivateThruster("Unknown direction: " + dir);
    beepPlayer.start("..", 50);

  }
}


void deactivateThruster(String source) {
  if (verbose) {
    Serial.print("Deactivating directions from: "); Serial.println(source);
    //TODO Verify via A1/A2 this is correct

    if (portActive) Serial.println("Port was active");
    if (stbdActive) Serial.println("Starboard was active");
  }
  digitalWrite(PORT_CTRL_PIN, HIGH);
  digitalWrite(STBD_CTRL_PIN, HIGH);
  //TODO Verify A1/A2 that both are LOW

  portActive = false;
  stbdActive = false;
}


bool checkVoltage(uint8_t pin, uint8_t minVolts, uint8_t maxVolts){
// TODO This should be given the pin, over, under
// TODO return true/false if it's not in that range.
  unsigned long now = millis();
  int raw = analogRead(pin);
  float vout = raw * (REF_VOLTAGE / 1023.0);
  float vbat = vout * ((R1 + R2) / R2);

  if (vbat < minVolts) {
    Serial.println("Low voltage detected");
    return false;
  }
  if (vbat > maxVolts) {
    Serial.println("High voltage detected");
    return false;
  }



  return true;
}
