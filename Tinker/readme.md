# Bow Thruster Controller

**Platform:** Adafruit Metro Mini v2 (ATmega328P, 5 V, 16 MHz)

A safety-focused bow thruster controller that enforces press-to-arm, auto-disarm, low-voltage lockout, timed thrust limits, and audible/visual feedback.

---

## Features

* **5 s press-to-arm**: Hold the power button for 5 s to enable thruster controls.
* **2 min auto-disarm**: Automatically disables after 2 minutes of inactivity.
* **Low-voltage lockout**: Continuously monitors battery voltage; disarms if below threshold.
* **Timed thrust limits**: Max thrust time per direction, then enforces a cool-down period.
* **Debounced direction switching**: Prevents rapid back-and-forth between port/starboard.
* **Self-test on boot/disarm**: Checks battery voltage and signals errors via LED/beeper.
* **Audible beeps & LED feedback**: Beeps on arm/disarm, error, thrust start/stop; status LED flashes.
* **Verbose debug**: Optional `verbose` flag to print diagnostics over Serial (9600 baud).

---

## Pin Mapping

Wire your controls and MOSFET gates to the following Metro Mini headers:

| Macro                  | Arduino Pin | Silkscreen | Header Location        |
| ---------------------- | ----------- | ---------- | ---------------------- |
| `POWER_BUTTON_PIN`     | D2          | “2”        | 2nd pin down on D-side |
| `PORT_BUTTON_PIN`      | D3          | “3”        | 3rd pin down on D-side |
| `STARBOARD_BUTTON_PIN` | D4          | “4”        | 4th pin down on D-side |
| `STATUS_LED_PIN`       | D5          | “5”        | 5th pin down on D-side |
| `BEEPER_PIN`           | D6          | “6”        | 6th pin down on D-side |
| `PORT_OUTPUT_PIN`      | D7          | “7”        | 7th pin down on D-side |
| `STBD_OUTPUT_PIN`      | D8          | “8”        | 8th pin down on D-side |
| `VOLTAGE_PIN`          | A0          | “A0”       | 1st pin down on A-side |

> **Note:** A0–A3 also serve as analog inputs for voltage sensing.

---

## Wiring Overview

1. **Power Button**: Connect between D2 and GND. Use internal pull-up.
2. **Port/Starboard Buttons**: Each between D3/D4 and GND; debounce and active LOW.
3. **Status LED**: PWM-capable on D5; cathode to GND via resistor, anode to D5.
4. **Beeper**: Piezo element to D6; other lead to GND.
5. **MOSFET Gates**: Gate pins to D7 (port) and D8 (starboard); sources to GND; drains to thruster control lines.
6. **Voltage Divider**: R1 (10 kΩ) from battery + to A1 divider node; R2 (3.3 kΩ) from divider node to GND; A1 reads node.

---

## Configuration Constants

Adjust these in `main.ino` as needed:

```cpp
const float R1 = 10000.0;             // Top resistor
const float R2 = 3300.0;              // Bottom resistor
const float REF_VOLTAGE = 5.0;        // ADC reference (Vcc)
const float LOW_VOLTAGE_THRESHOLD = 11.0;  // Minimum volts to operate
const unsigned long MAX_THRUST_TIME = 5000UL;   // ms per thrust
const unsigned long LOCKOUT_TIME = 2000UL;      // ms cool-down
```

---

## Software Setup

1. **Board Selection**: In Arduino IDE, choose **Tools → Board → Arduino/Genuino Uno** (or “Adafruit Metro Mini (ATmega328P, 5V, 16 MHz)” if installed).
2. **Port**: Select the USB serial port.
3. **Compile & Upload**: Open `main.ino`, verify, then upload.

---

## Usage

* **Arm:** Hold **power button** (D2) LOW for 5 s → LED comes ON, beep sounds.
* **Disarm:** Hold same button for 2 s → LED OFF, beep sounds.
* **Port/Starboard Thrust:** With system armed, press port or starboard button → corresponding MOSFET engages, beep, and times out after max thrust.
* **Auto-disarm:** No thrust activity for 2 min → system disarms automatically.
* **Low-Voltage Shutdown:** If battery < threshold, system disarms and status LED blinks.

---

## Troubleshooting

* **No LED or beep on arm/disarm:** Verify wiring of status LED and buzzer; check pin definitions.
* **Buttons unresponsive:** Confirm internal pull-ups and active LOW wiring.
* **Unexpected auto-disarm:** Ensure `lastActionTime` resets only on valid thrust or arm presses.
* **Voltage readings off:** Recalculate divider and ADC scaling (use multimeter to confirm).

---

## License & Credits

Original design by David Ross (05-22-2025). Ported for Metro Mini v2.

Feel free to adapt and improve!
