
# Helm Feather: Remote Control Sender

This Feather handles the helm-side control for bow thruster and windlass. It:

- Sends CAN control messages every 50ms while buttons are pressed
- Provides a press-to-arm button (local or remote)
- Responds to status messages from the Bow Feather
- Mirrors system state using a status LED
- Responds to beep command messages for audible feedback

## Pin Assignments

| Function           | Pin  |
|--------------------|------|
| Power Arm Button   | 11   |
| Thruster Port Btn  | 10   |
| Thruster Stbd Btn  | 9    |
| Windlass Up Btn    | 8    |
| Windlass Down Btn  | 7    |
| Beeper             | A0   |
| Status LED         | A1   |

## CAN IDs (Helm-Side)

| ID    | Direction | Purpose              |
|-------|-----------|----------------------|
| 0x100 | Out       | Power on/off command |
| 0x110 | Out       | Bow thruster command |
| 0x120 | Out       | Windlass command     |
| 0x200 | In        | System status        |
| 0x210 | In        | Beeper command       |

## Behavior

- LED is solid ON when armed, flashes if voltage is low
- Beeper responds to commands from the bow (armed/disarmed/lockout/etc.)
- Local switch inputs and CAN are decoupled — bow always enforces final control logic
