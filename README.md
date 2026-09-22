# PICO-2W PDX

**Digital modes transceiver / beacon platform** based on the **Raspberry Pi Pico 2W (RP2350)**.

Station example: **VU2UPX** · Grid **MK69KE**

Inspired by the ADX-style Si5351 digital transceiver concept, implemented with the [Earle Philhower Arduino-Pico core](https://github.com/earlephilhower/arduino-pico).

---

## Step-1 (current milestone)

Standalone **FT8 / WSPR beacon** platform with:

| Feature | Detail |
|--------|--------|
| MCU | Raspberry Pi Pico 2W (RP2350) |
| RF | Si5351 — CLK0 LO, CLK1 CAL 10 MHz, CLK2 TX |
| Display | 1.3" SH1106 OLED (primary) + optional ILI9341 TFT TX log |
| Controls | Rotary encoder + 3 switches |
| Network | WiFi Manager (STA/AP), NTP, web UI |
| Beacons | FT8 (once/min) + WSPR Type 1 — **Beacon default OFF** (web toggle) |
| Time | IST on OLED/TFT; UTC for slot timing |

### Firmware

```
firmware/
  PICO2W_PDX_Step1.ino   # main sketch
  config.h               # callsign, pins, bands, WiFi AP, intervals
  PICO2W_PDX_ILI9341.h   # TFT_eSPI User_Setup (copy to library User_Setups)
  TFT_SETUP_NOTE.txt
  README_Step1.md
```

### Docs & manual tooling

```
docs/                    # User manual .docx
manual_build/            # Automated manual versioning (build_manual.py)
hardware/                # Schematics / photos (add your files here)
```

---

## Quick start

1. Install **Arduino IDE 2.x** and **Arduino-Pico** board package (Philhower).
2. Select board: **Raspberry Pi Pico 2W**, USB stack: **Pico SDK**.
3. Libraries: **Etherkit Si5351**, **Etherkit JTEncode**, **U8g2**, **TFT_eSPI** (if `DISPLAY_OPTION 1`).
4. For TFT: copy `firmware/PICO2W_PDX_ILI9341.h` into `TFT_eSPI/User_Setups/` and select it in `User_Setup_Select.h`.
5. Edit `firmware/config.h` (callsign, grid, WiFi AP password, `WSPR_DBM`, etc.).
6. Open `firmware/PICO2W_PDX_Step1.ino`, upload.
7. Join AP `PICO2W-PDX` if needed, set home WiFi, enable **Beacon ON** from the web page when ready.

Serial: **115200 baud**.

---

## Web UI

- AP portal: typically `http://192.168.4.1`
- After STA join: IP shown on OLED
- Soft buttons: Mode, Band, Tune/PTT, Menu/Cal, **Beacon ON/OFF**

---

## Calibration

Long-press **encoder switch** → 10 MHz on **CLK1** → adjust correction with encoder → long-press **Switch A** to save EEPROM.

See the User Manual in `docs/` for the full procedure.

---

## Manual versioning

```bash
cd manual_build
python3 build_manual.py --check
python3 build_manual.py
bash bump_version.sh patch "Your note"
```

---

## Roadmap

| Step | Focus |
|------|--------|
| **Step-1** | Beacon platform (this repo milestone) |
| Step-2 | USB Audio + CAT for WSJT-X |
| Step-3 | DC RX path, ADC audio |
| Step-4 | Filters / ATU |
| Step-5 | JS8/FT4, dual-display polish |

---

## License

Hardware and documentation: share freely for amateur radio use.  
Respect library licenses (JTEncode, Si5351, TFT_eSPI, U8g2).

## Author

VU2UPX — PICO-2W PDX project
