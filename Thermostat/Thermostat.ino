#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
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
  digitalWrite(RELAY_PIN, RELAY_OFF); // start with relay off
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  updateLCD(0.0);                     // show initial state before first read
  wdt_enable(WDTO_4S);               // reset MCU if loop stalls for 4 seconds
}

void loop() {
  wdt_reset();

  uint32_t now = millis();

  // ── Button polling (debounced) ──────────────────────────────────────────────
  if (now - lastBtnTime >= BTN_DEBOUNCE_MS) {
    if (digitalRead(BTN_UP) == LOW) {
      setpointC = min(setpointC + SETPOINT_STEP, (float)SETPOINT_MAX);
      lastBtnTime = now;
      Serial.print(F("Setpoint: "));
      Serial.print(setpointC, 1);
      Serial.println(F(" C"));
      updateLCD(lastTemp);
    } else if (digitalRead(BTN_DOWN) == LOW) {
      setpointC = max(setpointC - SETPOINT_STEP, (float)SETPOINT_MIN);
      lastBtnTime = now;
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
    relayOn = false;
    sensorError = true;
    digitalWrite(RELAY_PIN, RELAY_OFF);  // fail-safe: cut heat
    if (sensorOk) {
      Serial.println(F("ERROR: sensor disconnected - relay forced OFF"));
    } else {
      Serial.println(F("ERROR: no sensor detected - retrying scan"));
      sensors.begin();  // re-scan bus in case sensor was just connected
    }
    updateLCD(0.0);
    return;
  }

  sensorOk  = true;
  sensorError = false;
  lastTemp  = tempC;  // store for immediate LCD refresh on button press

  if (tempC >= MAX_SAFE_TEMP) {
    relayOn = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
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
    relayOn = desired;
    digitalWrite(RELAY_PIN, relayOn ? RELAY_ON : RELAY_OFF);
    Serial.print(F("Relay "));
    Serial.println(relayOn ? F("ON") : F("OFF"));
  }

  updateLCD(tempC);
}
