---
status: complete
priority: p2
issue_id: "005"
tags: [code-review, performance, memory]
---

# SRAM and Serial Optimisation

## Problem Statement

On the ATmega328P (2KB SRAM), string literals passed to `Serial.print()` are copied into SRAM at startup by the C runtime. The current sketch uses ~50 bytes of SRAM just for error/log strings. Additionally, 9600 baud means each print call blocks for ~14ms while the TX buffer drains.

## Proposed Solution

### 1. Use F() macro to keep strings in flash

```cpp
// Before:
Serial.println("ERROR: sensor disconnected");
Serial.print("Temp: ");

// After:
Serial.println(F("ERROR: sensor disconnected - relay forced OFF"));
Serial.print(F("Temp: "));
```

This reads strings directly from flash at print time — zero SRAM cost.

### 2. Increase baud rate to 115200

```cpp
// Before:
Serial.begin(9600);

// After:
Serial.begin(115200);
```

Reduces per-cycle serial blocking from ~14ms to ~1ms. No other changes required. Remember to match the baud rate in the Serial Monitor.

## Acceptance Criteria

- [x] All string literals wrapped in `F()`
- [x] Baud rate updated to 115200 in both code and Serial Monitor
- [x] SRAM savings verified (Arduino IDE shows memory usage after compile)
