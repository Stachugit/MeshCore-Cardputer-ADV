# M5Stack Cardputer-Adv Variant

This variant configures MeshCore for the M5Stack Cardputer-Adv with an external SX1262 LoRa module (DX-LR30-900M22SP or compatible Cap LoRa868).

## Hardware

- **Main Board:** M5Stack Cardputer-Adv (ESP32-S3, 8MB Flash, 8MB PSRAM)
- **LoRa Module:** DX-LR30-900M22SP (SX1262, 868-915 MHz, 22 dBm)
- **Display:** 240×135 TFT LCD
- **Keyboard:** Built-in QWERTY keyboard
- **Battery:** LiPo with voltage monitoring

## Pin Mapping

| Function | GPIO | DX-LR30 Pin |
|----------|------|-------------|
| NSS/CS   | 5    | NSS         |
| IRQ/DIO1 | 4    | DIO1        |
| RESET    | 3    | NRST        |
| BUSY     | 6    | BUSY        |
| MOSI     | 14   | MOSI        |
| MISO     | 39   | MISO        |
| SCK      | 40   | SCK         |

**Note:** RXEN and TXEN must be hard-wired (RXEN→GND, TXEN→3V3).

## Files

- `target.h` - Hardware abstraction declarations
- `target.cpp` - Radio and board initialization
- `M5CardputerBoard.h` - Board-specific class
- `platformio.ini` - Build configurations
- `test_main.cpp` - Enhanced test firmware with self-test

## Build Environments

### 1. m5stack_cardputer_repeater (Recommended)
Simple mesh repeater with display output and OTA updates.

```bash
pio run -e m5stack_cardputer_repeater
```

### 2. m5stack_cardputer_test
Test mode with radio self-test and periodic test packets.

```bash
pio run -e m5stack_cardputer_test
```

### 3. m5stack_cardputer_companion
Full-featured companion radio with USB/BLE connectivity.

```bash
pio run -e m5stack_cardputer_companion
```

## Quick Start

1. **Wire the LoRa module** according to pin mapping above
2. **Build:** `pio run -e m5stack_cardputer_repeater`
3. **Enter download mode:** Power OFF → Hold G0 → Plug USB → Release G0
4. **Flash:** `pio run -e m5stack_cardputer_repeater -t upload -t monitor`
5. **Verify:** Serial monitor should show "Radio initialized successfully!"

## Radio Configuration

- **Frequency:** 868.0 MHz (EU_868)
- **Bandwidth:** 125 kHz
- **Spreading Factor:** 11
- **Coding Rate:** 4/5
- **TX Power:** 22 dBm

To change region or parameters, edit `platformio.ini`:
```ini
-D LORA_FREQ=915.0    # For US_915
-D LORA_SF=10         # Different spreading factor
```

## Troubleshooting

**Radio Init Failed:**
- Check wiring, especially NSS (G5) and RST (G3)
- Verify 3V3 and GND connections
- Ensure BUSY (G6) and DIO1 (G4) are connected

**Upload Failed:**
- Ensure download mode entered correctly
- Try different USB cable (must support data)
- Check COM port in Device Manager

**No Display Output:**
- M5Cardputer library installed automatically
- Display should show "MESHCORE Ready!"

## Documentation

See `docs/` folder for complete documentation:
- `CARDPUTER_BUILD_GUIDE.md` - Complete build and troubleshooting guide
- `CARDPUTER_QUICKSTART.md` - Quick reference card
- `CARDPUTER_IMPLEMENTATION.md` - Technical details and design decisions
- `CARDPUTER_DELIVERABLES.md` - Complete deliverables checklist

## Serial Commands

```
help     - List all commands
info     - Show node information
stats    - Display mesh statistics
advert   - Send advertisement packet
```

## Support

For issues or questions:
1. Check the documentation in `docs/` folder
2. Review serial output for error messages
3. Enable debug flags in `platformio.ini`
4. Verify hardware wiring against pin tables

## License

See `license.txt` in the root directory.
