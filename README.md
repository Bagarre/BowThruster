
# Bow Thruster + Windlass CAN Control System

This project provides a robust CAN-based control system for a sailboat's bow thruster and windlass. It uses two Feather microcontrollers:

- **Bow Feather**: Directly controls relays and solenoids at the bow
- **Helm Feather**: Sends commands via CAN and mirrors system status with a status LED and beeper

## Features

- Momentary button inputs for thruster and windlass
- Remote control over CAN bus from helm to bow
- Dual-mode arming (local or CAN)
- Solenoid enable pin activates only when system is armed
- Automatic timeout and lockout protections
- Beeper notifications and status LED feedback
- CAN message heartbeat safety (commands stop if messages stop)

## System Overview

| Component       | Description                                     |
|----------------|-------------------------------------------------|
| Bow Feather     | Receives CAN, controls relays, enforces safety |
| Helm Feather    | Sends CAN commands from user input             |
| CAN IDs         | 0x100–0x120 (control), 0x200 (status), 0x210 (beep) |
| Voltage Monitor | Auto-disarms below 11.5V                       |

## Wiring Summary

- Uses standard 5-button interface (power, port, starboard, up, down)
- Optional buzzer and status LED at both bow and helm
- CAN operates at 250kbps with simple 4-wire trunk: V+, GND, CANH, CANL

## License

MIT License. Created by David Ross, 2025.
