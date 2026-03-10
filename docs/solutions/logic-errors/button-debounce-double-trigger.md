---
title: "Arduino: Button Double-Trigger from Press and Release Bounce"
date: 2026-03-10
category: logic-errors
tags: [arduino, debounce, buttons, avr, input, pullup]
symptoms:
  - setpoint changes by double the expected step per button press
  - button appears to fire on both press and release
environment: [Arduino Uno/Nano, AVR ATmega328P, momentary push buttons, INPUT_PULLUP]
difficulty: medium
---

# Arduino: Button Double-Trigger from Press and Release Bounce

## Overview

Push buttons on an Arduino thermostat (D5 = UP, D6 = DOWN, wired to GND with `INPUT_PULLUP`) caused the setpoint to change by 1 °C or 1.5 °C per press instead of the intended 0.5 °C. The root cause was that the debounce logic fired on both the button press and the release bounce, producing two actions per physical press.

---

## Root Cause

Mechanical push buttons produce electrical noise ("bounce") on both press and release — the contacts rapidly open and close multiple times in the first ~20 ms of each transition. Without proper debounce logic, every bounce LOW reading was interpreted as a new button press. The specific failure mode was that no guard prevented the release-side bounce from satisfying the same "button pressed" condition as the original press.

---

## Investigation Steps

### Attempt 1 — FAILED: Simple Timer Debounce

Only process button input if at least 50 ms has passed since the last event.

```cpp
uint32_t lastBtnTime = 0;

if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
  if (digitalRead(BTN_UP) == LOW) {
    setpointC = min(setpointC + SETPOINT_STEP, (float)SETPOINT_MAX);
    lastBtnTime = now;
    updateLCD(lastTemp);
  }
}
```

**Why it failed:** Holding the button down meant `now - lastBtnTime >= 50ms` became true again every 50 ms, firing repeatedly for as long as the button was held — producing 1 °C or 1.5 °C jumps.

---

### Attempt 2 — FAILED: Edge Detection (HIGH→LOW Transition)

Track the previous button state and only fire on a HIGH→LOW transition.

```cpp
bool lastBtnUp = HIGH;

bool curBtnUp = digitalRead(BTN_UP);
if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
  if (curBtnUp == LOW && lastBtnUp == HIGH) {
    // fire
    lastBtnTime = now;
  }
  lastBtnUp = curBtnUp;  // BUG: updated every loop, even during bounce
}
```

**Why it failed:** `lastBtnUp` was updated on every loop iteration. On release, contacts bounce LOW→HIGH→LOW. The transient HIGH reading updated `lastBtnUp` to HIGH; the next LOW bounce then looked like a fresh HIGH→LOW press edge and fired a second time.

---

### Attempt 3 — FAILED: Freeze State During Debounce Window

Move the state update inside the debounce-gated block.

```cpp
if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
  if (curBtnUp == LOW && lastBtnUp == HIGH) {
    // fire
    lastBtnTime = now;
  }
  lastBtnUp = curBtnUp;  // moved inside block — still fails on release
}
```

**Why it failed:** Protected the press-side bounce correctly but not the release. After the 50 ms window cleared, a bounce LOW on release still satisfied `curBtnUp == LOW && lastBtnUp == HIGH`, causing a phantom second press.

---

## Working Solution: Armed Flag with Release Debounce

Introduce a boolean "armed" flag per button. The flag disarms on press and only re-arms after the button reads HIGH (released) **and** a fresh 50 ms debounce window has passed — covering the release bounce as well.

```cpp
bool     btnUpArmed   = true;
bool     btnDownArmed = true;
uint32_t lastBtnTime  = 0;

// In loop():
if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
  bool curBtnUp   = digitalRead(BTN_UP);
  bool curBtnDown = digitalRead(BTN_DOWN);

  // Re-arm on release AND restart debounce timer to cover release bounce
  if (curBtnUp == HIGH && !btnUpArmed) {
    btnUpArmed  = true;
    lastBtnTime = now;
  }
  if (curBtnDown == HIGH && !btnDownArmed) {
    btnDownArmed = true;
    lastBtnTime  = now;
  }

  // Fire once per press while armed
  if (curBtnUp == LOW && btnUpArmed) {
    btnUpArmed  = false;
    lastBtnTime = now;
    // ... action ...
  } else if (curBtnDown == LOW && btnDownArmed) {
    btnDownArmed = false;
    lastBtnTime  = now;
    // ... action ...
  }
}
```

**Why it works:**

1. **Press:** Button reads LOW, armed = true → fires, armed = false, timer resets. Press-side bounce ignored (armed is false).
2. **Held:** Button stays LOW, armed = false → fire condition never satisfied again.
3. **Release:** Button reads HIGH, armed = false → re-arms, timer resets. This is the key step — the timer reset means the 50 ms window must expire a second time before any new LOW can fire.
4. **Release bounce:** Any LOW bounce occurs within the new 50 ms window and is blocked by the outer debounce gate.

The two-phase timer reset (once on press, once on re-arm) covers both press-side and release-side bounce, guaranteeing exactly one action per physical press.

---

## Prevention

**Design checklist for button input on Arduino:**

1. **Define logic polarity explicitly.** With `INPUT_PULLUP`, document that `LOW` = pressed and `HIGH` = released. Inverted assumptions are the most common source of double-trigger bugs.
2. **Debounce both edges, not just one.** Contacts bounce on press *and* release. Any scheme guarding only the falling edge leaves the rising-edge bounce unguarded.
3. **Use an armed flag to separate "settled" from "action triggered."** The flag ensures the action fires exactly once per press cycle regardless of hold duration or bounce count.
4. **Reset the debounce timer on re-arm, not just on fire.** A single shared timer reset on both events is what covers the release bounce window.
5. **Test with Serial output first.** Print raw `digitalRead()` values at high loop frequency to observe the actual bounce signature of your specific buttons before trusting any debounce interval.

**Common mistakes:**

| Mistake | Effect |
|---|---|
| Simple timer only | Re-fires every `DEBOUNCE_MS` while held |
| Edge detection without release guard | Release bounce triggers a second fire |
| Updating state variables outside the debounce gate | Timer resets on every bounce, window never expires |

**Hardware note:** Typical mechanical push-button bounce lasts 5–20 ms; worn or cheap switches can reach 50 ms. A 50 ms software window covers virtually all consumer-grade tactile switches. Consider a hardware debounce capacitor (100 nF + 10 kΩ series) only when the pin drives an interrupt (`attachInterrupt`) or is in a high-noise electrical environment.

---

## Summary

| Attempt | Approach | Result |
|---|---|---|
| 1 | Simple 50 ms timer | Re-fires while held |
| 2 | HIGH→LOW edge detection | Release bounce creates false edge |
| 3 | Freeze state in debounce window | Release bounce fires after window clears |
| 4 ✓ | Armed flag + re-arm debounce | One action per press, both edges covered |

---

## Related

- Plan: `docs/plans/2026-03-10-feat-setpoint-buttons-plan.md`
- Solution: `docs/solutions/logic-errors/relay-state-sync-and-sensor-rescan.md`
- Implementation: `Thermostat/Thermostat.ino`
- Fix commits: `79f8efb`, `4e983fc`, `6f94de2`
