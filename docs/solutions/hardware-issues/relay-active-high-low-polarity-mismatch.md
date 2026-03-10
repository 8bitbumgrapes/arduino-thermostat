---
title: Arduino Thermostat Relay Stays On Due to Incorrect Active-HIGH/LOW Logic
date: 2026-03-10
category: hardware-issues
tags: [arduino, relay, active-high, active-low, thermostat, DS18B20, logic-inversion, hardware-bug]
symptoms: [relay energizes immediately at boot before temperature is read, relay cannot be turned off regardless of temperature, relay remains on even when temperature exceeds turn-off threshold]
environment: [Arduino Uno/Nano, DS18B20 temperature sensor, OneWire library, DallasTemperature library, relay module on pin D4]
difficulty: easy
---

# Relay Stays On: Active-HIGH vs Active-LOW Polarity Mismatch

## Symptoms

- Relay energizes immediately at boot, before any temperature reading occurs
- Relay cannot be turned off regardless of temperature
- Serial monitor shows correct temperature readings, but relay ignores the turn-off threshold

## Root Cause

The code assumed an **active-LOW** relay module (energized by driving the pin LOW), but the physical module was **active-HIGH** (energized by driving the pin HIGH).

Most relay breakout boards use an optocoupler with an inverted output, making them active-LOW. However, some modules — particularly those without an optocoupler or with a non-inverting driver — are active-HIGH. Using the wrong assumption means `RELAY_OFF` (HIGH) actually energizes the relay, and `RELAY_ON` (LOW) releases it — exactly backwards.

The result: `digitalWrite(RELAY_PIN, RELAY_OFF)` in `setup()` energizes the relay at boot, and every subsequent "turn off" call keeps it on.

## Diagnostic Test

**Quickest check:** Restart the Arduino. If the relay clicks on immediately at boot — before the `loop()` has run even once — the polarity assumption is wrong.

## Solution

Swap the `RELAY_ON` / `RELAY_OFF` defines to match the actual module:

```cpp
// ❌ Wrong (assumed active-LOW):
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ✅ Correct for active-HIGH module:
#define RELAY_ON  HIGH
#define RELAY_OFF LOW
```

Because all relay state changes in the sketch use these named constants rather than raw `HIGH`/`LOW`, this two-line change fixes every call site automatically. No logic in `setup()` or `loop()` needs to change.

### Why the Abstraction Matters

```cpp
// All relay writes use intent, not electrical level:
digitalWrite(RELAY_PIN, RELAY_ON);   // energize
digitalWrite(RELAY_PIN, RELAY_OFF);  // de-energize
```

The mapping from intent to voltage level lives in one place. Swapping the defines propagates the fix everywhere. Without this abstraction, you'd need to audit and toggle every `digitalWrite` call manually — error-prone in larger sketches.

## Prevention

### Before Writing Code

1. **Check the PCB silkscreen** for labels like `HIGH/LOW`, `H/L`, or a jumper labeled `ACTIVE`.
2. **Read the driver chip datasheet** — NPN transistor driver = active-LOW; PNP or non-inverting optocoupler = active-HIGH.
3. **Bench test:** Power the module, leave the signal pin floating, then tie it HIGH, then LOW. Listen for the relay click and watch the indicator LED at each step.

### Checklist for Future Relay Projects

- [ ] Confirm active-HIGH vs. active-LOW before writing code; record it as a comment
- [ ] Define named polarity constants (`RELAY_ON` / `RELAY_OFF`); never use raw `HIGH`/`LOW` at call sites
- [ ] Set the pin to the safe/de-energized state in `setup()` before enabling the pin as `OUTPUT`
- [ ] Test with a safe dummy load (e.g., an LED circuit) before connecting any real load
- [ ] Never assume polarity transfers between suppliers — re-verify for every new module batch

### Boot-Time Diagnostic (Development Only)

Add this to the top of `setup()` during development to confirm polarity before any control logic runs. Remove before production.

```cpp
void relayPolarityDiagnostic() {
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(RELAY_PIN, OUTPUT);
  delay(500);  // LED on = active-HIGH; LED off = active-LOW

  digitalWrite(RELAY_PIN, LOW);
  delay(1000); // click here = active-LOW module

  digitalWrite(RELAY_PIN, HIGH);
  delay(1000); // click here = active-HIGH module

  digitalWrite(RELAY_PIN, RELAY_OFF); // leave in safe state
}
```

| LED on at raw HIGH | Click at LOW step | Module polarity |
|---|---|---|
| No | Yes | Active-LOW |
| Yes | No | Active-HIGH |

## Related

- Plan: `docs/plans/2026-03-10-feat-arduino-ds18b20-relay-thermostat-plan.md` — Dependencies & Risks section flags relay polarity as a known risk
- Implementation: `Thermostat/Thermostat.ino` — corrected defines at line 11–12
