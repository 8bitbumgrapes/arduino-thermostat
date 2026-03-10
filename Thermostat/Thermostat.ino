#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>

// ── Configuration ─────────────────────────────────────────────────────────────
#define ONE_WIRE_BUS  2       // DS18B20 data pin
#define RELAY_PIN     4       // Relay signal pin
#define SETPOINT_C    24.0    // Target temperature in °C
#define HYSTERESIS    1.0     // ±°C band around setpoint
#define MAX_SAFE_TEMP 30.0    // hard safety cutoff — relay forced off above this
#define READ_INTERVAL 1000UL  // ms between temperature reads
#define CONVERSION_MS 750UL   // DS18B20 conversion time at 12-bit resolution
// ─────────────────────────────────────────────────────────────────────────────

// Active-HIGH relay: HIGH = coil energized (heating ON), LOW = coil released
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

bool sensorOk = false;            // set true after first valid reading
bool conversionRequested = false; // tracks async conversion state
uint32_t lastRequest = 0;         // millis() timestamp of last conversion request
bool relayOn = false;             // tracks current relay state

void setup() {
  Serial.begin(115200);
  sensors.begin();
  sensors.setWaitForConversion(false); // async: don't block during conversion
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF); // start with relay off
  wdt_enable(WDTO_4S);               // reset MCU if loop stalls for 4 seconds
}

void loop() {
  wdt_reset();

  uint32_t now = millis();

  // Request a new conversion every READ_INTERVAL ms
  if (!conversionRequested && now - lastRequest >= READ_INTERVAL) {
    sensors.requestTemperatures();
    lastRequest = now;
    conversionRequested = true;
  }

  // Read result once conversion time has elapsed
  if (!conversionRequested || now - lastRequest < CONVERSION_MS) return;
  conversionRequested = false;

  float tempC = sensors.getTempCByIndex(0);

  if (tempC < -55.0 || tempC > 125.0) {
    relayOn = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);  // fail-safe: cut heat
    if (sensorOk) {
      Serial.println(F("ERROR: sensor disconnected - relay forced OFF"));
    } else {
      Serial.println(F("ERROR: no sensor detected - retrying scan"));
      sensors.begin();  // re-scan bus in case sensor was just connected
    }
    return;
  }

  sensorOk = true;

  if (tempC >= MAX_SAFE_TEMP) {
    relayOn = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println(F("SAFETY CUTOFF: max temperature exceeded - relay forced OFF"));
    return;
  }

  Serial.print(F("Temp: "));
  Serial.print(tempC, 1);
  Serial.println(F(" C"));

  bool desired;
  if (tempC < SETPOINT_C - HYSTERESIS) {
    desired = true;
  } else if (tempC > SETPOINT_C + HYSTERESIS) {
    desired = false;
  } else {
    desired = relayOn;  // within hysteresis band: maintain current state
  }

  if (desired != relayOn) {
    relayOn = desired;
    digitalWrite(RELAY_PIN, relayOn ? RELAY_ON : RELAY_OFF);
    Serial.print(F("Relay "));
    Serial.println(relayOn ? F("ON") : F("OFF"));
  }
}
