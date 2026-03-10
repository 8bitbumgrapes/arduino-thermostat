---
status: complete
priority: p1
issue_id: "001"
tags: [code-review, safety, hardware]
---

# Relay Failsafe on Sensor Disconnect

## Problem Statement

When the DS18B20 sensor disconnects, the sketch returns early without touching `RELAY_PIN`. If the relay was energized at the moment of disconnection, it stays on indefinitely — the heater runs without any upper-bound cutoff. This is a runaway heating scenario.

## Findings

**File:** `Thermostat/Thermostat.ino` lines 29-33

Current code:
```cpp
if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("ERROR: sensor disconnected");
    delay(1000);
    return;  // relay state unchanged
}
```

## Proposed Solution

Add `digitalWrite(RELAY_PIN, RELAY_OFF)` as the first line of the error handler:

```cpp
if (tempC == DEVICE_DISCONNECTED_C) {
    digitalWrite(RELAY_PIN, RELAY_OFF);  // fail-safe: cut heat
    Serial.println("ERROR: sensor disconnected - relay forced OFF");
    delay(1000);
    return;
}
```

## Acceptance Criteria

- [x] Relay de-energizes immediately when sensor disconnects
- [x] Error message updated to confirm relay was forced off
- [x] Relay re-energizes normally once sensor reconnects and temperature is below setpoint
