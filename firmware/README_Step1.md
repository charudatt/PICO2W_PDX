# PICO2W PDX – Step 1

Standalone digital-modes foundation for the PICO 2W based transceiver.

## Features implemented

- 1.3" SH1106 OLED (U8x8) – Mode + IST time, frequency, RX/TX + band, IP
- Si5351 (CLK0 = LO, CLK2 = TX) with EEPROM-stored correction factor
- Rotary encoder + 3 switches (debounce + long-press)
- Switch A = Mode cycle / Long-press = Save in menu/cal
- Switch B/C = Band Up/Down / Cancel menu
- Encoder long-press = enter Menu (correction adjust)
- Calibration mode (10 MHz on CLK1)
- WiFi (STA with fallback AP) + captive DNS
- Simple web UI with soft buttons + live status + I2C device list + corr editor
- NTP time sync (IST) every 5 minutes when connected
- CAT skeleton (Kenwood TS-480 style) over USB Serial
- Beacon mode stubs ready for FT8 / WSPR
- Rx EN (GP12) and Tx/Rx relay (GP10) control
- Default: FT8 on 20 m, callsign VU2UPX / MK69KE

## Hardware (from your schematic + pin table)

| Function          | Pico GPIO |
|-------------------|-----------|
| I2C SDA / SCL     | 4 / 5     |
| Enc A / B / SW    | 2 / 3 / 6 |
| SW A / B / C      | 7 / 8 / 9 |
| Rx/Tx Relay       | 10        |
| Si4732 RST (opt)  | 11        |
| Rx EN (active H)  | 12        |
| PTT (opt)         | 13        |
| ADC Audio         | 26 (ADC0) |

Si5351 on I2C, OLED on I2C (usually 0x3C).

## Arduino IDE setup

1. Board: **Raspberry Pi Pico 2W** (Earle Philhower core)
2. USB Stack: **Adafruit TinyUSB** (required later for audio; Serial works with either)
3. Libraries:
   - Etherkit Si5351
   - U8g2 (provides U8x8)
   - (WiFi / WebServer / EEPROM are built-in)

## Compile & upload

Open `PICO2W_PDX_Step1.ino` (keep `config.h` in the same folder).  
Upload. Open Serial Monitor at 115200.

On first boot you should see:

```
=== PICO2W PDX Step-1 ===
Call: VU2UPX  Grid: MK69KE
Si5351 OK
...
I2C: 0x3C (OLED) 0x60 (Si5351) ...
```

OLED shows Mode, IST time, frequency, RX/TX status and IP.

## Web UI

- If the board joins your WiFi → browse to the printed IP.
- If not → it starts AP `PICO2W-PDX` / `pdx12345` → connect and open `http://192.168.4.1`.

Soft buttons emulate the three switches + Tune + Menu.  
You can also edit and save the Si5351 correction factor.

## Next steps (after you test Step-1)

1. Real FT8 / WSPR symbol generation + timing (beacon + CAT TX).
2. USB Audio (RX audio to PC + TX tones from WSJT-X) using pschatzmann TinyUSB Audio branch.
3. TFT (ILI9341 portrait) for decoded FT8 messages (DISPLAY_OPTION = 1).
4. Optional Si4732 path and PCF8574 filter switching.
5. Full menu system + touch support.

## USB Audio note

Arduino-Pico’s built-in Adafruit TinyUSB does not yet include a complete UAC2 Audio class.
The working approach used by the QP-7C earlephilhower port is:

https://github.com/pschatzmann/Adafruit_TinyUSB_Arduino/tree/Audio

Install that library (it overrides the core one). Then we can add a proper USB headset interface for WSJT-X in Step 2.

## Files

- `config.h` – all pins, options, bands, callsign
- `PICO2W_PDX_Step1.ino` – main sketch
- `README_Step1.md` – this file
