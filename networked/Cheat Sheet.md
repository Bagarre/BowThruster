# 📘 Bow Thruster & Windlass CAN Protocol Cheat Sheet

## 🔧 CAN Bus Settings

| Parameter | Value                  |
|-----------|------------------------|
| Speed     | 250 kbps               |
| Topology  | Terminated bus (120Ω at both ends) |
| Wires     | CANH, CANL, GND (+12V optional) |

---

## 📤 Messages Sent by HELM ➝ BOW

| CAN ID  | Purpose              | Data Format                          | Notes                      |
|--------|----------------------|--------------------------------------|----------------------------|
| `0x100` | Power Arm Button     | `[0x01] = Pressed`, `[0x00] = Idle`  | Sent every 50ms while held |
| `0x110` | Thruster Direction   | `[0x01] = Port`, `[0x02] = Starboard`, `[0x00] = None` | Only one direction active |
| `0x120` | Windlass Direction   | `[0x01] = Up`, `[0x02] = Down`, `[0x00] = None` |                           |

---

## 📥 Messages Sent by BOW ➝ HELM

| CAN ID  | Purpose           | Data Format                            | Notes                                |
|--------|-------------------|----------------------------------------|--------------------------------------|
| `0x200` | System Status     | `bitfield`: `0x01` = Armed, `0x02` = Low Voltage | Multiple flags can be combined |
| `0x210` | Beeper Command    | `[count, onTime, offTime, flags]`      | Used to trigger feedback buzzer      |

### 🧨 Beeper Format (`0x210`)
- `byte 0` = number of beeps (e.g. 2)
- `byte 1` = on time in ms (e.g. 50)
- `byte 2` = off time in ms (e.g. 50)
- `byte 3` = flags:
  - `0x00` = normal beeps
  - `0x01` = end with long beep (300ms)

**Example:**
`[3, 50, 50, 0]` → 3 quick beeps
`[1, 100, 0, 1]` → 1 beep + long final

---

## 🔄 Control Logic Summary

- Commands are **heartbeat-based**: they must repeat every 50ms.
- If both helm and bow buttons are active but conflict (e.g. Port + Starboard), the system **locks out** and disables output.
- **isArmed** must be `true` to enable thruster or windlass.
- System **auto-disarms** on low voltage (<11.5V).
- Status LED and buzzer follow state:
  - Armed → solid LED
  - Low Voltage → flashing LED
  - Lockout → 2 short beeps
