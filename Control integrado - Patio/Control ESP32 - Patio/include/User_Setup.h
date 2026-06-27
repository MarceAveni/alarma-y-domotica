// ===========================
//  TFT_eSPI – User Setup File
//  Pantalla 3.5" RPI Display
//  ILI9486 + XPT2046
// ===========================

// ======= Tipo de TFT =======
//#define ST7796_DRIVER
//#define ILI9486_DRIVER
#define ILI9488_DRIVER
//#define HX8357D_DRIVER
//#define ILI9341_DRIVER
//#define R61581_DRIVER

// ======= Dimensiones =======
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// ======= SPI DEL DISPLAY =======
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18

// ======= Pines de control =======
#define TFT_CS   5
#define TFT_DC   15
#define TFT_RST  4   // si no lo usás físicamente podés dejarlo igual
//#define TFT_BL   2

// ======= SPI TOUCH (XPT2046) =======
#define TOUCH_CS     32
#define TOUCH_IRQ    34   // entrada solo lectura

// ======= Opciones TFT =======
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SPI_FREQUENCY  16000000
//#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
