---
title: Arduino DS18B20 Relay Thermostat
type: feat
status: active
date: 2026-03-10
---

# Arduino DS18B20 Relay Thermostat

## Overview

A single `.ino` sketch for Arduino Uno/Nano that reads a DS18B20 temperature sensor and controls a relay: energize when cold, release when warm. A configurable setpoint and hysteresis band prevent relay chatter. Serial output provides live readings for debugging.

## Technical Considerations

### Required Libraries

Install via Arduino Library Manager before compiling:

| Library | Purpose |
|---|---|
| `OneWire` by Jim Studt | 1-Wire bus communication |
| `DallasTemperature` by Miles Burton | DS18B20 abstraction |

### Pin Assignments

| Component | Arduino Pin | Notes |
|---|---|---|
| DS18B20 DATA | D2 | Requires 4.7 kΩ pull-up resistor to 5V |
| Relay signal | D4 | Most relay modules are active-LOW |

### Wiring Notes

- **DS18B20:** Connect VCC → 5V, GND → GND, DATA → D2 with a 4.7 kΩ resistor between DATA and VCC.
- **Relay module:** Connect VCC → 5V, GND → GND, IN → D4. Most common relay modules are active-LOW: `LOW` energizes the coil, `HIGH` releases it. If your module is active-HIGH, invert the `digitalWrite` calls in the sketch.

### Hysteresis

Without hysteresis, the relay would toggle rapidly when temperature hovers at the setpoint. With a ±1 °C band:

```
Relay turns ON  when temp drops BELOW  (SETPOINT - HYSTERESIS)   → e.g. < 19 °C
Relay turns OFF when temp rises ABOVE  (SETPOINT + HYSTERESIS)   → e.g. > 21 °C
Between 19–21 °C: relay holds its current state
```

### DS18B20 Conversion Time

At 12-bit resolution (default) the sensor needs ~750 ms to complete a conversion. The sketch uses `sensors.requestTemperatures()` (blocking) followed by a 1-second `delay()`, which is sufficient and keeps the code simple.

## Implementation Notes

- **Single-file sketch** — no external state, no EEPROM, no interrupts; purely synchronous.
- **Relay load isolation** — the relay coil is driven by the module's onboard transistor; the Arduino pin only sinks/sources the control signal (≤ 5 mA). The switched load is fully isolated from the Arduino.
- **Sensor failure handling** — if the sensor is disconnected, `getTempCByIndex(0)` returns `DEVICE_DISCONNECTED_C` (-127 °C). The sketch detects this and skips relay logic, printing an error to Serial.

## Acceptance Criteria

- [x] Sketch compiles without errors for Arduino Uno/Nano (AVR target)
- [x] Relay energizes when temperature is below `SETPOINT_C - HYSTERESIS`
- [x] Relay de-energizes when temperature is above `SETPOINT_C + HYSTERESIS`
- [x] Relay state does not change when temperature is within the hysteresis band
- [x] Serial monitor prints current temperature every loop iteration
- [x] Disconnected/faulty sensor prints an error message and does not change relay state
- [x] Setpoint and hysteresis are easy to find and change (top-of-file `#define`)

## Dependencies & Risks

| Item | Notes |
|---|---|
| Relay module polarity | Verify active-LOW vs active-HIGH for your specific module before connecting a load |
| 4.7 kΩ pull-up resistor | Required; DS18B20 will not communicate without it |
| Library versions | Tested pattern works with OneWire ≥ 2.3 and DallasTemperature ≥ 3.9 |
| Load voltage | Relay coil rated for 5V; ensure your module matches |

## MVP

### `Thermostat.ino`

```cpp
#include <OneWire.h>
#include <DallasTemperature.h>

// ── Configuration ─────────────────────────────────────────────────────────────
#define ONE_WIRE_BUS  2       // DS18B20 data pin
#define RELAY_PIN     4       // Relay signal pin
#define SETPOINT_C    20.0    // Target temperature in °C
#define HYSTERESIS    1.0     // ±°C band around setpoint
// ─────────────────────────────────────────────────────────────────────────────

// Active-LOW relay: LOW = coil energized (heating ON), HIGH = coil released
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(9600);
  sensors.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF); // start with relay off
}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("ERROR: sensor disconnected");
    delay(1000);
    return;
  }

  Serial.print("Temp: ");
  Serial.print(tempC, 1);
  Serial.println(" C");

  if (tempC < SETPOINT_C - HYSTERESIS) {
    digitalWrite(RELAY_PIN, RELAY_ON);   // cold → heating on
  } else if (tempC > SETPOINT_C + HYSTERESIS) {
    digitalWrite(RELAY_PIN, RELAY_OFF);  // warm → heating off
  }
  // Within hysteresis band: maintain current state

  delay(1000);
}
```

## Sources & References

- DS18B20 datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf
- DallasTemperature library: https://github.com/milesburton/Arduino-Temperature-Control-Library
- OneWire library: https://github.com/PaulStoffregen/OneWire
