# 🔥 MeshCore-Cardputer-ADV 🔥

[![Buy Me a Coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=☕&slug=Stachu&button_colour=ff8800&font_colour=000000&font_family=Lato&outline_colour=000000&coffee_colour=FFDD00)](https://buymeacoffee.com/Stachu)

Enhanced TFT user interface for MeshCore mesh networking firmware, optimized for M5Stack Cardputer-Adv.

![MeshCore-Cardputer-ADV](docs/images/title.JPG)

## 📦 Installation via M5Burner

Search in M5Burner for:
- `MeshCore-Cardputer-ADV M5Stack Cap LoRa868 version!!!!` - **Plug-and-play** (Recommended)
- `MeshCore-Cardputer-ADV DX-LR30 version` - Custom module (check wiring below)

## 🔧 Two Hardware Options

### Option 1: M5Stack Cap LoRa868 (Recommended)
Simply attach the Cap LoRa868 to your Cardputer-Adv - no wiring needed!
- **Module**: RA-01SH (SX1262)
- **Frequency**: 863-870 MHz
- **Docs**: [Cap LoRa868](https://docs.m5stack.com/en/cap/Cap_LoRa868)

### Option 2: DX-LR30-900M22SP (Custom Build)
For advanced users - requires custom wiring:

![LoRa Module](docs/images/lora-module.jpg)

| Module Pin | Cardputer GPIO | Description |
|------------|----------------|-------------|
| VCC        | 3V3            | Power (3.3V only!) |
| GND        | GND            | Ground |
| NSS        | G5             | Chip Select |
| NRST       | G3             | Reset |
| MOSI       | G14            | SPI MOSI |
| SCK        | G40            | SPI Clock |
| DIO1       | G4             | Interrupt |
| MISO       | G39            | SPI MISO |
| BUSY       | G6             | Busy signal |
| RXEN       | G13            | RX Enable |
| TXEN       | G15            | TX Enable |

**⚠️ CRITICAL**: Module requires 3.3V! Use voltage converter or [3.3V mod](https://www.reddit.com/r/CardPuter/comments/1pjlkby/cardputer_adv_33v_mod/). Using 5V will permanently damage the module!

## ✨ Features

### Interface
- **Chat bubbles** with sender names
- **150-character limit** with counter
- **Notification popups** for new messages
- **Message scrolling** (FN+UP/DOWN)
- **18 color themes** with brightness control
- **Real-time search** in contacts/channels

### Controls
- **`;`** - Up | **`.`** - Down | **`,`** - Left/Contacts | **`/`** - Right/Channels
- **Enter** - Send/Select | **Backspace** - Delete | **Hold Backspace** - Clear all
- **FN+`** or **OPT** - Back/Exit
- **FN+;** / **FN+.** - Scroll messages

### Settings Menu
- Access via **☰** icon (top-left)
- Adjust brightness and colors
- Settings persist across restarts

## 🚀 Initial Setup

**Important**: First-time configuration requires the MeshCore mobile app:
1. Flash firmware to Cardputer-Adv
2. Download MeshCore app on smartphone
3. Connect via Bluetooth (use the code on the top right corner of the screen for pairing)
4. Configure node name, region, network keys, channels

## 📁 Pre-compiled Binaries

Download from [Releases](https://github.com/Stachugit/MeshCore-Cardputer-ADV/releases):
- `firmware_Cap_LoRa868.bin` - For Cap LoRa868
- `firmware_DX-LR30.bin` - For custom module

Flash using esptool.py, ESP Flash Download Tool, or web flasher.

## 🛠️ Building from Source

```bash
git clone https://github.com/Stachugit/MeshCore-Cardputer-ADV.git
cd MeshCore-Cardputer-ADV

# For Cap LoRa868:
pio run -e CardputerADV_keyboard_UI_Cap_LoRa868 --target upload

# For DX-LR30:
pio run -e CardputerADV_keyboard_UI --target upload
```

## 🙏 Credits

Based on [MeshCore](https://github.com/meshcore-dev/MeshCore) mesh networking firmware. This project adds custom TFT UI, chat bubbles, theme system, and enhanced keyboard navigation.
Cap LoRa868 compatibility fixes based on work by [sosprz](https://github.com/sosprz/meshcore-cardputer-adv).

## 📜 License

Same license as original MeshCore firmware. See [license.txt](license.txt).

## 🤝 Contributing

Contributions welcome! Report bugs, suggest features, submit PRs, or improve documentation.

## 🔗 Links

- **Original MeshCore**: https://github.com/meshcore-dev/MeshCore
- **M5Stack Cardputer-ADV**: https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3
- **Cap LoRa868**: https://docs.m5stack.com/en/cap/Cap_LoRa868
- **3.3V Mod Guide**: https://www.reddit.com/r/CardPuter/comments/1pjlkby/cardputer_adv_33v_mod/

## ⚠️ Disclaimer

Independent UI modification of MeshCore. For core networking questions, refer to original project. **Ensure proper 3.3V voltage when using custom LoRa modules to avoid permanent damage.**

---

**Version**: 1.0.0 | **Last Updated**: January 12, 2026
