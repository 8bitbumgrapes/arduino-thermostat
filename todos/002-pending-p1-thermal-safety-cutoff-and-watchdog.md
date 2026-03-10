---
status: complete
priority: p1
issue_id: "002"
tags: [code-review, safety, hardware]
---

# Thermal Safety Cutoff and Watchdog Timer

## Problem Statement

Two independent failure modes can leave the relay permanently energized with no recovery:

1. **No hard temperature ceiling:** A misconfigured setpoint or a plausible-but-wrong sensor reading can keep the heater on indefinitely above a safe threshold.
2. **No watchdog timer:** If the sketch hangs (locked OneWire bus, library deadlock, stack corruption), the MCU stalls silently with the relay in its last state. Nothing resets it.

## Proposed Solution

### 1. Hard thermal safety cutoff

Add a `MAX_SAFE_TEMP_C` constant and enforce it unconditionally before the normal hysteresis logic:

```cpp
#define MAX_SAFE_TEMP_C  35.0   // tune to your application

// In loop(), before the normal control logic:
if (tempC >= MAX_SAFE_TEMP_C) {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println("SAFETY CUTOFF: max temperature exceeded");
    delay(1000);
    return;
}
```

### 2. AVR Watchdog Timer

Enable the watchdog in `setup()` and pet it at the top of every `loop()`. If the loop stalls for more than 4 seconds, the MCU resets and `setup()` re-initialises the relay pin to `RELAY_OFF`.

**Note:** The watchdog requires the blocking `delay()` calls to be replaced with `millis()`-based timing first (see todo 004) — the current ~1.75s block per loop cycle is dangerously close to a 2s watchdog window.

```cpp
#include <avr/wdt.h>

void setup() {
    // ... existing setup ...
    wdt_enable(WDTO_4S);
}

void loop() {
    wdt_reset();
    // ... rest of loop ...
}
```

## Acceptance Criteria

- [x] `MAX_SAFE_TEMP_C` defined in the configuration block
- [x] Safety cutoff de-energizes relay and logs before returning
- [x] Watchdog enabled with a window safely above the loop cycle time
- [x] `wdt_reset()` called at the top of every loop iteration
