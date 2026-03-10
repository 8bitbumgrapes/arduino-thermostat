---
status: complete
priority: p2
issue_id: "003"
tags: [code-review, safety, quality]
---

# Sensor Reading Validation

## Problem Statement

Two issues with how sensor readings are validated:

1. **Float equality for sentinel:** `tempC == DEVICE_DISCONNECTED_C` compares floats with `==`. In practice this works (the sentinel is exactly -127.0f), but it's fragile — a library update or compiler flag change could break it silently.
2. **No plausibility range check:** A CRC-corrupted reading that passes as a valid non-sentinel float (e.g., 4.0°C when the room is 26°C) goes directly into the control logic, potentially energizing the relay.

## Proposed Solution

Replace the exact sentinel comparison with a range check, and add a physical plausibility guard:

```cpp
// Replace: if (tempC == DEVICE_DISCONNECTED_C)
// With:
if (tempC < -55.0f || tempC > 125.0f) {   // outside DS18B20 physical range
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println("ERROR: invalid temperature reading - relay forced OFF");
    delay(1000);
    return;
}
```

The DS18B20 is rated -55°C to +125°C. Any reading outside that range is either the disconnect sentinel or a corrupt value — both are handled the same way.

## Acceptance Criteria

- [x] Exact float `==` sentinel comparison replaced with range check
- [x] Range check covers both disconnect sentinel (-127) and physical implausibility
- [x] Relay forced off on invalid reading
- ~~Startup sensor count check~~ — removed: `getDeviceCount()` unreliable with pull-up resistor; range check in `loop()` covers the same scenario
