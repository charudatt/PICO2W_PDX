/*
 * PICO2W PDX - Step-1 Configuration
 * Callsign: VU2UPX   Grid: MK69KE
 * Arduino-Pico core (Earle Philhower) - Pico 2W (RP2350)
 *
 * Include this file BEFORE using any of the symbols below.
 * Sketch order:  #include "config.h"  then libraries that need DISPLAY_OPTION, etc.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// COMPILE-TIME OPTIONS  (referenced throughout .ino)
// ============================================================
#define DISPLAY_OPTION       1   // 0 = OLED only, 1 = OLED + ILI9341 TFT TX log
#define SI4732_AVAILABLE     0
#define PCF8574_AVAILABLE    0
#define BEACON_MODE_ENABLE   1
#define CAT_ENABLE           1
#define WEB_UI_ENABLE        1

// ============================================================
// PIN DEFINITIONS  (matches schematic)
// ============================================================
#define PIN_SDA              4
#define PIN_SCL              5

#define PIN_ENC_A            2
#define PIN_ENC_B            3
#define PIN_ENC_SW           6

#define PIN_SW_A             7    // Mode
#define PIN_SW_B             8    // Band up
#define PIN_SW_C             9    // Band down

#define PIN_RX_TX_RELAY      10
#define PIN_SI4732_RST       11
#define PIN_RX_EN            12   // ACTIVE HIGH in RX
#define PIN_PTT              13

#define PIN_TFT_MISO         16
#define PIN_TFT_CS           17
#define PIN_TFT_SCK          18
#define PIN_TFT_MOSI         19
#define PIN_TFT_DC           20
#define PIN_TFT_RST          21
#define PIN_TOUCH_CS         22
#define PIN_TOUCH_IRQ        14
#define PIN_D_CS             15

#define PIN_ADC_AUDIO        26   // ADC0

// ============================================================
// BANDS  (BANDS[], BAND_COUNT, DEFAULT_BAND)
// ============================================================
enum BandIndex {
  BAND_80M = 0,
  BAND_40M,
  BAND_30M,
  BAND_20M,
  BAND_17M,
  BAND_15M,
  BAND_12M,
  BAND_10M,
  BAND_COUNT
};

struct BandInfo {
  const char* name;
  uint32_t ft8;
  uint32_t ft4;
  uint32_t wspr;
  uint32_t js8;
};

const BandInfo BANDS[BAND_COUNT] = {
  { "80M",  3573000UL,  3575000UL,  3568600UL,  3578000UL },
  { "40M",  7074000UL,  7047500UL,  7038600UL,  7078000UL },
  { "30M", 10136000UL, 10140000UL, 10138700UL, 10130000UL },
  { "20M", 14074000UL, 14080000UL, 14095600UL, 14078000UL },
  { "17M", 18100000UL, 18104000UL, 18104600UL, 18104000UL },
  { "15M", 21074000UL, 21140000UL, 21094600UL, 21078000UL },
  { "12M", 24915000UL, 24919000UL, 24924600UL, 24922000UL },
  { "10M", 28074000UL, 28180000UL, 28124600UL, 28078000UL }
};

// ============================================================
// MODES  (MODE_NAMES[], MODE_COUNT, DEFAULT_MODE)
// ============================================================
enum ModeIndex {
  MODE_FT8 = 0,
  MODE_FT4,
  MODE_JS8,
  MODE_WSPR,
  MODE_COUNT
};

const char* const MODE_NAMES[MODE_COUNT] = {
  "FT8", "FT4", "JS8", "WSPR"
};

// Defaults (after enums so MODE_FT8 / BAND_20M exist)
#define DEFAULT_MODE         MODE_FT8
#define DEFAULT_BAND         BAND_20M

// ============================================================
// STATION / BEACON  (MY_CALLSIGN, MY_GRID4, WSPR_*, FT8_*)
// ============================================================
#define MY_CALLSIGN          "VU2UPX"
#define MY_GRID              "MK69KE"
#define MY_GRID4             "MK69"
#define WSPR_DBM             23
#define WSPR_TX_EVERY_MIN    4
#define FT8_TX_EVERY_MIN     1
#define MY_POWER_DBM         30

// ============================================================
// Si5351  (SI5351_REF_FREQ, CORR, drive — drive expands after #include <si5351.h>)
// ============================================================
#define SI5351_REF_FREQ      25000000UL
#define SI5351_CORR_DEFAULT  0
#define SI5351_CLK0_DRIVE    SI5351_DRIVE_8MA
#define SI5351_CLK2_DRIVE    SI5351_DRIVE_8MA

// ============================================================
// WiFi / NTP / WEB  (WIFI_AP_*, NTP_*, TIMEZONE_OFFSET_SEC, WIFI_*_MAX)
// ============================================================
#define WIFI_AP_SSID         "PICO2W-PDX"
#define WIFI_AP_PASS         "pdx12345"
#define WIFI_HOSTNAME        "pico2w-pdx"
#define NTP_SERVER           "pool.ntp.org"
#define NTP_SYNC_INTERVAL_MS (5UL * 60UL * 1000UL)
#define TIMEZONE_OFFSET_SEC  (5 * 3600 + 30 * 60)   // IST
#define WIFI_SSID_MAX        32
#define WIFI_PASS_MAX        64

// ============================================================
// TIMING UI
// ============================================================
#define DEBOUNCE_MS          40
#define LONG_PRESS_MS        800
#define MENU_TIMEOUT_MS      15000

// ============================================================
// EEPROM LAYOUT
// ============================================================
#define EEPROM_SIZE          512
#define EE_ADDR_MAGIC        0
#define EE_MAGIC_VALUE       0xA5
#define EE_ADDR_CORR         4
#define EE_ADDR_BAND         8
#define EE_ADDR_MODE         9
#define EE_ADDR_FLAGS        10
#define EE_ADDR_WIFI_SSID    16
#define EE_ADDR_WIFI_PASS    50
#define EE_ADDR_WIFI_VALID   120

#endif // CONFIG_H
