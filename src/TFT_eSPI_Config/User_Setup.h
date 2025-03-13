// カスタムUser_Setup.h for TFT_eSPI
// ATOM S3用の外部SPI液晶設定

#define USER_SETUP_INFO "User_Setup for ATOM S3"

// ディスプレイの解像度設定
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ドライバの定義
#define ILI9341_DRIVER

// ソフトウェアSPIモードを有効化
#define USE_SOFT_SPI

// ピン設定
#define TFT_MISO -1
#define TFT_MOSI 7
#define TFT_SCLK 6
#define TFT_CS   5
#define TFT_DC   39
#define TFT_RST  38
#define TFT_BL   8

// 回転設定
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// SPI周波数設定（ソフトウェアSPIでは低速に設定）
#define SPI_FREQUENCY  4000000
#define SPI_READ_FREQUENCY  4000000
#define SPI_TOUCH_FREQUENCY  2500000

// ESP32 S3設定
// ESP32_PARALLELを無効化（SPIモードと競合するため）
// #define ESP32_PARALLEL
