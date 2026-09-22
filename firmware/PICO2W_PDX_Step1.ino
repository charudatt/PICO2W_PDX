/*
 * PICO2W PDX - Digital Modes Transceiver  (Step-1)
 * Raspberry Pi Pico 2W (RP2350) + Arduino-Pico core (Earle Philhower)
 *
 * Step-1 features:
 *  - 1.3" SH1106 OLED primary display (U8x8)
 *  - Si5351 (CLK0 = LO, CLK2 = TX)
 *  - Rotary encoder + 3 switches with debounce / long-press
 *  - WiFi Manager + simple web UI (status + soft buttons)
 *  - NTP time (IST) with periodic re-sync
 *  - CAT skeleton (Kenwood TS-480 style) over USB Serial
 *  - FT8 + WSPR beacon TX (JTEncode, non-blocking)
 *  - Rx EN / Tx-Rx relay control
 *  - Calibration entry (10 MHz on CLK1)
 *
 * USB Audio: use Adafruit TinyUSB with Audio support
 *   (pschatzmann fork recommended under Arduino-Pico).
 *   For pure CAT + later audio, Serial is already available.
 *
 * Libraries required:
 *   - Etherkit Si5351
 *   - U8g2 (U8x8)
 *   - WiFi (built-in Pico W / 2W)
 *   - WebServer / DNSServer (built-in)
 *   - EEPROM (emulated)
 */

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <time.h>

#include "config.h"

#include <si5351.h>
#include <JTEncode.h>
#include <U8x8lib.h>
#if DISPLAY_OPTION == 1
#include <SPI.h>
#include <TFT_eSPI.h>
#endif
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// ============================================================
// GLOBALS
// ============================================================
Si5351 si5351;
JTEncode jtencode;
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

#if DISPLAY_OPTION == 1
// ----- TFT / TX log runtime declarations -----
TFT_eSPI tft = TFT_eSPI();
#ifndef TFT_LOG_LINES
#define TFT_LOG_LINES   16
#endif
#ifndef TFT_LOG_LEN
#define TFT_LOG_LEN     48
#endif
char     tftLog[TFT_LOG_LINES][TFT_LOG_LEN];
uint8_t  tftLogWrite = 0;   // next write index in circular buffer
uint8_t  tftLogCount = 0;   // valid entries (0 .. TFT_LOG_LINES)
bool     tftLogDirty = true;
#endif

WebServer server(80);
DNSServer dnsServer;

int32_t si5351_corr = SI5351_CORR_DEFAULT;
uint8_t currentBand = DEFAULT_BAND;
uint8_t currentMode = DEFAULT_MODE;
bool isTransmitting = false;
bool inCalibration = false;
bool inMenu = false;
bool wifiConnected = false;
bool apMode = false;
bool timeValid = false;
bool haveWifiCreds = false;
String wifiIP = "-";
String apIP = "0.0.0.0";
char storedSSID[WIFI_SSID_MAX + 1] = {0};
char storedPASS[WIFI_PASS_MAX + 1] = {0};

unsigned long lastNtpSync = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonCheck = 0;

// Encoder / switches state
volatile int encoderPos = 0;

// FT8 beacon
#define FT8_SYMBOL_COUNT   79
#define FT8_TONE_SPACING   625     // 6.25 Hz in 0.01 Hz units
#define FT8_SYMBOL_MS      160
#define FT8_SLOT_SEC       15
uint8_t ft8_symbols[FT8_SYMBOL_COUNT];
bool ft8_encoded = false;
uint8_t ft8_sym_index = 0;
unsigned long ft8_sym_start_ms = 0;
int lastFt8Slot = -1;

// WSPR beacon
#define WSPR_SYMBOL_COUNT  162
#define WSPR_TONE_SPACING  146     // ~1.46 Hz in 0.01 Hz units
#define WSPR_SYMBOL_MS     683
uint8_t wspr_symbols[WSPR_SYMBOL_COUNT];
bool wspr_encoded = false;
uint8_t wspr_sym_index = 0;
unsigned long wspr_sym_start_ms = 0;
int lastWsprSlot = -1;

bool beaconTxActive = false;
bool beaconEnabled = false;   // web toggle; default OFF

int lastEncA = HIGH;
bool swA_pressed = false, swB_pressed = false, swC_pressed = false;
bool encSw_pressed = false;
unsigned long swA_down = 0, swB_down = 0, swC_down = 0, encSw_down = 0;

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
void setupPins();
void setupSi5351();
void setupOLED();
#if DISPLAY_OPTION == 1
void setupTFT();
void tftLogAdd(const char* line);
void tftLogTxEvent();
void tftLogRedraw();
void tftFormatTxLine(char* out, size_t outLen);
#endif
void setupEEPROM();
void loadSettings();
void saveSettings();
void setupWiFi();
void setupWebServer();
void updateDisplay();
void handleButtons();
void handleEncoder();
void setFrequency(uint32_t freq);
void setRxMode();
void setTxMode();
void startCalibration();
void stopCalibration();
void applyCorrection(int32_t corr);
void processCAT();
void scanI2C();
void syncNTP();
String getTimeIST();
uint32_t getCurrentDialFreq();
void beaconTick();
void encodeFt8Message();
void encodeWsprMessage();
void stopBeaconTx();
bool getUtcTm(struct tm *t);

// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.ignoreFlowControl(true);
  delay(1500);
  Serial.println("\n=== PICO2W PDX Step-1 ===");
  Serial.println("Call: " MY_CALLSIGN "  Grid: " MY_GRID);

  setupPins();
  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();
  Wire.setClock(400000);

  setupEEPROM();
  loadSettings();

  setupSi5351();
  setupOLED();
#if DISPLAY_OPTION == 1
  setupTFT();
#endif

  setRxMode();
  setFrequency(getCurrentDialFreq());

  setupWiFi();
  if (WEB_UI_ENABLE) setupWebServer();

  scanI2C();
  encodeFt8Message();
  encodeWsprMessage();
  updateDisplay();

  Serial.println("Setup complete. Default: FT8 20m");
}

void loop() {
  handleButtons();
  handleEncoder();

  if (millis() - lastDisplayUpdate > 500) {
    updateDisplay();
#if DISPLAY_OPTION == 1
    tftLogRedraw();
#endif
    lastDisplayUpdate = millis();
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiIP = WiFi.localIP().toString();
  } else {
    wifiConnected = false;
  }
  if (haveWifiCreds && (millis() - lastNtpSync > NTP_SYNC_INTERVAL_MS || lastNtpSync == 0)) {
    syncNTP();
  }

  if (WEB_UI_ENABLE) {
    if (apMode) dnsServer.processNextRequest();
    server.handleClient();
  }

  if (CAT_ENABLE) processCAT();

  if (BEACON_MODE_ENABLE && beaconEnabled && !inCalibration && !inMenu) {
    beaconTick();
  } else if (beaconTxActive && !beaconEnabled) {
    stopBeaconTx();  // turned off mid-TX
  }
}

// ============================================================
// PINS / Si5351 / RX-TX / OLED / TFT
// ============================================================
void setupPins() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_SW_A, INPUT_PULLUP);
  pinMode(PIN_SW_B, INPUT_PULLUP);
  pinMode(PIN_SW_C, INPUT_PULLUP);
  pinMode(PIN_RX_EN, OUTPUT);
  pinMode(PIN_RX_TX_RELAY, OUTPUT);
  pinMode(PIN_PTT, INPUT_PULLUP);
  digitalWrite(PIN_RX_EN, HIGH);
  digitalWrite(PIN_RX_TX_RELAY, LOW);
  if (SI4732_AVAILABLE) {
    pinMode(PIN_SI4732_RST, OUTPUT);
    digitalWrite(PIN_SI4732_RST, HIGH);
  }
}

void setupSi5351() {
  bool ok = si5351.init(SI5351_CRYSTAL_LOAD_8PF, SI5351_REF_FREQ, si5351_corr);
  if (!ok) {
    Serial.println("Si5351 not found!");
  } else {
    Serial.println("Si5351 OK");
    si5351.set_correction(si5351_corr, SI5351_PLL_INPUT_XO);
    si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
    si5351.drive_strength(SI5351_CLK0, SI5351_CLK0_DRIVE);
    si5351.drive_strength(SI5351_CLK2, SI5351_CLK2_DRIVE);
    si5351.output_enable(SI5351_CLK0, 1);
    si5351.output_enable(SI5351_CLK1, 0);
    si5351.output_enable(SI5351_CLK2, 0);
  }
}

void setFrequency(uint32_t freq) {
  si5351.set_freq((uint64_t)freq * 100ULL, SI5351_CLK0);
  if (isTransmitting) {
    si5351.set_freq((uint64_t)freq * 100ULL, SI5351_CLK2);
  }
}

void applyCorrection(int32_t corr) {
  si5351_corr = corr;
  si5351.set_correction(si5351_corr, SI5351_PLL_INPUT_XO);
  setFrequency(getCurrentDialFreq());
  Serial.printf("New corr = %ld\n", si5351_corr);
}

void startCalibration() {
  inCalibration = true;
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK2, 0);
  si5351.set_freq(1000000000ULL, SI5351_CLK1);
  si5351.output_enable(SI5351_CLK1, 1);
  digitalWrite(PIN_RX_EN, LOW);
  digitalWrite(PIN_RX_TX_RELAY, LOW);
  Serial.println("CAL mode: 10 MHz on CLK1");
}

void stopCalibration() {
  inCalibration = false;
  si5351.output_enable(SI5351_CLK1, 0);
  setRxMode();
  setFrequency(getCurrentDialFreq());
  Serial.println("CAL exit");
}

void setRxMode() {
  isTransmitting = false;
  digitalWrite(PIN_RX_EN, HIGH);
  digitalWrite(PIN_RX_TX_RELAY, LOW);
  si5351.output_enable(SI5351_CLK2, 0);
  si5351.output_enable(SI5351_CLK0, 1);
}

void setTxMode() {
  isTransmitting = true;
  digitalWrite(PIN_RX_EN, LOW);
  digitalWrite(PIN_RX_TX_RELAY, HIGH);
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  setFrequency(getCurrentDialFreq());
}

void setupOLED() {
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.clear();
  u8x8.drawString(0, 0, "PICO2W PDX");
  u8x8.drawString(0, 2, "VU2UPX");
  delay(800);
  u8x8.clear();
}

#if DISPLAY_OPTION == 1
void tftDrawBeaconStatus() {
  // Status strip under title: BEACON ON/OFF
  tft.fillRect(0, 28, 240, 12, TFT_BLACK);
  tft.drawFastHLine(0, 28, 240, TFT_DARKGREY);
  if (beaconEnabled) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("BEACON ON", 8, 30, 1);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("BEACON OFF", 8, 30, 1);
  }
}

void setupTFT() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("PICO2W PDX  TX LOG", 8, 6, 2);
  tftLogWrite = 0;
  tftLogCount = 0;
  tftLogDirty = true;
  tftDrawBeaconStatus();
  Serial.println(F("TFT ILI9341 OK (TX-only circular log)"));
}

void tftFormatTxLine(char* out, size_t outLen) {
  time_t now = time(nullptr);
  char when[24] = "--/--/----  --:--:--";
  if (now >= 1600000000UL) {
    now += TIMEZONE_OFFSET_SEC;
    struct tm t;
    gmtime_r(&now, &t);
    // DD-MM-YYYY  HH:MM:SS  (extra space between date and time)
    snprintf(when, sizeof(when), "%02d-%02d-%04d  %02d:%02d:%02d",
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
             t.tm_hour, t.tm_min, t.tm_sec);
  }
  // Mode  Band  Date  Time — wider gaps for readability
  snprintf(out, outLen, "%-4s  %-4s  %s",
           MODE_NAMES[currentMode],
           BANDS[currentBand].name,
           when);
}

void tftLogAdd(const char* line) {
  strncpy(tftLog[tftLogWrite], line, TFT_LOG_LEN - 1);
  tftLog[tftLogWrite][TFT_LOG_LEN - 1] = 0;
  tftLogWrite = (tftLogWrite + 1) % TFT_LOG_LINES;
  if (tftLogCount < TFT_LOG_LINES) tftLogCount++;
  tftLogDirty = true;
}

void tftLogTxEvent() {
  char line[TFT_LOG_LEN];
  tftFormatTxLine(line, sizeof(line));
  tftLogAdd(line);
  Serial.println(line);
}

void tftLogRedraw() {
  if (!tftLogDirty) return;
  tftLogDirty = false;
  const int top = 44;
  const int lineH = 22;          // more vertical space so time is not cramped
  const int maxVisible = (320 - top) / lineH;
  tft.fillRect(0, top, 240, 320 - top, TFT_BLACK);
  uint8_t oldest = (tftLogCount < TFT_LOG_LINES) ? 0 : tftLogWrite;
  uint8_t n = tftLogCount;
  if (n > maxVisible) {
    oldest = (oldest + (n - maxVisible)) % TFT_LOG_LINES;
    n = maxVisible;
  }
  for (uint8_t i = 0; i < n; i++) {
    uint8_t idx = (oldest + i) % TFT_LOG_LINES;
    int y = top + (int)i * lineH;
    bool newest = (i == (n - 1));
    tft.setTextColor(newest ? TFT_YELLOW : TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(tftLog[idx], 6, y, 2);  // slightly more left margin
  }
}
#endif

void updateDisplay() {
  if (inCalibration) {
    u8x8.clear();
    u8x8.drawString(0, 0, "CALIBRATION");
    u8x8.drawString(0, 2, "10MHz CLK1");
    u8x8.drawString(0, 4, "Corr:");
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", si5351_corr);
    u8x8.drawString(6, 4, buf);
    u8x8.drawString(0, 6, "LongA=Save");
    return;
  }

  if (inMenu) {
    u8x8.clear();
    u8x8.drawString(0, 0, "MENU");
    u8x8.drawString(0, 2, "Enc=Corr");
    u8x8.drawString(0, 4, "LongA=Save");
    u8x8.drawString(0, 6, "B/C=Cancel");
    return;
  }

  // Line 0: Mode + IST time
  char line0[17];
  String t = getTimeIST();
  snprintf(line0, sizeof(line0), "%-4s %s", MODE_NAMES[currentMode], t.c_str());
  u8x8.drawString(0, 0, line0);

  // Line 1: Frequency with separators (e.g. 14.074.000)
  uint32_t f = getCurrentDialFreq();
  char freqStr[17];
  snprintf(freqStr, sizeof(freqStr), "%2lu.%03lu.%03lu",
           f / 1000000UL, (f / 1000UL) % 1000UL, f % 1000UL);
  u8x8.drawString(0, 2, freqStr);

  // Line 2: Rx/Tx status + band + beacon progress
  char line2[17];
  if (beaconTxActive && currentMode == MODE_FT8) {
    snprintf(line2, sizeof(line2), "TX FT8 %u/%u", ft8_sym_index + 1, FT8_SYMBOL_COUNT);
  } else if (beaconTxActive && currentMode == MODE_WSPR) {
    snprintf(line2, sizeof(line2), "TX WSPR %u", wspr_sym_index + 1);
  } else {
    snprintf(line2, sizeof(line2), "%s %s %s",
             isTransmitting ? "TX" : "RX",
             BANDS[currentBand].name,
             beaconEnabled ? "BCN" : "---");
  }
  u8x8.drawString(0, 4, line2);

  // Line 3: show STA IP if connected, else AP IP
  char line3[17];
  if (wifiConnected && wifiIP.length() > 1 && wifiIP != "-") {
    snprintf(line3, sizeof(line3), "%s", wifiIP.c_str());
  } else {
    snprintf(line3, sizeof(line3), "AP:%s", apIP.c_str());
  }
  u8x8.drawString(0, 6, line3);
}

// ============================================================
// BUTTONS & ENCODER
// ============================================================
void handleButtons() {
  unsigned long now = millis();

  // --- Switch A (Mode / Long = confirm in menu/cal)
  bool a = (digitalRead(PIN_SW_A) == LOW);
  if (a && !swA_pressed) {
    swA_pressed = true;
    swA_down = now;
  } else if (!a && swA_pressed) {
    unsigned long dur = now - swA_down;
    swA_pressed = false;
    if (dur >= LONG_PRESS_MS) {
      // Long press
      if (inCalibration || inMenu) {
        saveSettings();
        stopCalibration();
        inMenu = false;
        Serial.println("Saved & exit menu/cal");
      }
    } else if (dur > DEBOUNCE_MS) {
      // Short press = change mode
      if (!inCalibration && !inMenu) {
        currentMode = (currentMode + 1) % MODE_COUNT;
        stopBeaconTx();
        setFrequency(getCurrentDialFreq());
        if (currentMode == MODE_FT8) encodeFt8Message();
        if (currentMode == MODE_WSPR) encodeWsprMessage();
        Serial.printf("Mode -> %s\n", MODE_NAMES[currentMode]);
      }
    }
  }

  // --- Switch B (Band Up / Cancel)
  bool b = (digitalRead(PIN_SW_B) == LOW);
  if (b && !swB_pressed) {
    swB_pressed = true;
    swB_down = now;
  } else if (!b && swB_pressed) {
    unsigned long dur = now - swB_down;
    swB_pressed = false;
    if (dur > DEBOUNCE_MS) {
      if (inCalibration || inMenu) {
        stopCalibration();
        inMenu = false;
        Serial.println("Cancel menu/cal");
      } else {
        currentBand = (currentBand + 1) % BAND_COUNT;
        setFrequency(getCurrentDialFreq());
        Serial.printf("Band -> %s\n", BANDS[currentBand].name);
      }
    }
  }

  // --- Switch C (Band Down / Cancel)
  bool c = (digitalRead(PIN_SW_C) == LOW);
  if (c && !swC_pressed) {
    swC_pressed = true;
    swC_down = now;
  } else if (!c && swC_pressed) {
    unsigned long dur = now - swC_down;
    swC_pressed = false;
    if (dur > DEBOUNCE_MS) {
      if (inCalibration || inMenu) {
        stopCalibration();
        inMenu = false;
      } else {
        currentBand = (currentBand + BAND_COUNT - 1) % BAND_COUNT;
        setFrequency(getCurrentDialFreq());
        Serial.printf("Band -> %s\n", BANDS[currentBand].name);
      }
    }
  }

  // --- Encoder Switch (long = enter menu / cal)
  bool e = (digitalRead(PIN_ENC_SW) == LOW);
  if (e && !encSw_pressed) {
    encSw_pressed = true;
    encSw_down = now;
  } else if (!e && encSw_pressed) {
    unsigned long dur = now - encSw_down;
    encSw_pressed = false;
    if (dur >= LONG_PRESS_MS) {
      if (!inCalibration && !inMenu) {
        inMenu = true;
        startCalibration();   // 10 MHz on CLK1 for counter
        Serial.println("Enter CAL/MENU (long enc)");
      }
    }
  }
}

void handleEncoder() {
  // Simple polling encoder (interrupt version can be added later)
  int a = digitalRead(PIN_ENC_A);
  int b = digitalRead(PIN_ENC_B);
  if (a != lastEncA) {
    if (b != a) {
      encoderPos++;
    } else {
      encoderPos--;
    }
    lastEncA = a;

    if (inMenu || inCalibration) {
      // Adjust correction
      si5351_corr += (encoderPos > 0) ? 10 : -10;
      encoderPos = 0;
      applyCorrection(si5351_corr);
    }
    // else: could use encoder for fine frequency later
  }
}

// ============================================================
// FREQUENCY HELPERS
// ============================================================
uint32_t getCurrentDialFreq() {
  switch (currentMode) {
    case MODE_FT8:  return BANDS[currentBand].ft8;
    case MODE_FT4:  return BANDS[currentBand].ft4;
    case MODE_JS8:  return BANDS[currentBand].js8;
    case MODE_WSPR: return BANDS[currentBand].wspr;
    default:        return BANDS[currentBand].ft8;
  }
}

// ============================================================
// EEPROM + stored WiFi credentials
// ============================================================
void setupEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
}

void loadWifiCreds() {
  haveWifiCreds = false;
  storedSSID[0] = 0;
  storedPASS[0] = 0;
  if (EEPROM.read(EE_ADDR_WIFI_VALID) != 0x5A) return;
  for (int i = 0; i < WIFI_SSID_MAX; i++) storedSSID[i] = EEPROM.read(EE_ADDR_WIFI_SSID + i);
  storedSSID[WIFI_SSID_MAX] = 0;
  for (int i = 0; i < WIFI_PASS_MAX; i++) storedPASS[i] = EEPROM.read(EE_ADDR_WIFI_PASS + i);
  storedPASS[WIFI_PASS_MAX] = 0;
  if (strlen(storedSSID) > 0) {
    haveWifiCreds = true;
    Serial.printf("EEPROM WiFi SSID: '%s'\n", storedSSID);
  }
}

void saveWifiCreds(const char* ssid, const char* pass) {
  memset(storedSSID, 0, sizeof(storedSSID));
  memset(storedPASS, 0, sizeof(storedPASS));
  strncpy(storedSSID, ssid, WIFI_SSID_MAX);
  strncpy(storedPASS, pass, WIFI_PASS_MAX);
  for (int i = 0; i < WIFI_SSID_MAX; i++) EEPROM.write(EE_ADDR_WIFI_SSID + i, storedSSID[i]);
  for (int i = 0; i < WIFI_PASS_MAX; i++) EEPROM.write(EE_ADDR_WIFI_PASS + i, storedPASS[i]);
  EEPROM.write(EE_ADDR_WIFI_VALID, 0x5A);
  EEPROM.commit();
  haveWifiCreds = true;
  Serial.printf("WiFi creds saved: '%s'\n", storedSSID);
}

void clearWifiCreds() {
  EEPROM.write(EE_ADDR_WIFI_VALID, 0);
  EEPROM.commit();
  haveWifiCreds = false;
  storedSSID[0] = 0;
  storedPASS[0] = 0;
  Serial.println("WiFi creds cleared");
}

void loadSettings() {
  if (EEPROM.read(EE_ADDR_MAGIC) != EE_MAGIC_VALUE) {
    Serial.println("EEPROM empty - using defaults");
    si5351_corr = SI5351_CORR_DEFAULT;
    currentBand = DEFAULT_BAND;
    currentMode = DEFAULT_MODE;
  } else {
    EEPROM.get(EE_ADDR_CORR, si5351_corr);
    currentBand = EEPROM.read(EE_ADDR_BAND);
    currentMode = EEPROM.read(EE_ADDR_MODE);
    if (currentBand >= BAND_COUNT) currentBand = DEFAULT_BAND;
    if (currentMode >= MODE_COUNT) currentMode = DEFAULT_MODE;
    Serial.printf("Loaded corr=%ld band=%u mode=%u\n", si5351_corr, currentBand, currentMode);
  }
  loadWifiCreds();
}

void saveSettings() {
  EEPROM.write(EE_ADDR_MAGIC, EE_MAGIC_VALUE);
  EEPROM.put(EE_ADDR_CORR, si5351_corr);
  EEPROM.write(EE_ADDR_BAND, currentBand);
  EEPROM.write(EE_ADDR_MODE, currentMode);
  EEPROM.commit();
  Serial.println("Settings saved to EEPROM");
}

// ============================================================
// WiFiManager: exclusive STA *or* AP (never both)
// Boot: try STA from EEPROM → if fail / no creds → AP portal only
// After successful connect from portal: save EEPROM, stop AP, stay STA
// ============================================================

void startConfigPortal() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  apMode = true;
  wifiConnected = false;
  wifiIP = "-";
  apIP = WiFi.softAPIP().toString();
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println(F("----- Config Portal (AP only) -----"));
  Serial.print(F("AP SSID : ")); Serial.println(WIFI_AP_SSID);
  Serial.print(F("AP Pass : ")); Serial.println(WIFI_AP_PASS);
  Serial.print(F("Portal  : http://")); Serial.println(apIP);
  Serial.println(F("-----------------------------------"));
}

void stopConfigPortal() {
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
  Serial.println(F("Config portal AP stopped"));
}

// STA-only connect + NTP. Returns true on success.
bool connectSTA(const char* ssid, const char* pass, uint32_t timeoutMs = 20000) {
  if (!ssid || strlen(ssid) == 0) return false;

  Serial.printf("STA connecting to '%s' ...\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("STA FAILED"));
    wifiConnected = false;
    wifiIP = "-";
    return false;
  }

  wifiConnected = true;
  apMode = false;
  wifiIP = WiFi.localIP().toString();
  Serial.print(F("STA OK  IP: "));
  Serial.println(wifiIP);

  NTP.begin(NTP_SERVER, "time.nist.gov");
  if (NTP.waitSet(12000)) {
    timeValid = true;
    lastNtpSync = millis();
    Serial.print(F("NTP OK  IST = "));
    Serial.println(getTimeIST());
  } else {
    Serial.println(F("NTP failed (STA still up)"));
  }
  return true;
}

void setupWiFi() {
  WiFi.setHostname(WIFI_HOSTNAME);
  wifiConnected = false;
  wifiIP = "-";
  timeValid = false;
  apMode = false;

  Serial.println(F("----- WiFiManager -----"));

  if (haveWifiCreds) {
    Serial.print(F("EEPROM SSID: "));
    Serial.println(storedSSID);
    if (connectSTA(storedSSID, storedPASS, 20000)) {
      Serial.println(F("Boot: STA OK — AP off"));
      Serial.println(F("-----------------------"));
      return;
    }
    Serial.println(F("Boot: STA failed — opening config portal"));
  } else {
    Serial.println(F("Boot: no saved credentials"));
  }

  startConfigPortal();
  Serial.println(F("-----------------------"));
}

void syncNTP() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiIP = WiFi.localIP().toString();
    NTP.begin(NTP_SERVER, "time.nist.gov");
    if (NTP.waitSet(8000)) {
      timeValid = true;
      lastNtpSync = millis();
      Serial.print(F("NTP re-sync OK  IST = "));
      Serial.println(getTimeIST());
    }
  } else if (haveWifiCreds && !apMode) {
    Serial.println(F("STA lost — reconnecting..."));
    if (!connectSTA(storedSSID, storedPASS, 15000)) {
      startConfigPortal();
    }
  }
}

String getTimeIST() {
  time_t now = time(nullptr);
  // Valid Unix time is well after 2020
  if (now < 1600000000UL) {
    return "--:--:--";
  }
  now += TIMEZONE_OFFSET_SEC;
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  char buf[10];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}
String formatFreqPretty(uint32_t f) {
  // e.g. 14.074,000
  char buf[20];
  snprintf(buf, sizeof(buf), "%lu.%03lu,%03lu",
           f / 1000000UL, (f / 1000UL) % 1000UL, f % 1000UL);
  return String(buf);
}

// ============================================================
// WEB SERVER
// ============================================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PICO2W PDX</title>
  <style>
    *{box-sizing:border-box}
    body{font-family:system-ui,sans-serif;background:#0d1117;color:#e6edf3;
         margin:0;padding:16px;text-align:center}
    h1{color:#58a6ff;margin:8px 0 16px}
    .card{background:#161b22;border:1px solid #30363d;padding:16px;
          border-radius:10px;margin:0 auto 14px;max-width:420px}
    .freq{font-size:2.2rem;font-weight:700;color:#3fb950;letter-spacing:1px;
          margin:10px 0;font-variant-numeric:tabular-nums}
    .label{color:#8b949e;font-size:0.85rem}
    .val{font-size:1.1rem;margin:4px 0 10px}
    button{padding:10px 16px;margin:5px;font-size:15px;border:none;
           border-radius:8px;background:#238636;color:#fff;cursor:pointer}
    button.secondary{background:#21262d;border:1px solid #30363d}
    button.danger{background:#da3633}
    input,select{padding:10px;font-size:15px;width:90%;max-width:280px;
                 margin:6px auto;display:block;border-radius:6px;
                 border:1px solid #30363d;background:#0d1117;color:#e6edf3;
                 text-align:center}
    .i2c-line{padding:4px 0;border-bottom:1px solid #21262d;font-family:monospace}
    #scanResult{text-align:left;max-width:320px;margin:10px auto;font-size:0.9rem}
  </style>
</head>
<body>
  <h1>PICO2W PDX</h1>

  <div class="card">
    <div class="label">FREQUENCY</div>
    <div class="freq">%FREQ%</div>
    <div class="val">%MODE% &nbsp;|&nbsp; %BAND% &nbsp;|&nbsp; %STATUS%</div>
    <div class="label">Beacon</div>
    <div class="val">%BEACON%</div>
    <div class="label">Time (IST)</div>
    <div class="val">%TIME%</div>
    <div class="label">Call / Grid</div>
    <div class="val">%CALL% &nbsp; %GRID%</div>
    <div class="label">Network</div>
    <div class="val">%IP%</div>
  </div>

  <div class="card">
    <div class="label">SOFT BUTTONS</div>
    <button onclick="go('/mode')">Mode (A)</button>
    <button onclick="go('/bandup')">Band Up (B)</button>
    <button onclick="go('/banddn')">Band Dn (C)</button><br>
    <button onclick="go('/tune')">Tune / PTT</button>
    <button onclick="go('/menu')">Menu / Cal</button><br>
    <button id="beaconBtn" onclick="go('/beacon')" style="background:%BEACON_COLOR%">Beacon %BEACON%</button>
  </div>

  <div class="card">
    <div class="label">Si5351 CORRECTION</div>
    <div class="val">Current: %CORR%</div>
    <input type="number" id="corr" value="%CORR%">
    <button onclick="setCorr()">Set Corr</button>
    <button class="danger" onclick="go('/save')">Save EEPROM</button>
  </div>

  <div class="card">
    <div class="label">WiFiManager (credentials)</div>
    <div class="val">Stored SSID: %SSID%</div>
    <button class="secondary" onclick="scanWifi()">Scan Networks</button>
    <div id="scanResult">%SCAN%</div>
    <select id="ssidList" style="display:none" onchange="document.getElementById('ssid').value=this.value">
      <option value="">-- select network --</option>
    </select>
    <input type="text" id="ssid" placeholder="SSID" value="">
    <input type="password" id="pass" placeholder="Password">
    <button onclick="saveWifi()">Save & Connect</button>
    <button class="danger" onclick="go('/wificlear')">Erase credentials</button>
  </div>

  <div class="card">
    <div class="label">I2C DEVICES</div>
    %I2C%
  </div>

  <script>
    function go(u){ fetch(u).then(()=>setTimeout(()=>location.reload(),400)); }
    function setCorr(){
      fetch('/setcorr?v='+document.getElementById('corr').value)
        .then(()=>setTimeout(()=>location.reload(),300));
    }
    function saveWifi(){
      let s=encodeURIComponent(document.getElementById('ssid').value);
      let p=encodeURIComponent(document.getElementById('pass').value);
      fetch('/wifisave?ssid='+s+'&pass='+p)
        .then(r=>r.text()).then(t=>{ alert(t); location.reload(); });
    }
    function scanWifi(){
      document.getElementById('scanResult').innerHTML='Scanning...';
      fetch('/wifiscan').then(r=>r.json()).then(list=>{
        let sel=document.getElementById('ssidList');
        sel.innerHTML='<option value="">-- select network --</option>';
        let html='';
        list.forEach(n=>{
          html+='<div class="i2c-line">'+n.ssid+' ('+n.rssi+' dBm)'+(n.enc?' *':'')+'</div>';
          let o=document.createElement('option');
          o.value=n.ssid; o.text=n.ssid+' ('+n.rssi+')';
          sel.add(o);
        });
        document.getElementById('scanResult').innerHTML=html||'No networks';
        sel.style.display='block';
      }).catch(e=>{
        document.getElementById('scanResult').innerHTML='Scan failed';
      });
    }
  </script>
</body>
</html>
)rawliteral";

String i2cDeviceList = "Scanning...";
String lastScanHtml = "<div class='label'>Press Scan Networks</div>";

void scanI2C() {
  i2cDeviceList = "";
  byte error, address;
  int n = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      char buf[64];
      const char* name = "";
      if (address == 0x60 || address == 0x61) name = "Si5351";
      else if (address == 0x3C || address == 0x3D) name = "OLED SH1106/SSD1306";
      else if (address == 0x68) name = "DS3231 RTC?";
      else if (address == 0x11) name = "Si4732?";
      else if (address >= 0x20 && address <= 0x27) name = "PCF8574?";
      else if (address == 0x57) name = "EEPROM?";
      snprintf(buf, sizeof(buf),
               "<div class='i2c-line'>0x%02X &nbsp; %s</div>", address, name);
      i2cDeviceList += buf;
      n++;
      Serial.printf("I2C found 0x%02X %s\n", address, name);
    }
  }
  if (n == 0) i2cDeviceList = "<div class='i2c-line'>None found</div>";
}

void handleRoot() {
  String page = FPSTR(HTML_PAGE);
  page.replace("%CALL%", MY_CALLSIGN);
  page.replace("%GRID%", MY_GRID);
  page.replace("%MODE%", MODE_NAMES[currentMode]);
  page.replace("%BAND%", BANDS[currentBand].name);
  page.replace("%FREQ%", formatFreqPretty(getCurrentDialFreq()));
  page.replace("%STATUS%", isTransmitting ? "TX" : "RX");
  page.replace("%BEACON%", beaconEnabled ? "ON" : "OFF");
  page.replace("%BEACON_COLOR%", beaconEnabled ? "#da3633" : "#238636");
  page.replace("%TIME%", getTimeIST());
  String ipInfo;
  if (wifiConnected && wifiIP.length() > 1 && wifiIP != "-") {
    ipInfo = "Mode: STA<br>IP: " + wifiIP;
  } else if (apMode) {
    ipInfo = "Mode: Config Portal (AP)<br>IP: " + apIP;
  } else {
    ipInfo = "Mode: offline";
  }
  page.replace("%IP%", ipInfo);
  page.replace("%CORR%", String(si5351_corr));
  page.replace("%I2C%", i2cDeviceList);
  page.replace("%SSID%", haveWifiCreds ? String(storedSSID) : String("(none)"));
  page.replace("%SCAN%", lastScanHtml);
  server.send(200, "text/html", page);
}

void handleWifiScan() {
  Serial.println("WiFi scan...");
  // Brief STA scan while AP stays up
  int n = WiFi.scanNetworks();
  String json = "[";
  lastScanHtml = "";
  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    bool enc = WiFi.encryptionType(i) != ENC_TYPE_NONE;
    json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) +
            ",\"enc\":" + String(enc ? "true" : "false") + "}";
    lastScanHtml += "<div class='i2c-line'>" + ssid + " (" + String(rssi) + " dBm)" +
                    (enc ? " *" : "") + "</div>";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
  Serial.printf("Scan found %d networks\n", n);
}

void handleWifiSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "Missing SSID");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > WIFI_SSID_MAX) {
    server.send(400, "text/plain", "Bad SSID");
    return;
  }

  // Reply before tearing down AP so the browser gets a response
  server.send(200, "text/plain", "Connecting to " + ssid + " ...");
  delay(300);

  stopConfigPortal();
  delay(200);

  if (connectSTA(ssid.c_str(), pass.c_str(), 25000)) {
    // Store credentials only after successful connect
    saveWifiCreds(ssid.c_str(), pass.c_str());
    Serial.println(F("Credentials saved after successful STA connect"));
  } else {
    Serial.println(F("Connect failed — credentials NOT saved, reopening portal"));
    startConfigPortal();
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/mode", []() {
    stopBeaconTx();
    currentMode = (currentMode + 1) % MODE_COUNT;
    setFrequency(getCurrentDialFreq());
    if (currentMode == MODE_FT8) encodeFt8Message();
    if (currentMode == MODE_WSPR) encodeWsprMessage();
    server.send(200, "text/plain", MODE_NAMES[currentMode]);
  });
  server.on("/bandup", []() {
    stopBeaconTx();
    currentBand = (currentBand + 1) % BAND_COUNT;
    setFrequency(getCurrentDialFreq());
    server.send(200, "text/plain", BANDS[currentBand].name);
  });
  server.on("/banddn", []() {
    stopBeaconTx();
    currentBand = (currentBand + BAND_COUNT - 1) % BAND_COUNT;
    setFrequency(getCurrentDialFreq());
    server.send(200, "text/plain", BANDS[currentBand].name);
  });
  server.on("/tune", []() {
    if (!isTransmitting) setTxMode();
    else setRxMode();
    server.send(200, "text/plain", isTransmitting ? "TX" : "RX");
  });
  server.on("/beacon", []() {
    beaconEnabled = !beaconEnabled;
    if (!beaconEnabled && beaconTxActive) stopBeaconTx();
#if DISPLAY_OPTION == 1
    {
      char line[TFT_LOG_LEN];
      snprintf(line, sizeof(line), "BEACON %s", beaconEnabled ? "ON" : "OFF");
      tftLogAdd(line);
    }
    tftDrawBeaconStatus();
#endif
    Serial.printf("Beacon %s\n", beaconEnabled ? "ON" : "OFF");
    server.send(200, "text/plain", beaconEnabled ? "ON" : "OFF");
  });
  server.on("/menu", []() {
    inMenu = true;
    server.send(200, "text/plain", "MENU");
  });
  server.on("/setcorr", []() {
    if (server.hasArg("v")) applyCorrection(server.arg("v").toInt());
    server.send(200, "text/plain", "OK");
  });
  server.on("/save", []() {
    saveSettings();
    inMenu = false;
    stopCalibration();
    server.send(200, "text/plain", "SAVED");
  });
  server.on("/wifiscan", handleWifiScan);
  server.on("/wifisave", handleWifiSave);
  server.on("/wificlear", []() {
    clearWifiCreds();
    server.send(200, "text/plain", "WiFi cleared");
  });
  server.begin();
  Serial.println("Web server started");
}

// ============================================================
// CAT (Kenwood TS-480 style skeleton)
// ============================================================
void processCAT() {
  static String cmd;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == ';') {
      // process command
      cmd.toUpperCase();
      if (cmd.startsWith("FA")) {               // Set / Get VFO A
        if (cmd.length() > 2) {
          // set frequency (11 digits)
          // FA00014074000;
          uint32_t f = cmd.substring(2).toInt();
          // find closest band/mode later; for now just set Si5351
          setFrequency(f);
        }
        // reply
        char reply[20];
        snprintf(reply, sizeof(reply), "FA%011lu;", getCurrentDialFreq());
        Serial.print(reply);
      }
      else if (cmd == "ID") {
        Serial.print("ID020;");   // TS-480
      }
      else if (cmd == "IF") {
        // Information
        char reply[40];
        snprintf(reply, sizeof(reply), "IF%011lu     +0000000%01u00000;",
                 getCurrentDialFreq(), isTransmitting ? 1 : 0);
        Serial.print(reply);
      }
      else if (cmd.startsWith("MD")) {
        // Mode
        Serial.print("MD2;");  // USB for digital
      }
      else if (cmd == "RX") {
        setRxMode();
        Serial.print("RX;");
      }
      else if (cmd == "TX") {
        setTxMode();
        Serial.print("TX;");
      }
      // add more as needed
      cmd = "";
    } else if (c >= 32) {
      cmd += c;
      if (cmd.length() > 32) cmd = "";
    }
  }
}

// ============================================================
// FT8 + WSPR BEACON (TX only)
// Requires: Etherkit JTEncode library
// FT8: free-text "CQ VU2UPX MK6", 15 s UTC slots
// WSPR: Type 1 call+grid4+dBm, even UTC minute + 1 s
// ============================================================

// Standard FT8 Costas array (tone indices 0..7) — must appear 3 times
static const uint8_t FT8_COSTAS[7] = { 3, 1, 4, 0, 6, 5, 2 };

bool verifyFt8Costas() {
  // Leading 0..6, middle 36..42, trailing 72..78
  const int pos[3] = { 0, 36, 72 };
  for (int p = 0; p < 3; p++) {
    for (int i = 0; i < 7; i++) {
      if (ft8_symbols[pos[p] + i] != FT8_COSTAS[i]) return false;
    }
  }
  return true;
}

void encodeFt8Message() {
  char msg[14];
  snprintf(msg, sizeof(msg), "CQ %s MK6", MY_CALLSIGN);
  msg[13] = 0;
  memset(ft8_symbols, 0, sizeof(ft8_symbols));
  // JTEncode builds full 79-symbol frame INCLUDING three Costas preambles:
  //   symbols[0..6], [36..42], [72..78] = {3,1,4,0,6,5,2}
  // Data fills [7..35] and [43..71]. We transmit the array as-is.
  jtencode.ft8_encode(msg, ft8_symbols);
  ft8_encoded = true;
  Serial.print(F("FT8 encoded: "));
  Serial.println(msg);
  if (verifyFt8Costas()) {
    Serial.println(F("FT8 Costas OK (start/mid/end 3-1-4-0-6-5-2)"));
  } else {
    // If library output differs, force-insert standard Costas so OTA sync works
    Serial.println(F("FT8 Costas mismatch — inserting standard preamble"));
    for (int i = 0; i < 7; i++) {
      ft8_symbols[i]      = FT8_COSTAS[i];
      ft8_symbols[36 + i] = FT8_COSTAS[i];
      ft8_symbols[72 + i] = FT8_COSTAS[i];
    }
  }
}

void encodeWsprMessage() {
  memset(wspr_symbols, 0, sizeof(wspr_symbols));
  // Type 1: callsign, 4-char grid, power dBm
  jtencode.wspr_encode(MY_CALLSIGN, MY_GRID4, WSPR_DBM, wspr_symbols);
  wspr_encoded = true;
  Serial.printf("WSPR encoded: %s %s %u dBm\n", MY_CALLSIGN, MY_GRID4, WSPR_DBM);
}

// UTC broken-down time helper
bool getUtcTm(struct tm *t) {
  time_t now = time(nullptr);
  if (now < 1600000000UL) return false;
  gmtime_r(&now, t);
  return true;
}

int getUtcSlotPhase15() {
  struct tm t;
  if (!getUtcTm(&t)) return -1;
  return (t.tm_min * 60 + t.tm_sec) % FT8_SLOT_SEC;
}

void stopBeaconTx() {
  if (!beaconTxActive) return;
  beaconTxActive = false;
  si5351.output_enable(SI5351_CLK2, 0);
  setRxMode();
  Serial.println(F("Beacon TX end"));
}

void setBeaconTone(uint32_t dialHz, uint32_t audioBase, uint8_t symbol, uint16_t toneSpacingCenti) {
  uint64_t f = ((uint64_t)(dialHz + audioBase) * 100ULL) +
               ((uint64_t)symbol * toneSpacingCenti);
  si5351.set_freq(f, SI5351_CLK2);
}

// ---- FT8 TX ----
void startFt8Tx() {
  if (!ft8_encoded) encodeFt8Message();
  uint32_t dial = getCurrentDialFreq();
  setTxMode();
  beaconTxActive = true;
  ft8_sym_index = 0;
  ft8_sym_start_ms = millis();
  si5351.output_enable(SI5351_CLK2, 1);
  setBeaconTone(dial, 1500, ft8_symbols[0], FT8_TONE_SPACING);
  Serial.printf("FT8 TX start dial=%lu\n", dial);
#if DISPLAY_OPTION == 1
  tftLogTxEvent();
#endif
}

void serviceFt8Tx() {
  if (millis() - ft8_sym_start_ms < FT8_SYMBOL_MS) return;
  ft8_sym_index++;
  if (ft8_sym_index >= FT8_SYMBOL_COUNT) {
    stopBeaconTx();
    return;
  }
  setBeaconTone(getCurrentDialFreq(), 1500, ft8_symbols[ft8_sym_index], FT8_TONE_SPACING);
  ft8_sym_start_ms = millis();
}

// ---- WSPR TX ----
void startWsprTx() {
  if (!wspr_encoded) encodeWsprMessage();
  uint32_t dial = getCurrentDialFreq();
  setTxMode();
  beaconTxActive = true;
  wspr_sym_index = 0;
  wspr_sym_start_ms = millis();
  si5351.output_enable(SI5351_CLK2, 1);
  setBeaconTone(dial, 1500, wspr_symbols[0], WSPR_TONE_SPACING);
  Serial.printf("WSPR TX start dial=%lu  %s %s %udBm\n",
                dial, MY_CALLSIGN, MY_GRID4, WSPR_DBM);
#if DISPLAY_OPTION == 1
  tftLogTxEvent();
#endif
}

void serviceWsprTx() {
  if (millis() - wspr_sym_start_ms < WSPR_SYMBOL_MS) return;
  wspr_sym_index++;
  if (wspr_sym_index >= WSPR_SYMBOL_COUNT) {
    stopBeaconTx();
    return;
  }
  setBeaconTone(getCurrentDialFreq(), 1500, wspr_symbols[wspr_sym_index], WSPR_TONE_SPACING);
  wspr_sym_start_ms = millis();
}

void beaconTick() {
  // Mode-exclusive: only ONE of FT8 or WSPR may run, based on currentMode.
  // FT4 / JS8: no beacon TX in Step-1.
  if (beaconTxActive) {
    if (currentMode == MODE_FT8) {
      serviceFt8Tx();
    } else if (currentMode == MODE_WSPR) {
      serviceWsprTx();
    } else {
      stopBeaconTx();  // mode changed away from beacon modes
    }
    return;
  }

  if (inCalibration || inMenu) return;
  if (currentMode != MODE_FT8 && currentMode != MODE_WSPR) return;
  if (time(nullptr) < 1600000000UL) return;  // need valid UTC

  struct tm t;
  if (!getUtcTm(&t)) return;

  // -------- FT8: once every FT8_TX_EVERY_MIN UTC minutes --------
  // FT8 protocol uses 15 s slots; we only key the first slot of selected minutes
  // so the beacon is not continuous every 15 s (default: once per minute).
  if (currentMode == MODE_FT8) {
    if (t.tm_sec > 1) return;
    if ((t.tm_min % FT8_TX_EVERY_MIN) != 0) return;
    int slotId = t.tm_hour * 60 + t.tm_min;
    if (slotId != lastFt8Slot) {
      lastFt8Slot = slotId;
      startFt8Tx();
    }
    return;
  }

  // -------- WSPR: start at second 1 of even UTC minutes --------
  if (currentMode == MODE_WSPR) {
    // Only even minutes, and only every WSPR_TX_EVERY_MIN minutes
    if ((t.tm_min % 2) != 0) return;
    if ((t.tm_min % WSPR_TX_EVERY_MIN) != 0) return;
    if (t.tm_sec < 1 || t.tm_sec > 2) return;  // start window ~1s

    int slotId = t.tm_hour * 60 + t.tm_min;  // unique even-minute id
    if (slotId == lastWsprSlot) return;
    lastWsprSlot = slotId;
    startWsprTx();
    return;
  }
}

// ============================================================
// END
// ============================================================
