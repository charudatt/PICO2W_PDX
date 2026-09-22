// TFT_eSPI User_Setup for PICO2W PDX
// ILI9341 320x240, hardware SPI

//#include "PICO2W_PDX_Pins.h"

#define USER_SETUP_ID 920

#define ILI9341_DRIVER

// TFT_eSPI uses these dimensions internally as portrait dimensions.
// setRotation(1) gives 320x240 landscape.
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 16
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   17
#define TOUCH_CS 22

#define TFT_DC   20
#define TFT_RST  21

// Shared SPI bus: XPT2046 touch controller
//#define TOUCH_CS PDX_TOUCH_CS

// Use ordinary SPI for the first validation.
// Do NOT enable RP2040_PIO_SPI because later touch/SD reads need MISO.
#define SPI_FREQUENCY       10000000
#define SPI_TOUCH_FREQUENCY 2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
