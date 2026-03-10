---
title: LCD Display for Temperature, Setpoint, and Relay State
type: feat
status: completed
date: 2026-03-10
branch: feat/lcd-display
---

# LCD Display for Temperature, Setpoint, and Relay State

## Overview

Add a 16x2 I2C LCD to the thermostat to show live temperature, setpoint, and relay state. The display updates every read cycle (piggy-backing on the existing `CONVERSION_MS` gate) with no additional timers or `delay()` calls.

## Technical Considerations

### Required Library

Install via Arduino Library Manager:

| Library | Author | Purpose |
|---|---|---|
| `LiquidCrystal_I2C` | Frank de Brabander | 16x2 LCD over I2C |

### Pin Assignments

I2C uses A4 (SDA) and A5 (SCL) on Uno/Nano — both are free. No conflicts with existing pins (D2 = DS18B20, D4 = relay).

| Component | Pin | Notes |
|---|---|---|
| LCD SDA | A4 | I2C data |
| LCD SCL | A5 | I2C clock |

### I2C Address

Most 16x2 I2C modules use `0x27`. If the display doesn't initialise, try `0x3F`. Add to the configuration block as `LCD_ADDRESS`.

### Display Layout

```
Line 0: Temp: 23.5 C
Line 1: SP:24.0    [ON]
```

No sensor at startup (`sensorOk == false`):
```
Line 0: No Sensor!
Line 1: SP:24.0   [OFF]
```

Sensor disconnected mid-run (`sensorOk == true`):
```
Line 0: Sensor Lost!
Line 1: SP:24.0   [OFF]
```

Safety cutoff state:
```
Line 0: Temp: 30.1 C
Line 1: CUTOFF    [OFF]
```

The `sensorOk` flag (already in the sketch) distinguishes startup absence from mid-run disconnect. A new file-scope `bool sensorError` flag tracks whether the most recent read was invalid, so `updateLCD()` always knows the current error state.

### Update Strategy

LCD updates happen at the same point in `loop()` where the Serial temperature print already occurs — after a valid conversion read. No separate refresh timer needed. LCD writes are fast (microseconds) and do not violate the 4-second watchdog window.

### Conventions to Follow

- Add `LCD_ADDRESS` to the existing `#define` configuration block
- Wrap any LCD string literals in `F()` where the library supports it
- No `delay()` in LCD code
- Keep LCD update logic in a dedicated `updateLCD()` helper function to keep `loop()` readable

## Implementation Notes

- `tempC` is currently a local variable inside `loop()`. Pass it as a parameter to `updateLCD()` — no need to promote it to file scope.
- `relayOn` and `sensorOk` are already file-scope booleans — accessible directly from `updateLCD()`.
- Add `bool sensorError` as a file-scope variable. Set it `true` in the invalid-reading error path, `false` after a valid read. `updateLCD()` reads it to know whether to show temperature or an error message.
- `SETPOINT_C` is a compile-time `#define` — accessible everywhere.
- Use `lcd.setCursor()` and `lcd.print()` for each field. Clear individual fields by printing spaces rather than calling `lcd.clear()` (which causes a visible flicker).

## Acceptance Criteria

- [x] `LiquidCrystal_I2C` library installed and sketch compiles
- [x] LCD initialises on startup and backlight turns on
- [x] Line 0 shows current temperature updated every read cycle
- [x] Line 1 shows setpoint and relay state (`[ON]` / `[OFF]`)
- [x] "No Sensor!" shown on line 0 when sensor absent at startup (`sensorOk == false`)
- [x] "Sensor Lost!" shown on line 0 when sensor disconnects mid-run (`sensorOk == true`)
- [x] Safety cutoff state shown on line 1 when `MAX_SAFE_TEMP` is exceeded
- [x] No `delay()` introduced
- [x] `LCD_ADDRESS` defined in the configuration block
- [x] No display flicker (use field-level overwrites, not `lcd.clear()`)

## Dependencies & Risks

| Item | Notes |
|---|---|
| I2C address | Most modules use `0x27`; try `0x3F` if display doesn't respond |
| Library version | `LiquidCrystal_I2C` by Frank de Brabander, any recent version |
| I2C pull-ups | Most I2C LCD backpack modules include onboard pull-ups; no external resistors needed |
| Watchdog | I2C `begin()` and `print()` calls complete in microseconds — well within the 4s window |

## MVP

### `Thermostat.ino` changes

```cpp
// Add to includes:
#include <LiquidCrystal_I2C.h>

// Add to configuration block:
#define LCD_ADDRESS 0x27  // try 0x3F if display doesn't respond

// Add after config block:
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// Add to setup():
lcd.init();
lcd.backlight();

// Add file-scope state variable:
bool sensorError = false;  // true when most recent read was invalid

// Add helper function:
void updateLCD(float tempC) {
  // Line 0: temperature or error
  lcd.setCursor(0, 0);
  if (sensorError) {
    if (sensorOk) {
      lcd.print(F("Sensor Lost!    "));  // was working, now disconnected
    } else {
      lcd.print(F("No Sensor!      "));  // never seen at startup
    }
  } else if (tempC >= MAX_SAFE_TEMP) {
    lcd.print(F("Temp: "));
    lcd.print(tempC, 1);
    lcd.print(F(" C  "));
  } else {
    lcd.print(F("Temp: "));
    lcd.print(tempC, 1);
    lcd.print(F(" C  "));
  }

  // Line 1: setpoint + relay state (or cutoff warning)
  lcd.setCursor(0, 1);
  if (!sensorError && tempC >= MAX_SAFE_TEMP) {
    lcd.print(F("CUTOFF    "));
  } else {
    lcd.print(F("SP:"));
    lcd.print(SETPOINT_C, 1);
    lcd.print(F("  "));
  }
  lcd.print(relayOn ? F("  [ON] ") : F(" [OFF] "));
}

// Call from loop() after Serial temp print:
updateLCD(tempC);
```

## Sources & References

- LiquidCrystal_I2C library: https://github.com/johnrickman/LiquidCrystal_I2C
- Existing sketch: `Thermostat/Thermostat.ino`
- Related branch: `feat/lcd-display`
