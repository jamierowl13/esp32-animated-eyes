// ============================================================================
//  TFT_eSPI setup for 2x GC9A01 240x240 round displays on an ESP32
//
//  COPY THIS FILE OVER:  Arduino/libraries/TFT_eSPI/User_Setup.h
//  (back up the original first). TFT_eSPI only reads its own folder,
//  so leaving this file next to the sketch is not enough.
// ============================================================================

#define USER_SETUP_INFO "ESP32_Dual_GC9A01_Eyes"

#define GC9A01_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Shared SPI bus for both displays
#define TFT_MOSI 23   // display pin "SDA"
#define TFT_SCLK 18   // display pin "SCL"
#define TFT_MISO -1   // not used
#define TFT_DC    2
#define TFT_RST   4

// Chip select MUST be -1: the sketch drives the two CS pins (GPIO 5 and 27) itself
#define TFT_CS   -1

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT

// 40 MHz is reliable with short wires. Try 80000000 for more speed;
// drop to 27000000 if you see garbage on the screens.
#define SPI_FREQUENCY  40000000
