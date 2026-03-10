---
title: Runtime Setpoint Adjustment via Up/Down Buttons
type: feat
status: completed
date: 2026-03-10
branch: feat/setpoint-buttons
---

# Runtime Setpoint Adjustment via Up/Down Buttons

## Overview

Add two push buttons (UP and DOWN) to allow the user to adjust the target setpoint temperature at runtime without recompiling. Pressing UP raises the setpoint by 0.5 °C; pressing DOWN lowers it. The LCD line 1 updates immediately to reflect the new value.

## Technical Considerations

### Pin Assignments

D2 (DS18B20) and D4 (relay) are taken. D5 and D6 are free and have no secondary functions that matter here.

| Button | Pin | Wiring |
|---|---|---|
| UP | D5 | Pin → button → GND (use `INPUT_PULLUP`) |
| DOWN | D6 | Pin → button → GND (use `INPUT_PULLUP`) |

Using `INPUT_PULLUP` means the pin reads `HIGH` at rest and `LOW` when pressed — no external resistors needed.

### New `#define` Entries (Configuration Block)

Follow the existing convention of declaring all tunables in the `#define` block at the top of the sketch:

```cpp
#define BTN_UP          5       // Up button pin
#define BTN_DOWN        6       // Down button pin
#define BTN_DEBOUNCE_MS 50UL    // Debounce window in ms
#define SETPOINT_STEP   0.5     // °C per button press
#define SETPOINT_MIN    5.0     // Lowest permitted setpoint
#define SETPOINT_MAX    29.0    // Highest permitted setpoint (below MAX_SAFE_TEMP)
```

`SETPOINT_MAX` should be capped below `MAX_SAFE_TEMP` so the setpoint can never be raised to a value where the safety cutoff would immediately fire.

### `SETPOINT_C` → Runtime Variable

`SETPOINT_C` is currently a compile-time `#define`. It must become a file-scope `float`:

```cpp
float setpointC = 24.0;  // runtime-adjustable setpoint
```

A named default can be kept in the config block for clarity:

```cpp
#define SETPOINT_DEFAULT 24.0
```

References to update (all in `Thermostat/Thermostat.ino`):

| Line | Old | New |
|---|---|---|
| 56 | `dtostrf(SETPOINT_C, 5, 1, buf)` | `dtostrf(setpointC, 5, 1, buf)` |
| 125 | `if (tempC < SETPOINT_C - HYSTERESIS)` | `if (tempC < setpointC - HYSTERESIS)` |
| 127 | `else if (tempC > SETPOINT_C + HYSTERESIS)` | `else if (tempC > setpointC + HYSTERESIS)` |

### Debounce Pattern

Follow the existing `millis()` non-blocking pattern already used for the sensor read cycle. No `delay()`, no hardware interrupts.

```cpp
uint32_t lastBtnTime = 0;  // file-scope, alongside other state variables

// In loop(), before the conversion gate:
if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
  if (digitalRead(BTN_UP) == LOW) {
    setpointC = min(setpointC + SETPOINT_STEP, (float)SETPOINT_MAX);
    lastBtnTime = now;
    updateLCD(lastTemp);  // refresh display immediately
  } else if (digitalRead(BTN_DOWN) == LOW) {
    setpointC = max(setpointC - SETPOINT_STEP, (float)SETPOINT_MIN);
    lastBtnTime = now;
    updateLCD(lastTemp);  // refresh display immediately
  }
}
```

`lastTemp` is the most recent valid temperature reading — promote it from a local variable in `loop()` to a file-scope variable so `updateLCD()` can be called from the button handler without waiting for the next sensor read.

### Immediate LCD Refresh

When a button is pressed the setpoint on line 1 should update straight away, not wait up to 1 second for the next read cycle. This requires calling `updateLCD()` immediately after updating `setpointC`. To do that, `tempC` must be accessible outside the read block — promote it to file scope as `float lastTemp = 0.0`.

### Serial Logging

Log setpoint changes to Serial for field diagnosis, consistent with existing relay change logging:

```cpp
Serial.print(F("Setpoint: "));
Serial.print(setpointC, 1);
Serial.println(F(" C"));
```

## Implementation Notes

- Read both buttons at the start of every `loop()` iteration, before the conversion gate — this keeps button response fast regardless of where in the read cycle the loop is.
- A single shared `lastBtnTime` debounce timestamp is sufficient; simultaneous UP+DOWN presses simply trigger whichever branch is tested first and are ignored on the next iteration.
- `min()` and `max()` are available in the Arduino core — no extra include needed.
- `setpointC` does not need to persist across power cycles for this feature (no EEPROM). If persistence is wanted, that is a separate feature.

## Acceptance Criteria

- [x] `BTN_UP`, `BTN_DOWN`, `BTN_DEBOUNCE_MS`, `SETPOINT_STEP`, `SETPOINT_MIN`, `SETPOINT_MAX` defined in the configuration block
- [x] `SETPOINT_C` `#define` replaced by `float setpointC` file-scope variable
- [x] Pressing UP raises setpoint by `SETPOINT_STEP`, capped at `SETPOINT_MAX`
- [x] Pressing DOWN lowers setpoint by `SETPOINT_STEP`, capped at `SETPOINT_MIN`
- [x] LCD line 1 updates immediately on button press
- [x] Relay logic uses `setpointC` for both threshold comparisons
- [x] No `delay()` introduced
- [x] Setpoint change logged to Serial
- [x] Sketch compiles and watchdog is not triggered by button polling

## Dependencies & Risks

| Item | Notes |
|---|---|
| Pin conflicts | D5 and D6 are free on Uno/Nano — confirm before wiring |
| `INPUT_PULLUP` | Button must wire to GND, not 5V |
| `SETPOINT_MAX` | Must remain below `MAX_SAFE_TEMP` (currently 30.0) — enforced by the `#define` value |
| No EEPROM | Setpoint resets to default on power cycle — acceptable for now |
| `lastTemp` promotion | Promoting `tempC` to file scope is a small structural change; initial value `0.0` is safe because `updateLCD(0.0)` is already called in `setup()` |

## MVP

### `Thermostat/Thermostat.ino` changes

```cpp
// Add to configuration block:
#define SETPOINT_DEFAULT 24.0   // initial setpoint
#define BTN_UP           5      // up button pin
#define BTN_DOWN         6      // down button pin
#define BTN_DEBOUNCE_MS  50UL   // debounce window
#define SETPOINT_STEP    0.5    // °C per press
#define SETPOINT_MIN     5.0    // lower bound
#define SETPOINT_MAX     29.0   // upper bound (below MAX_SAFE_TEMP)

// Remove: #define SETPOINT_C 24.0

// Add file-scope variables:
float setpointC = SETPOINT_DEFAULT;
float lastTemp  = 0.0;
uint32_t lastBtnTime = 0;

// In setup(), add after existing pinMode calls:
pinMode(BTN_UP,   INPUT_PULLUP);
pinMode(BTN_DOWN, INPUT_PULLUP);

// In loop(), add at the top of the loop (before conversion gate):
if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
  if (digitalRead(BTN_UP) == LOW) {
    setpointC = min(setpointC + SETPOINT_STEP, (float)SETPOINT_MAX);
    lastBtnTime = now;
    Serial.print(F("Setpoint: ")); Serial.print(setpointC, 1); Serial.println(F(" C"));
    updateLCD(lastTemp);
  } else if (digitalRead(BTN_DOWN) == LOW) {
    setpointC = max(setpointC - SETPOINT_STEP, (float)SETPOINT_MIN);
    lastBtnTime = now;
    Serial.print(F("Setpoint: ")); Serial.print(setpointC, 1); Serial.println(F(" C"));
    updateLCD(lastTemp);
  }
}

// Replace all SETPOINT_C references with setpointC.
// Store last valid temp: at the point tempC is read successfully, assign lastTemp = tempC.
```

## Sources & References

- Existing sketch: `Thermostat/Thermostat.ino`
- LCD plan (for updateLCD conventions): `docs/plans/2026-03-10-feat-lcd-display-plan.md`
- Relay state sync solution (setRelay wrapper): `docs/solutions/logic-errors/relay-state-sync-and-sensor-rescan.md`
- Related branch: `feat/setpoint-buttons`
