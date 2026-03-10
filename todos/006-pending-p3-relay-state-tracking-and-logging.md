---
status: complete
priority: p3
issue_id: "006"
tags: [code-review, quality]
---

# Relay State Tracking and Change Logging

## Problem Statement

Two minor quality issues:

1. `digitalWrite` is called every loop iteration even when the relay state hasn't changed. On some relay driver hardware this produces a brief glitch at the coil input. It also causes unnecessary mechanical relay wear over time.
2. The Serial log prints temperature every second but never logs when the relay actually changes state, making field diagnosis harder.

## Proposed Solution

Track relay state in a variable and only write to the pin on change. Log state transitions:

```cpp
static bool relayOn = false;

bool desired;
if (tempC < SETPOINT_C - HYSTERESIS) {
    desired = true;
} else if (tempC > SETPOINT_C + HYSTERESIS) {
    desired = false;
} else {
    desired = relayOn;  // hold current state in hysteresis band
}

if (desired != relayOn) {
    relayOn = desired;
    digitalWrite(RELAY_PIN, relayOn ? RELAY_ON : RELAY_OFF);
    Serial.print(F("Relay "));
    Serial.println(relayOn ? F("ON") : F("OFF"));
}
```

## Acceptance Criteria

- [x] `digitalWrite` only called when relay state actually changes
- [x] State transition logged to Serial ("Relay ON" / "Relay OFF")
- [x] Hysteresis band hold behaviour remains correct
