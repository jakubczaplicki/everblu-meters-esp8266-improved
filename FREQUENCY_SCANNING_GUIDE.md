# Frequency Scanning Guide

## Overview

This enhanced version includes **intelligent frequency scanning** to address frequency drift issues common in aging water meters. Your log showed classic signs of frequency drift - the meter has likely shifted from its original 433.700007 MHz frequency.

## How It Works

### **Automatic Frequency Discovery**
1. **First attempt**: Tries your configured frequency (433.700007 MHz)
2. **Smart scanning**: If that fails, scans ±25 kHz in 2.5 kHz steps
3. **Signal validation**: Filters false triggers and noise
4. **Adaptive learning**: Logs successful frequencies for future use

### **Frequency Range Tested**
- **Base**: 433.700007 MHz (your current setting)
- **Range**: 433.675007 MHz to 433.725007 MHz  
- **Step size**: 2.5 kHz (21 frequencies total)
- **Test order**: Base frequency first, then alternating higher/lower

## New Features Added

### **Enhanced Signal Validation**
- Filters out false GDO0 triggers (fixes "Very fast GDO0 trigger detected")
- Validates signal stability over time
- Checks noise floor before attempting communication

### **MQTT Frequency Reporting**
- New topic: `everblu/cyble/successful_frequency`
- Shows which frequency worked for each reading
- Helps track frequency drift over time

### **Detailed Logging**
```
--- Testing frequency 433.702507 MHz (attempt 3/21) ---
Initial noise floor: -95 dBm
✅ SUCCESS! Frequency 433.702507 MHz worked!
Data received: 12450 liters, counter 15, RSSI -78 dBm, LQI 180
🎯 RECOMMENDATION: Update FREQUENCY in private.h to 433.702507
```

## Usage Instructions

### **1. Flash the Updated Code**
```bash
pio run --target upload
pio device monitor
```

### **2. Trigger a Reading**
Via Home Assistant or MQTT:
```bash
mosquitto_pub -h YOUR_MQTT_BROKER -t "everblu/cyble/trigger" -m "update"
```

### **3. Monitor the Results**

**Expected Output:**
```
Attempting meter reading with configured frequency...
GDO0 signal detected after 0ms
WARNING: Very fast GDO0 trigger detected - validating signal quality...
❌ Signal validation failed - appears to be noise/glitch
🔍 Standard frequency failed - starting intelligent frequency scan...

--- Testing frequency 433.700007 MHz (attempt 1/21) ---
❌ No success at 433.700007 MHz (RSSI: -110 dBm)

--- Testing frequency 433.702507 MHz (attempt 2/21) ---
✅ SUCCESS! Frequency 433.702507 MHz worked!
📡 Published successful frequency: 433.702507 MHz to MQTT
```

### **4. Update Configuration (If Successful)**

If a new frequency is found, update your `src/private.h`:
```cpp
#define FREQUENCY 433.702507  // Use the discovered frequency
```

## Troubleshooting

### **No Frequency Found**
If scanning fails completely:
```
❌ No frequency in the tested range provided valid meter data
📋 TROUBLESHOOTING SUGGESTIONS:
   1. Check CC1101 wiring and antenna connection
   2. Ensure meter is in wake window (business hours on weekdays)
   3. Reduce distance between CC1101 and water meter
   4. Try a broader frequency range scan
```

### **Partial Success**
```
⚠️  Partial success at 433.705007 MHz - good signal but no data (RSSI: -85 dBm)
   This might be close to the correct frequency
```
This suggests the frequency is close - try manually testing nearby frequencies.

### **Hardware Issues**
- **Poor RSSI** (<-100 dBm): Move CC1101 closer to meter or check antenna
- **Fast GDO0 triggers**: Check wiring, may have electromagnetic interference
- **No GDO0 signals**: Verify CC1101 connections and power supply

## Performance

- **Time**: 2-3 minutes for full frequency scan
- **Memory**: Static buffers prevent stack overflow
- **Reliability**: Enhanced signal validation reduces false positives
- **Learning**: Successful frequencies logged to MQTT for tracking

## Home Assistant Integration

Monitor frequency drift in Home Assistant:
```yaml
sensor:
  - platform: mqtt
    name: "Water Meter Frequency"
    state_topic: "everblu/cyble/successful_frequency" 
    unit_of_measurement: "MHz"
    icon: mdi:signal
```

This will help you track when your meter's frequency changes and update your configuration accordingly.
