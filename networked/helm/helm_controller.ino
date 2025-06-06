/*
 * ---------------------------------------------------------------
 * Helm Remote Control Sender (Helm Feather)
 * ---------------------------------------------------------------
 * Description:
 *   Reads helm control buttons and sends corresponding CAN bus
 *   commands to the Bow Feather controller. Mirrors system status
 *   with a status LED and buzzer based on received CAN messages.
 *
 * Features:
 *   - Momentary button detection (Power, Left, Right, Up, Down)
 *   - CAN bus heartbeat transmission every 50ms
 *   - LED status: Solid for armed, flashing for low voltage
 *   - Beeper activation in response to CAN messages
 *
 * CAN Messaging:
 *   - 0x100: Power (armed/disarmed) [Out]
 *   - 0x110: Bow thruster direction command [Out]
 *   - 0x120: Windlass up/down command [Out]
 *   - 0x200: System status (armed/low voltage) [In]
 *   - 0x210: Beeper control [In]
 *
 * Author: David Ross
 * Date: 26 April 2025
 * Version: 1.0
 * License: MIT
 * ---------------------------------------------------------------
 */



#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

#define POWER_BUTTON_PIN     11
#define BT_LEFT_PIN          10
#define BT_RIGHT_PIN          9
#define WINDLASS_UP_PIN       8
#define WINDLASS_DOWN_PIN     7
#define BEEPER_PIN           A0
#define STATUS_LED_PIN       A1

const unsigned long sendInterval = 50;
unsigned long lastSendTime = 0;

bool systemArmed = false;
bool voltageLow = false;

bool beeperActive = false;
unsigned long beeperTimer = 0;
int beeperStep = 0;
int beepCount = 0;
int targetBeeps = 0;
unsigned long beepOnTime = 50;
unsigned long beepOffTime = 50;
bool beepLongLast = false;

void setup() {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BT_LEFT_PIN, INPUT_PULLUP);
  pinMode(BT_RIGHT_PIN, INPUT_PULLUP);
  pinMode(WINDLASS_UP_PIN, INPUT_PULLUP);
  pinMode(WINDLASS_DOWN_PIN, INPUT_PULLUP);
  pinMode(BEEPER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  digitalWrite(BEEPER_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.begin(9600);
  Can0.begin();
  Can0.setBaudRate(250000);
}

void loop() {
  unsigned long now = millis();
  updateBeeps();
  processIncomingCAN();

  if (now - lastSendTime >= sendInterval) {
    lastSendTime = now;
    sendPowerStatus();
    sendThrusterCommand();
    sendWindlassCommand();
  }

  if (voltageLow) {
    int flash = (millis() / 250) % 2;
    digitalWrite(STATUS_LED_PIN, flash ? HIGH : LOW);
  } else if (systemArmed) {
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else {
    digitalWrite(STATUS_LED_PIN, LOW);
  }
}

void sendPowerStatus() {
  CAN_message_t msg;
  msg.id = 0x100;
  msg.len = 1;
  msg.buf[0] = digitalRead(POWER_BUTTON_PIN) == LOW ? 0x01 : 0x00;
  Can0.write(msg);
}

void sendThrusterCommand() {
  CAN_message_t msg;
  msg.id = 0x110;
  msg.len = 1;
  if (digitalRead(BT_LEFT_PIN) == LOW) msg.buf[0] = 0x01;
  else if (digitalRead(BT_RIGHT_PIN) == LOW) msg.buf[0] = 0x02;
  else msg.buf[0] = 0x00;
  Can0.write(msg);
}

void sendWindlassCommand() {
  CAN_message_t msg;
  msg.id = 0x120;
  msg.len = 1;
  if (digitalRead(WINDLASS_UP_PIN) == LOW) msg.buf[0] = 0x01;
  else if (digitalRead(WINDLASS_DOWN_PIN) == LOW) msg.buf[0] = 0x02;
  else msg.buf[0] = 0x00;
  Can0.write(msg);
}

void processIncomingCAN() {
  CAN_message_t msg;
  if (Can0.read(msg)) {
    if (msg.id == 0x210 && msg.len == 4) {
      int count = msg.buf[0];
      int onTime = msg.buf[1];
      int offTime = msg.buf[2];
      bool longFinal = (msg.buf[3] & 0x01);
      startBeeps(count, onTime, offTime, longFinal);
    }
    else if (msg.id == 0x200 && msg.len == 1) {
      uint8_t status = msg.buf[0];
      systemArmed = status & 0x01;
      voltageLow = status & 0x02;
    }
  }
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
