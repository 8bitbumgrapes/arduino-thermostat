---
title: "Arduino Thermostat: Relay State Sync and Sensor Re-scan After Disconnect"
date: 2026-03-10
category: logic-errors
tags: [arduino, thermostat, relay, ds18b20, dallastemperature, state-sync, bug-fix]
symptoms:
  - relay stays off after sensor reconnects even though temperature requires it on
  - sensor error message loops indefinitely after plugging sensor in at runtime
environment: [Arduino Uno/Nano, DS18B20, DallasTemperature library, relay module]
difficulty: easy
---

# Arduino Thermostat: Relay State Sync and Sensor Re-scan After Disconnect

## Overview

Two bugs cause the thermostat to fail silently after a sensor disconnect or a cold boot with no sensor attached. Both share the same root cause pattern: one-time initialisation or state tracking that is never retried when conditions change at runtime.

---

## Bug 1: `relayOn` State Variable Not Synced on Forced Relay-Off

### Symptom

- Relay de-energizes correctly when sensor disconnects ✓
- After reconnecting the sensor at a temperature that should activate the relay, relay stays off
- No "Relay ON" log message appears

### Root Cause

The thermostat tracks relay state in `bool relayOn` to avoid redundant `digitalWrite` calls. In error and safety paths, `digitalWrite(RELAY_PIN, RELAY_OFF)` was called without updating `relayOn`. When the fault cleared, the control loop compared `desired (true)` against `relayOn (true, stale)` — found no difference — and never called `digitalWrite`, leaving the relay physically off but the variable still reporting it as on.

### Fix

Set `relayOn = false` alongside every forced `RELAY_OFF` write in error/safety paths:

```cpp
if (tempC < -55.0 || tempC > 125.0) {
    relayOn = false;  // sync state with hardware
    digitalWrite(RELAY_PIN, RELAY_OFF);
    // ...
}

if (tempC >= MAX_SAFE_TEMP) {
    relayOn = false;  // sync state with hardware
    digitalWrite(RELAY_PIN, RELAY_OFF);
    // ...
}
```

### Prevention

Encapsulate all relay writes in a single helper to make mismatches structurally impossible:

```cpp
void setRelay(bool on) {
    relayOn = on;
    digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}
```

Every call site becomes `setRelay(true)` / `setRelay(false)` — the variable and the pin are always updated together.

---

## Bug 2: DallasTemperature Bus Not Re-Scanned After Cold Boot With No Sensor

### Symptom

- Boot with sensor unplugged → correct error message ✓
- Plug sensor in after boot → error message continues indefinitely, relay never recovers

### Root Cause

`sensors.begin()` scans the OneWire bus and caches device addresses. If no sensor is present at boot, the cache is empty. Plugging one in later doesn't trigger a re-scan — `getTempCByIndex(0)` returns `DEVICE_DISCONNECTED_C` forever because the library has no address to query.

### Fix

Call `sensors.begin()` again in the error handler when `sensorOk` is still `false` (sensor never successfully read), to re-scan the bus for newly connected devices:

```cpp
if (tempC < -55.0 || tempC > 125.0) {
    relayOn = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
    if (sensorOk) {
        Serial.println(F("ERROR: sensor disconnected - relay forced OFF"));
    } else {
        Serial.println(F("ERROR: no sensor detected - retrying scan"));
        sensors.begin();  // re-scan bus in case sensor was just connected
    }
    return;
}
```

The `sensorOk` guard ensures re-scanning only happens before a sensor has ever been seen — avoiding unnecessary bus scans on every mid-operation disconnect.

### Prevention

- Add a comment next to `sensors.begin()` in `setup()` noting that a matching call exists in the error handler, so a future maintainer doesn't remove what looks like a duplicate.
- After `sensors.begin()`, log `sensors.getDeviceCount()` to Serial during development to immediately confirm whether any device responded.

---

## Summary

| Bug | Cause | Fix |
|---|---|---|
| Relay stays off after reconnect | `relayOn` not updated on forced relay-off | Set `relayOn = false` alongside every `RELAY_OFF` write; prefer a `setRelay()` wrapper |
| Sensor not detected after boot | `sensors.begin()` called only once; misses devices connected later | Re-call `sensors.begin()` in the error handler when `sensorOk` is false |

## Related

- Plan: `docs/plans/2026-03-10-feat-arduino-ds18b20-relay-thermostat-plan.md`
- Solution: `docs/solutions/hardware-issues/relay-active-high-low-polarity-mismatch.md`
- Implementation: `Thermostat/Thermostat.ino`
