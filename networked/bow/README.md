
# Bow Feather: Thruster + Windlass Controller

This Feather microcontroller directly controls the bow thruster and windlass. It handles:

- Input buttons for local manual control
- Solenoid activation via digital pins
- Automatic timeout shutoff (5s for thruster, 2 min for windlass)
- Lockout if conflicting commands are received
- Monitoring voltage and disabling on low battery
- Receiving CAN commands from the helm
- Sending back system status and beep triggers via CAN

## Pin Assignments

| Function           | Pin  |
|--------------------|------|
| Power Arm Button   | 11   |
| Thruster Port Btn  | 10   |
| Thruster Stbd Btn  | 9    |
| Windlass Up Btn    | 8    |
| Windlass Down Btn  | 7    |
| Beeper             | A4   |
| Status LED         | A3   |
| Thruster Port Out  | 5    |
| Thruster Stbd Out  | 6    |
| Windlass Up Out    | 4    |
| Windlass Down Out  | 3    |
| Solenoid Enable    | 2    |
| Battery Voltage    | A0   |

## CAN IDs (Bow-Side)

| ID    | Direction | Purpose              |
|-------|-----------|----------------------|
| 0x100 | In        | Power on/off         |
| 0x110 | In        | Bow thruster command |
| 0x120 | In        | Windlass command     |
| 0x200 | Out       | Status update        |
| 0x210 | Out       | Beeper trigger       |

## Behavior

- If both port/starboard or up/down are pressed: system deactivates
- After timeout: solenoids shut off automatically
- CAN inputs are merged with physical buttons
- Loss of CAN command causes automatic stop within 100ms
