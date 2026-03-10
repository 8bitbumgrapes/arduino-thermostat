---
status: complete
priority: p2
issue_id: "004"
tags: [code-review, performance]
---

# Non-Blocking Timing and Async Sensor Reads

## Problem Statement

The loop blocks for ~1.75 seconds per cycle: `requestTemperatures()` busy-waits ~750ms for DS18B20 conversion, then `delay(1000)` blocks a further second. The MCU does nothing during this time. This also makes adding a watchdog timer difficult (a 2s window would be routinely exceeded).

## Proposed Solution

Use `millis()` timestamps and `setWaitForConversion(false)`:

```cpp
void setup() {
    // ...
    sensors.setWaitForConversion(false);  // async conversion
}

void loop() {
    static uint32_t lastRequest = 0;
    static uint32_t lastRead = 0;
    uint32_t now = millis();

    // Request conversion every 1 second
    if (now - lastRequest >= 1000UL) {
        sensors.requestTemperatures();
        lastRequest = now;
    }

    // Read result 750ms after request
    if (now - lastRead >= 1750UL) {
        float tempC = sensors.getTempCByIndex(0);
        lastRead = now;
        // ... validation and control logic ...
    }
}
```

Using `uint32_t` (not `int`) ensures correct rollover behaviour past the ~49 day `millis()` overflow.

## Acceptance Criteria

- [x] `delay()` removed from the main loop
- [x] `sensors.setWaitForConversion(false)` called in `setup()`
- [x] Conversion and read separated by at least 750ms using `millis()`
- [x] Loop cycle time reduced to < 1ms (excluding sensor read latency)
- [x] Watchdog timer can be safely enabled after this change
