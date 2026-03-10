#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <avr/wdt.h>

// ── Configuration ─────────────────────────────────────────────────────────────
#define ONE_WIRE_BUS     2       // DS18B20 data pin
#define RELAY_PIN        4       // Relay signal pin
#define SETPOINT_DEFAULT 24.0   // Initial setpoint in °C
#define SETPOINT_STEP    0.5    // °C per button press
#define SETPOINT_MIN     5.0    // Lowest permitted setpoint in °C
#define SETPOINT_MAX     29.0   // Highest permitted setpoint (below MAX_SAFE_TEMP)
#define BTN_UP           5      // Up button pin (button to GND, INPUT_PULLUP)
#define BTN_DOWN         6      // Down button pin (button to GND, INPUT_PULLUP)
#define BTN_DEBOUNCE_MS  50UL   // Debounce window in ms
#define HYSTERESIS       1.0    // ±°C band around setpoint
#define MAX_SAFE_TEMP    30.0   // Hard safety cutoff — relay forced off above this
#define READ_INTERVAL    1000UL // ms between temperature reads
#define CONVERSION_MS    750UL  // DS18B20 conversion time at 12-bit resolution
#define LCD_ADDRESS      0x27   // I2C address; try 0x3F if display doesn't respond
#define EEPROM_ADDR      0      // EEPROM address for persisted setpoint (uses 4 bytes)
// ─────────────────────────────────────────────────────────────────────────────

// Active-HIGH relay: HIGH = coil energized (heating ON), LOW = coil released
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

bool     sensorOk            = false;  // set true after first valid reading
bool     sensorError         = false;  // true when most recent read was invalid
bool     conversionRequested = false;
uint32_t lastRequest         = 0;
bool     relayOn             = false;
float    setpointC           = SETPOINT_DEFAULT;  // runtime-adjustable setpoint
float    lastTemp            = 0.0;               // most recent valid temperature
uint32_t lastBtnTime         = 0;                 // debounce timestamp
bool     btnUpArmed          = true;              // ready to accept next UP press
bool     btnDownArmed        = true;              // ready to accept next DOWN press

// Keeps relayOn state variable and hardware pin always in sync
void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

void updateLCD(float tempC) {
  char buf[8];
  // 3-char relay strings, right-aligned at columns 13-15 of line 1
  const char* relayStr = relayOn ? " ON" : "OFF";

  // Line 0: temperature or sensor error message
  // "Temp:" (5) + buf (5) + °C (2) + "    " (4) = 16 chars
  lcd.setCursor(0, 0);
  if (sensorError) {
    lcd.print(sensorOk ? F("Sensor Lost!    ") : F("No Sensor!      "));
  } else {
    dtostrf(tempC, 5, 1, buf);
    lcd.print(F("Temp:"));
    lcd.print(buf);
    lcd.print((char)0xDF);  // degree symbol
    lcd.print(F("C    "));
  }

  // Line 1: setpoint + relay state, or cutoff warning
  // "Set :" (5) + buf (5) + °C (2) + " " (1) + relay (3) = 16 chars
  // "CUTOFF" (6) + spaces (7 or 8) + relay (3) = 16 chars
  lcd.setCursor(0, 1);
  if (!sensorError && tempC >= MAX_SAFE_TEMP) {
    lcd.print(relayOn ? F("CUTOFF        ON") : F("CUTOFF       OFF"));
  } else {
    dtostrf(setpointC, 5, 1, buf);
    lcd.print(F("Set :"));
    lcd.print(buf);
    lcd.print((char)0xDF);  // degree symbol
    lcd.print(F("C "));
    lcd.print(relayStr);
  }
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  sensors.setWaitForConversion(false); // async: don't block during conversion
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);                     // start with relay off
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  // Load persisted setpoint from EEPROM; fall back to default if out of range
  float saved;
  EEPROM.get(EEPROM_ADDR, saved);
  if (saved >= SETPOINT_MIN && saved <= SETPOINT_MAX) {
    setpointC = saved;
    Serial.print(F("Setpoint loaded from EEPROM: "));
    Serial.print(setpointC, 1);
    Serial.println(F(" C"));
  } else {
    Serial.println(F("EEPROM setpoint invalid — using default"));
  }

  lcd.init();
  lcd.backlight();
  updateLCD(0.0);                     // show initial state before first read
  wdt_enable(WDTO_4S);               // reset MCU if loop stalls for 4 seconds
}

void loop() {
  wdt_reset();

  uint32_t now = millis();

  // ── Button polling (armed + debounced press and release) ───────────────────
  if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
    bool curBtnUp   = digitalRead(BTN_UP);
    bool curBtnDown = digitalRead(BTN_DOWN);

    // Re-arm when button is fully released; restart debounce so release
    // bounce cannot immediately trigger a new press
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
      setpointC   = min(setpointC + SETPOINT_STEP, (float)SETPOINT_MAX);
      EEPROM.put(EEPROM_ADDR, setpointC);
      Serial.print(F("Setpoint: "));
      Serial.print(setpointC, 1);
      Serial.println(F(" C"));
      updateLCD(lastTemp);
    } else if (curBtnDown == LOW && btnDownArmed) {
      btnDownArmed = false;
      lastBtnTime  = now;
      setpointC    = max(setpointC - SETPOINT_STEP, (float)SETPOINT_MIN);
      EEPROM.put(EEPROM_ADDR, setpointC);
      Serial.print(F("Setpoint: "));
      Serial.print(setpointC, 1);
      Serial.println(F(" C"));
      updateLCD(lastTemp);
    }
  }
  // ───────────────────────────────────────────────────────────────────────────

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
    sensorError = true;
    setRelay(false);  // fail-safe: cut heat
    if (sensorOk) {
      Serial.println(F("ERROR: sensor disconnected - relay forced OFF"));
    } else {
      Serial.println(F("ERROR: no sensor detected - retrying scan"));
      sensors.begin();  // re-scan bus in case sensor was just connected
    }
    updateLCD(0.0);
    return;
  }

  sensorOk    = true;
  sensorError = false;
  lastTemp    = tempC;  // store for immediate LCD refresh on button press

  if (tempC >= MAX_SAFE_TEMP) {
    setRelay(false);
    Serial.println(F("SAFETY CUTOFF: max temperature exceeded - relay forced OFF"));
    updateLCD(tempC);
    return;
  }

  Serial.print(F("Temp: "));
  Serial.print(tempC, 1);
  Serial.println(F(" C"));

  bool desired;
  if (tempC < setpointC - HYSTERESIS) {
    desired = true;
  } else if (tempC > setpointC + HYSTERESIS) {
    desired = false;
  } else {
    desired = relayOn;  // within hysteresis band: maintain current state
  }

  if (desired != relayOn) {
    setRelay(desired);
    Serial.print(F("Relay "));
    Serial.println(relayOn ? F("ON") : F("OFF"));
  }

  updateLCD(tempC);
}
