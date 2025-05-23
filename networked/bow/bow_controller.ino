/*
 * ---------------------------------------------------------------
 * Bow Thruster + Windlass Controller (Bow Feather)
 * ---------------------------------------------------------------
 * Description:
 *   Directly controls bow thruster and windlass relays.
 *   Merges physical button inputs with CAN bus commands.
 *   Manages system arming, solenoid control, voltage monitoring,
 *   automatic lockout, and beeper/status feedback over CAN.
 *
 * Features:
 *   - Local button and remote CAN control
 *   - Solenoid enable output tied to arm status
 *   - Auto shutdown timers and lockout protections
 *   - Low voltage auto-disarm
 *   - Status reporting over CAN (Armed / Low Voltage)
 *   - Beeper activation via CAN
 *
 * CAN Messaging:
 *   - 0x100: Power (armed/disarmed) [In]
 *   - 0x110: Bow thruster direction [In]
 *   - 0x120: Windlass up/down command [In]
 *   - 0x200: Status update (armed/low voltage) [Out]
 *   - 0x210: Beeper control [Out]
 *
 * Author: David Ross
 * Date: 26 April 2025
 * Version: 1.0
 * License: MIT
 * ---------------------------------------------------------------
 */
#include <OneWire.h>
#include <DallasTemperature.h>

#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

// =========================
// CONFIGURATION
// =========================

#define BATTERY_TEMP_THRESHOLD 60.0 // Celsius
#define THRUSTER_TEMP_THRESHOLD 70.0 // Celsius


#define BEEPER_PIN                A4
#define STATUS_LED_PIN            A3
#define BATTERY_TEMP_PIN          A2 // Digital pin for DS18B20
#define THRUSTER_TEMP_PIN         A1 // Analog pin for TMP36
#define VOLTAGE_PIN               A0

#define POWER_BUTTON_PIN         11
#define PORT_BUTTON_PIN          10
#define STARBOARD_BUTTON_PIN      9
#define WINDLASS_UP_BUTTON_PIN    8
#define WINDLASS_DOWN_BUTTON_PIN  7
#define STBD_OUTPUT_PIN           6
#define PORT_OUTPUT_PIN           5
#define WINDLASS_UP_OUTPUT_PIN    4
#define WINDLASS_DOWN_OUTPUT_PIN  3
#define SOLENOID_ENABLE_PIN       2


const float R1 = 10000.0;
const float R2 = 3300.0;
const float REF_VOLTAGE = 5.0;
const float LOW_VOLTAGE_THRESHOLD = 11.5;
const float MAX_THRUST_TIME = 5000.0;
const float LOCKOUT_TIME = 2000.0;
const float PORT_2_STBD_DELAY = 1500.0;
const float AUTO_SHUTDOWN_TIMER = 300000;
const float MAX_WINDLASS_TIME = 120000.0;

// =========================
// SYSTEM STATE
// =========================
bool verbose = true;

unsigned long lastActionTime = 0;
unsigned long powerButtonStartTime = 0;
unsigned long stbdPressTime = 0;
unsigned long portPressTime = 0;
unsigned long lastPortReleaseTime = 0;
unsigned long lastStbdReleaseTime = 0;
unsigned long lockOutTime = 0;

unsigned long windlassUpPressTime = 0;
unsigned long windlassDownPressTime = 0;

bool isArmed = false;
bool powerButtonHeld = false;
bool powerStatusFlipped = false;
bool portActive = false;
bool stbdActive = false;
bool portWasPressed = false;
bool stbdWasPressed = false;
bool voltageLow = false;

bool windlassUpWasPressed = false;
bool windlassDownWasPressed = false;
bool windlassUpActive = false;
bool windlassDownActive = false;

bool beeperActive = false;
unsigned long beeperTimer = 0;
int beeperStep = 0;
int beepCount = 0;
int targetBeeps = 0;
unsigned long beepOnTime = 50;
unsigned long beepOffTime = 50;
bool beepLongLast = false;

// CAN Tracking
bool canPowerArmed = false;
bool canBTLeftActive = false;
bool canBTRightActive = false;
bool canWindlassUpActive = false;
bool canWindlassDownActive = false;

unsigned long lastPowerCANMillis = 0;
unsigned long lastBTLeftCANMillis = 0;
unsigned long lastBTRightCANMillis = 0;
unsigned long lastWindlassUpCANMillis = 0;
unsigned long lastWindlassDownCANMillis = 0;

OneWire oneWire(BATTERY_TEMP_PIN);
DallasTemperature batteryTempSensor(&oneWire);

// =========================
// SETUP
// =========================
void setup() {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STARBOARD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PORT_OUTPUT_PIN, OUTPUT);
  pinMode(STBD_OUTPUT_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(BEEPER_PIN, OUTPUT);

  pinMode(WINDLASS_UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(WINDLASS_DOWN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(WINDLASS_UP_OUTPUT_PIN, OUTPUT);
  pinMode(WINDLASS_DOWN_OUTPUT_PIN, OUTPUT);

  pinMode(SOLENOID_ENABLE_PIN, OUTPUT);

  digitalWrite(PORT_OUTPUT_PIN, LOW);
  digitalWrite(STBD_OUTPUT_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);
  digitalWrite(BEEPER_PIN, LOW);
  digitalWrite(WINDLASS_UP_OUTPUT_PIN, LOW);
  digitalWrite(WINDLASS_DOWN_OUTPUT_PIN, LOW);
  digitalWrite(SOLENOID_ENABLE_PIN, LOW);

  batteryTempSensor.begin();

  Serial.begin(9600);
  Can0.begin();
  Can0.setBaudRate(250000);
}

// =========================
// LOOP
// =========================
void loop() {
  unsigned long now = millis();
  checkTemperatures();
  updateBeeps();
  processCANMessages();

  if (!checkBatteryVoltage()) {
    deactivateThruster("Low Battery disarm");
    deactivateWindlass("Low Battery disarm");
    isArmed = false;
    digitalWrite(SOLENOID_ENABLE_PIN, LOW);
    delay(100);
  }

  isArmed = (digitalRead(POWER_BUTTON_PIN) == LOW) || (canPowerArmed && (now - lastPowerCANMillis < 100));
  digitalWrite(SOLENOID_ENABLE_PIN, isArmed ? HIGH : LOW);

  if (isArmed) {
    handleThrustControl(now);
    handleWindlassControl(now);
  }
}

void deactivateThruster(String source) {
  if (verbose) {
    Serial.print("Deactivating thruster from: "); Serial.println(source);
    if (portActive) Serial.println("Port was active");
    if (stbdActive) Serial.println("Starboard was active");
  }
  digitalWrite(PORT_OUTPUT_PIN, LOW);
  digitalWrite(STBD_OUTPUT_PIN, LOW);
  portActive = false;
  stbdActive = false;
}

void deactivateWindlass(String source) {
  if (verbose) {
    Serial.print("Deactivating windlass from: "); Serial.println(source);
  }
  digitalWrite(WINDLASS_UP_OUTPUT_PIN, LOW);
  digitalWrite(WINDLASS_DOWN_OUTPUT_PIN, LOW);
  windlassUpActive = false;
  windlassDownActive = false;
}

void processCANMessages() {
  CAN_message_t msg;
  if (Can0.read(msg)) {
    unsigned long now = millis();
    switch (msg.id) {
      case 0x100:
        if (msg.len >= 1) {
          canPowerArmed = (msg.buf[0] == 0x01);
          lastPowerCANMillis = now;
        }
        break;
      case 0x110:
        if (msg.len >= 1) {
          canBTLeftActive = (msg.buf[0] == 0x01);
          canBTRightActive = (msg.buf[0] == 0x02);
          lastBTLeftCANMillis = now;
          lastBTRightCANMillis = now;
        }
        break;
      case 0x120:
        if (msg.len >= 1) {
          canWindlassUpActive = (msg.buf[0] == 0x01);
          canWindlassDownActive = (msg.buf[0] == 0x02);
          lastWindlassUpCANMillis = now;
          lastWindlassDownCANMillis = now;
        }
        break;
    }
  }
}



void checkTemperatures() {
  float batteryTemp = readBatteryTemperature();
  float thrusterTemp = readThrusterTemperature();

  if (batteryTemp >= BATTERY_TEMP_THRESHOLD || thrusterTemp >= THRUSTER_TEMP_THRESHOLD) {
    // Disarm system
    isArmed = false;
    digitalWrite(SOLENOID_ENABLE_PIN, LOW);
    // Additional disarm procedures...
  }
}

float readBatteryTemperature() {
  batteryTempSensor.requestTemperatures();
  return batteryTempSensor.getTempCByIndex(0);
}

float readThrusterTemperature() {
  int analogValue = analogRead(THRUSTER_TEMP_PIN);
  float voltage = analogValue * (3.3 / 1023.0);
  return (voltage - 0.5) * 100.0; // TMP36 conversion
}




void startBeeps(int numberOfBeeps, unsigned long onTime, unsigned long offTime, bool longFinalBeep) {
  beeperActive = true;
  beeperTimer = millis();
  beeperStep = 1;
  beepCount = 0;
  targetBeeps = numberOfBeeps;
  beepOnTime = onTime;
  beepOffTime = offTime;
  beepLongLast = longFinalBeep;
  digitalWrite(BEEPER_PIN, HIGH);
}

void updateBeeps() {
  if (!beeperActive) return;
  unsigned long now = millis();
  if (beeperStep == 1) {
    if (now - beeperTimer >= beepOnTime) {
      digitalWrite(BEEPER_PIN, LOW);
      beeperTimer = now;
      beeperStep = 2;
      beepCount++;
    }
  } else if (beeperStep == 2) {
    if (now - beeperTimer >= beepOffTime) {
      if (beepCount >= targetBeeps) {
        if (beepLongLast) {
          digitalWrite(BEEPER_PIN, HIGH);
          beepOnTime = 300;
          beepLongLast = false;
          beeperTimer = now;
          beeperStep = 1;
          targetBeeps++;
        } else {
          beeperActive = false;
        }
      } else {
        digitalWrite(BEEPER_PIN, HIGH);
        beeperTimer = now;
        beeperStep = 1;
      }
    }
  }
}

void handleThrustControl(unsigned long now) {
  bool portPressed = (digitalRead(PORT_BUTTON_PIN) == LOW) || (canBTLeftActive && (now - lastBTLeftCANMillis < 100));
  bool stbdPressed = (digitalRead(STARBOARD_BUTTON_PIN) == LOW) || (canBTRightActive && (now - lastBTRightCANMillis < 100));

  if (portPressed && stbdPressed) {
    deactivateThruster("Conflict");
    return;
  }

  if (now - lockOutTime < LOCKOUT_TIME) {
    deactivateThruster("Cooldown");
    return;
  }

  if ((portActive && now - portPressTime > MAX_THRUST_TIME) || (stbdActive && now - stbdPressTime > MAX_THRUST_TIME)) {
    lockOutTime = now;
    deactivateThruster("Max time");
    startBeeps(2, 50, 50, false);
    return;
  }

  if (portPressed) {
    digitalWrite(STBD_OUTPUT_PIN, LOW);
    digitalWrite(PORT_OUTPUT_PIN, HIGH);
    portActive = true;
    stbdActive = false;
    portPressTime = now;
  } else if (stbdPressed) {
    digitalWrite(PORT_OUTPUT_PIN, LOW);
    digitalWrite(STBD_OUTPUT_PIN, HIGH);
    stbdActive = true;
    portActive = false;
    stbdPressTime = now;
  } else {
    deactivateThruster("No input");
  }
}

void handleWindlassControl(unsigned long now) {
  bool upPressed = (digitalRead(WINDLASS_UP_BUTTON_PIN) == LOW) || (canWindlassUpActive && (now - lastWindlassUpCANMillis < 100));
  bool downPressed = (digitalRead(WINDLASS_DOWN_BUTTON_PIN) == LOW) || (canWindlassDownActive && (now - lastWindlassDownCANMillis < 100));

  if (upPressed && downPressed) {
    deactivateWindlass("Conflict");
    return;
  }

  if ((windlassUpActive && now - windlassUpPressTime > MAX_WINDLASS_TIME) ||
      (windlassDownActive && now - windlassDownPressTime > MAX_WINDLASS_TIME)) {
    deactivateWindlass("Max time");
    startBeeps(2, 50, 50, false);
    return;
  }

  if (upPressed) {
    digitalWrite(WINDLASS_DOWN_OUTPUT_PIN, LOW);
    digitalWrite(WINDLASS_UP_OUTPUT_PIN, HIGH);
    windlassUpActive = true;
    windlassDownActive = false;
    windlassUpPressTime = now;
  } else if (downPressed) {
    digitalWrite(WINDLASS_UP_OUTPUT_PIN, LOW);
    digitalWrite(WINDLASS_DOWN_OUTPUT_PIN, HIGH);
    windlassDownActive = true;
    windlassUpActive = false;
    windlassDownPressTime = now;
  } else {
    deactivateWindlass("No input");
  }
}
