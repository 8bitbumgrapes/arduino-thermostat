# Arduino Thermostat

A simple heating thermostat for Arduino Uno/Nano. Reads temperature from a DS18B20 sensor and controls a relay to switch heating on and off. A 16x2 I2C LCD displays the current temperature, setpoint, and relay state.

## Features

- Hysteresis control to prevent rapid relay switching
- Live LCD display showing temperature, setpoint, and relay state
- UP/DOWN buttons to adjust the setpoint at runtime (no recompile needed)
- Safety cutoff — relay forced off if temperature exceeds a configurable limit
- Sensor error detection — distinguishes between no sensor at startup and sensor lost mid-run
- Non-blocking loop — no `delay()` calls
- Watchdog timer resets the MCU if the loop stalls

## Hardware

| Component | Part |
|---|---|
| Microcontroller | Arduino Uno or Nano |
| Temperature sensor | DS18B20 (OneWire) |
| Relay module | Active-HIGH, 5V coil |
| Display | 16x2 I2C LCD (HD44780 + PCF8574 backpack) |
| Resistor | 4.7kΩ pull-up on DS18B20 data line |
| Buttons | 2× momentary push button (NO) |

## Wiring

| Component | Arduino Pin |
|---|---|
| DS18B20 data | D2 |
| DS18B20 VCC | 5V |
| DS18B20 GND | GND |
| DS18B20 pull-up | 4.7kΩ between D2 and 5V |
| Relay signal | D4 |
| LCD SDA | A4 |
| LCD SCL | A5 |
| UP button | D5 → GND |
| DOWN button | D6 → GND |

## Libraries

Install via Arduino Library Manager:

| Library | Author |
|---|---|
| `DallasTemperature` | Miles Burton |
| `OneWire` | Jim Studt et al. |
| `LiquidCrystal_I2C` | Frank de Brabander |

## Configuration

All settings are `#define` constants at the top of `Thermostat/Thermostat.ino`:

| Constant | Default | Description |
|---|---|---|
| `ONE_WIRE_BUS` | `2` | DS18B20 data pin |
| `RELAY_PIN` | `4` | Relay signal pin |
| `SETPOINT_DEFAULT` | `24.0` | Initial setpoint (°C) — adjustable at runtime via buttons |
| `SETPOINT_STEP` | `0.5` | °C change per button press |
| `SETPOINT_MIN` | `5.0` | Lowest permitted setpoint (°C) |
| `SETPOINT_MAX` | `29.0` | Highest permitted setpoint (°C) |
| `BTN_UP` | `5` | UP button pin |
| `BTN_DOWN` | `6` | DOWN button pin |
| `HYSTERESIS` | `1.0` | ±°C band — relay holds state within this range |
| `MAX_SAFE_TEMP` | `30.0` | Safety cutoff — relay forced off above this |
| `LCD_ADDRESS` | `0x27` | I2C address — try `0x3F` if display doesn't respond |
| `READ_INTERVAL` | `1000` | Milliseconds between temperature reads |

## Display Layout

```
Temp:  23.5°C
Set :  24.0°C  ON
```

| State | Line 0 | Line 1 |
|---|---|---|
| Normal | `Temp: XX.X°C` | `Set : XX.X°C  ON/OFF` |
| No sensor at boot | `No Sensor!` | `Set : XX.X°C  OFF` |
| Sensor lost mid-run | `Sensor Lost!` | `Set : XX.X°C  OFF` |
| Safety cutoff | `Temp: XX.X°C` | `CUTOFF        ON/OFF` |

## Relay Logic

- Relay turns **ON** when temperature drops below `setpoint - HYSTERESIS`
- Relay turns **OFF** when temperature rises above `setpoint + HYSTERESIS`
- Within the hysteresis band, the relay holds its current state
- Relay is forced **OFF** on sensor error or safety cutoff
