/**
 * @file Arduboy2Core.cpp
 * \brief
 * The Arduboy2Core class for Arduboy hardware initilization and control.
 */

#include "Arduboy2CoreDotMG.h"
#include <SPI.h>

static uint16_t borderLineColor = ST77XX_GRAY;
static uint16_t borderFillColor = ST77XX_BLACK;
static uint16_t pixelColor = ST77XX_WHITE;
static uint16_t bgColor = ST77XX_BLACK;
static uint8_t MADCTL = ST77XX_MADCTL_MY;
static uint8_t LEDs[] = {0, 0, 0};
static bool inverted = false;
static bool borderDrawn = false;

// Forward declarations
static void setWriteRegion(uint8_t x = (TFT_WIDTH-WIDTH)/2, uint8_t y = (TFT_HEIGHT-HEIGHT)/2, uint8_t width = WIDTH, uint8_t height = HEIGHT);
static void drawBorder();
static void drawLEDs();

Arduboy2Core::Arduboy2Core() { }

void Arduboy2Core::boot()
{
  bootPins();
  bootSPI();
  bootTFT();
  bootPowerSaving();
}

void Arduboy2Core::bootPins()
{
  pinMode(PIN_BUTTON_A, INPUT_PULLUP);
  pinMode(PIN_BUTTON_B, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_START, INPUT_PULLUP);
  pinMode(PIN_BUTTON_SELECT, INPUT_PULLUP);
}

void Arduboy2Core::bootTFT()
{
  pinMode(PIN_TFT_CS, OUTPUT);
  pinMode(PIN_TFT_DC, OUTPUT);
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);

  // Reset display
  delayShort(5);  // Let display stay in reset
  digitalWrite(PIN_TFT_RST, HIGH); // Bring out of reset
  delayShort(5);

  // run our customized boot-up command sequence against the
  // TFT to initialize it properly for Arduboy
  LCDCommandMode();

  startSPItransfer();

  sendLCDCommand(ST77XX_SWRESET);  // Software reset
  delayShort(150);

  sendLCDCommand(ST77XX_SLPOUT);  // Bring out of sleep mode
  delayShort(150);

  sendLCDCommand(ST7735_FRMCTR1);  // Framerate ctrl - normal mode
  SPItransfer(0x01);               // Rate = fosc/(1x2+40) * (LINE+2C+2D)
  SPItransfer(0x2C);
  SPItransfer(0x2D);

  sendLCDCommand(ST77XX_MADCTL);  // Set initial orientation
  SPItransfer(MADCTL);

  sendLCDCommand(ST77XX_COLMOD);  // Set color mode (12-bit)
  SPItransfer(0x03);

  sendLCDCommand(ST7735_GMCTRP1);  // Gamma Adjustments (pos. polarity)
  SPItransfer(0x02);
  SPItransfer(0x1c);
  SPItransfer(0x07);
  SPItransfer(0x12);
  SPItransfer(0x37);
  SPItransfer(0x32);
  SPItransfer(0x29);
  SPItransfer(0x2D);
  SPItransfer(0x29);
  SPItransfer(0x25);
  SPItransfer(0x2B);
  SPItransfer(0x39);
  SPItransfer(0x00);
  SPItransfer(0x01);
  SPItransfer(0x03);
  SPItransfer(0x10);

  sendLCDCommand(ST7735_GMCTRN1);  // Gamma Adjustments (neg. polarity)
  SPItransfer(0x03);
  SPItransfer(0x1D);
  SPItransfer(0x07);
  SPItransfer(0x06);
  SPItransfer(0x2E);
  SPItransfer(0x2C);
  SPItransfer(0x29);
  SPItransfer(0x2D);
  SPItransfer(0x2E);
  SPItransfer(0x2E);
  SPItransfer(0x37);
  SPItransfer(0x3F);
  SPItransfer(0x00);
  SPItransfer(0x00);
  SPItransfer(0x02);
  SPItransfer(0x10);

  setWriteRegion(0, 0, TFT_WIDTH, TFT_HEIGHT);
  for (int i = 0; i < TFT_WIDTH*TFT_HEIGHT/2; i++) {
    SPItransfer(bgColor >> 4);
    SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    SPItransfer(bgColor);
  }

  sendLCDCommand(ST77XX_DISPON); //  Turn screen on
  delayShort(100);

  endSPItransfer();

  LCDDataMode();

  drawBorder();

  // clear screen
  startSPItransfer();
  setWriteRegion();
  for (int i = 0; i < WIDTH*HEIGHT/2; i++) {
    SPItransfer(bgColor >> 4);
    SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    SPItransfer(bgColor);
  }
  endSPItransfer();
}

void Arduboy2Core::LCDDataMode()
{
  *portOutputRegister(IO_PORT) |= MASK_TFT_DC;
}

void Arduboy2Core::LCDCommandMode()
{
  *portOutputRegister(IO_PORT) &= ~MASK_TFT_DC;
}

// Initialize the SPI interface for the display
void Arduboy2Core::bootSPI()
{
  SPI.begin();
}

void Arduboy2Core::startSPItransfer()
{
  SPI.beginTransaction(SPI_SETTINGS);
  *portOutputRegister(IO_PORT) &= ~MASK_TFT_CS;
}

void Arduboy2Core::endSPItransfer()
{
  *portOutputRegister(IO_PORT) |= MASK_TFT_CS;
  SPI.endTransaction();
}

void Arduboy2Core::SPItransfer(uint8_t data)
{
  SPI_SERCOM->SPI.DATA.bit.DATA = data;
  while(!SPI_SERCOM->SPI.INTFLAG.bit.RXC);
}

void Arduboy2Core::safeMode()
{
  if (buttonsState() == UP_BUTTON)
  {
    digitalWriteRGB(RED_LED, RGB_ON);
    while (true);
  }
}

/* Power Management */

void Arduboy2Core::idle()
{
  // Not implemented
}

void Arduboy2Core::bootPowerSaving()
{
  // Not implemented
}

// Shut down the display
void Arduboy2Core::displayOff()
{
  startSPItransfer();
  sendLCDCommand(ST77XX_SLPIN);
  endSPItransfer();
  delayShort(150);
}

// Restart the display after a displayOff()
void Arduboy2Core::displayOn()
{
  startSPItransfer();
  sendLCDCommand(ST77XX_SLPOUT);
  endSPItransfer();
  delayShort(150);
}


/* Drawing */

uint16_t Arduboy2Core::getBorderLineColor()
{
  return borderLineColor;
}

void Arduboy2Core::setBorderLineColor(uint16_t color)
{
  borderLineColor = color;

  if (borderDrawn)
    drawBorder();
}

uint16_t Arduboy2Core::getBorderFillColor()
{
  return borderFillColor;
}

void Arduboy2Core::setBorderFillColor(uint16_t color)
{
  borderFillColor = color;

  if (borderDrawn)
    drawBorder();
}

uint16_t Arduboy2Core::getPixelColor()
{
  return pixelColor;
}

void Arduboy2Core::setPixelColor(uint16_t color)
{
  pixelColor = color;
}

uint16_t Arduboy2Core::getBackgroundColor()
{
  return bgColor;
}

void Arduboy2Core::setBackgroundColor(uint16_t color)
{
  bgColor = color;

  if (borderDrawn)
    drawBorder();
}

void Arduboy2Core::paint8Pixels(uint8_t pixels)
{
  // Not implemented
}

void Arduboy2Core::paintScreen(const uint8_t *image)
{
  paintScreen((uint8_t *)image, false);
}

void Arduboy2Core::paintScreen(uint8_t image[], bool clear)
{
  const uint16_t numCells = WIDTH*HEIGHT/8;

  startSPItransfer();

  setWriteRegion();
  for (int c = 0; c < WIDTH; c++)
  {
    for (int cell = c; cell < numCells; cell += WIDTH)
    {
      uint8_t pixels = image[cell];
      for (int b = 0; b < 4; b++)
      {
        const uint16_t p0 = (pixels & 0b01) ? pixelColor : bgColor;
        const uint16_t p1 = (pixels & 0b10) ? pixelColor : bgColor;
        SPItransfer(p0 >> 4);
        SPItransfer(((p0 & 0xF) << 4) | (p1 >> 8));
        SPItransfer(p1);
        pixels = pixels >> 2;
      }
    }
  }

  endSPItransfer();

  if (clear)
    memset(image, 0, numCells);
}

void Arduboy2Core::blank()
{
  startSPItransfer();

  setWriteRegion();
  for (int i = 0; i < WIDTH*HEIGHT/2; i++)
  {
    SPItransfer(bgColor >> 4);
    SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    SPItransfer(bgColor);
  }

  endSPItransfer();
}

void Arduboy2Core::sendLCDCommand(uint8_t command)
{
  LCDCommandMode();
  SPItransfer(command);
  LCDDataMode();
}

static void setWriteRegion(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
  Arduboy2Core::sendLCDCommand(ST77XX_CASET);  //  Column addr set
  Arduboy2Core::SPItransfer(0);
  Arduboy2Core::SPItransfer(y);                //  y start
  Arduboy2Core::SPItransfer(0);
  Arduboy2Core::SPItransfer(y + height - 1);   //  y end

  Arduboy2Core::sendLCDCommand(ST77XX_RASET);  //  Row addr set
  Arduboy2Core::SPItransfer(0);
  Arduboy2Core::SPItransfer(x);                //  x start
  Arduboy2Core::SPItransfer(0);
  Arduboy2Core::SPItransfer(x + width - 1);    //  x end

  Arduboy2Core::sendLCDCommand(ST77XX_RAMWR);  //  Initialize write to display RAM
}

static void drawBorder()
{
  const uint8_t innerGap = 1;
  const uint8_t windowWidth = WIDTH+innerGap*2;
  const uint8_t windowHeight = HEIGHT+innerGap*2;
  const uint8_t marginX = (TFT_WIDTH-windowWidth)/2;
  const uint8_t marginY = (TFT_HEIGHT-windowHeight)/2;

  // draw border fill

  Arduboy2Core::startSPItransfer();

  setWriteRegion(0, 0, TFT_WIDTH, marginY-1);
  for (int i = 0; i < (TFT_WIDTH*(marginY-1))/2; i++)
  {
    Arduboy2Core::SPItransfer(borderFillColor >> 4);
    Arduboy2Core::SPItransfer(((borderFillColor & 0xF) << 4) | (borderFillColor >> 8));
    Arduboy2Core::SPItransfer(borderFillColor);
  }

  setWriteRegion(0, TFT_HEIGHT-(marginY-1), TFT_WIDTH, marginY-1);
  for (int i = 0; i < (TFT_WIDTH*(marginY-1))/2; i++)
  {
    Arduboy2Core::SPItransfer(borderFillColor >> 4);
    Arduboy2Core::SPItransfer(((borderFillColor & 0xF) << 4) | (borderFillColor >> 8));
    Arduboy2Core::SPItransfer(borderFillColor);
  }

  setWriteRegion(0, marginY-1, marginX-1, windowHeight+4);
  for (int i = 0; i < ((marginX-1)*(windowHeight+4))/2; i++)
  {
    Arduboy2Core::SPItransfer(borderFillColor >> 4);
    Arduboy2Core::SPItransfer(((borderFillColor & 0xF) << 4) | (borderFillColor >> 8));
    Arduboy2Core::SPItransfer(borderFillColor);
  }

  setWriteRegion(TFT_WIDTH-(marginX-1), marginY-1, marginX-1, windowHeight+4);
  for (int i = 0; i < ((marginX-1)*(windowHeight+4))/2; i++)
  {
    Arduboy2Core::SPItransfer(borderFillColor >> 4);
    Arduboy2Core::SPItransfer(((borderFillColor & 0xF) << 4) | (borderFillColor >> 8));
    Arduboy2Core::SPItransfer(borderFillColor);
  }

  // draw border lines

  setWriteRegion(marginX-1, marginY-1, windowWidth+2, 1);
  for (int i = 0; i < (windowWidth+2)/2; i++)
  {
    Arduboy2Core::SPItransfer(borderLineColor >> 4);
    Arduboy2Core::SPItransfer(((borderLineColor & 0xF) << 4) | (borderLineColor >> 8));
    Arduboy2Core::SPItransfer(borderLineColor);
  }

  setWriteRegion(marginX-1, TFT_HEIGHT-marginY, windowWidth+2, 1);
  for (int i = 0; i < (windowWidth+2)/2; i++)
  {
    Arduboy2Core::SPItransfer(borderLineColor >> 4);
    Arduboy2Core::SPItransfer(((borderLineColor & 0xF) << 4) | (borderLineColor >> 8));
    Arduboy2Core::SPItransfer(borderLineColor);
  }

  setWriteRegion(marginX-1, marginY, 1, windowHeight);
  for (int i = 0; i < windowHeight/2; i++)
  {
    Arduboy2Core::SPItransfer(borderLineColor >> 4);
    Arduboy2Core::SPItransfer(((borderLineColor & 0xF) << 4) | (borderLineColor >> 8));
    Arduboy2Core::SPItransfer(borderLineColor);
  }

  setWriteRegion(TFT_WIDTH-marginX, marginY, 1, windowHeight);
  for (int i = 0; i < windowHeight/2; i++)
  {
    Arduboy2Core::SPItransfer(borderLineColor >> 4);
    Arduboy2Core::SPItransfer(((borderLineColor & 0xF) << 4) | (borderLineColor >> 8));
    Arduboy2Core::SPItransfer(borderLineColor);
  }

  // draw gap around display area
  setWriteRegion(marginX, marginY, windowWidth, innerGap);
  for (int i = 0; i < (windowWidth*innerGap)/2; i++)
  {
    Arduboy2Core::SPItransfer(bgColor >> 4);
    Arduboy2Core::SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    Arduboy2Core::SPItransfer(bgColor);
  }

  setWriteRegion(marginX, TFT_HEIGHT-marginY-innerGap, windowWidth, innerGap);
  for (int i = 0; i < (windowWidth*innerGap)/2; i++)
  {
    Arduboy2Core::SPItransfer(bgColor >> 4);
    Arduboy2Core::SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    Arduboy2Core::SPItransfer(bgColor);
  }

  setWriteRegion(marginX, marginY+innerGap, innerGap, HEIGHT);
  for (int i = 0; i < HEIGHT*innerGap/2; i++)
  {
    Arduboy2Core::SPItransfer(bgColor >> 4);
    Arduboy2Core::SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    Arduboy2Core::SPItransfer(bgColor);
  }

  setWriteRegion(TFT_WIDTH-marginX-innerGap, marginY+innerGap, innerGap, HEIGHT);
  for (int i = 0; i < HEIGHT*innerGap/2; i++)
  {
    Arduboy2Core::SPItransfer(bgColor >> 4);
    Arduboy2Core::SPItransfer(((bgColor & 0xF) << 4) | (bgColor >> 8));
    Arduboy2Core::SPItransfer(bgColor);
  }

  Arduboy2Core::endSPItransfer();

  borderDrawn = true;
}

// invert the display or set to normal
// when inverted, a pixel set to 0 will be on
void Arduboy2Core::invert(bool inverse)
{
  if (inverse == inverted)
    return;

  inverted = inverse;

  // keep LED bar color agnostic of inversion
  drawLEDs();

  startSPItransfer();
  sendLCDCommand(inverse ? ST77XX_INVON : ST77XX_INVOFF);
  endSPItransfer();
}

// turn all display pixels on, ignoring buffer contents
// or set to normal buffer display
void Arduboy2Core::allPixelsOn(bool on)
{
  startSPItransfer();
  sendLCDCommand(on ? ST77XX_DISPOFF : ST77XX_DISPON);
  endSPItransfer();
  delayShort(100);
}

// flip the display vertically or set to normal
void Arduboy2Core::flipVertical(bool flipped)
{
  if (flipped)
  {
    MADCTL |= ST77XX_MADCTL_MX;
  }
  else
  {
    MADCTL &= ~ST77XX_MADCTL_MX;
  }
  startSPItransfer();
  sendLCDCommand(ST77XX_MADCTL);
  SPItransfer(MADCTL);
  endSPItransfer();
}

// flip the display horizontally or set to normal
void Arduboy2Core::flipHorizontal(bool flipped)
{
  if (flipped)
  {
    MADCTL &= ~ST77XX_MADCTL_MY;
  }
  else
  {
    MADCTL |= ST77XX_MADCTL_MY;
  }
  startSPItransfer();
  sendLCDCommand(ST77XX_MADCTL);
  SPItransfer(MADCTL);
  endSPItransfer();
}


/* RGB LED */

void Arduboy2Core::setRGBled(uint8_t red, uint8_t green, uint8_t blue)
{
  LEDs[RED_LED] = red;
  LEDs[GREEN_LED] = green;
  LEDs[BLUE_LED] = blue;
  drawLEDs();
}

void Arduboy2Core::setRGBled(uint8_t color, uint8_t val)
{
  LEDs[color] = val;
  drawLEDs();
}

void Arduboy2Core::freeRGBled()
{
  // NOP
}

void Arduboy2Core::digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue)
{
  LEDs[RED_LED] = (red == RGB_ON ? 0xFF : 0);
  LEDs[GREEN_LED] = (green == RGB_ON ? 0xFF : 0);
  LEDs[BLUE_LED] = (blue == RGB_ON ? 0xFF : 0);
  drawLEDs();
}

void Arduboy2Core::digitalWriteRGB(uint8_t color, uint8_t val)
{
  LEDs[color] = (val == RGB_ON ? 0xFF : 0);
  drawLEDs();
}

static void drawLEDs()
{
  const uint8_t red = inverted ? 0xFF - LEDs[RED_LED] : LEDs[RED_LED];
  const uint8_t green = inverted ? 0xFF - LEDs[GREEN_LED] : LEDs[GREEN_LED];
  const uint8_t blue = inverted ? 0xFF - LEDs[BLUE_LED] : LEDs[BLUE_LED];

  Arduboy2Core::startSPItransfer();

  setWriteRegion(0, (MADCTL & ST77XX_MADCTL_MX) ? 0 : TFT_HEIGHT-4, TFT_WIDTH, 4);
  for (int i = 0; i < (TFT_WIDTH*5)/2; i++)
  {
    const uint16_t color = COLOR((red*0xF)/0xFF, (green*0xF)/0xFF , (blue*0xF)/0xFF);
    Arduboy2Core::SPItransfer(color >> 4);
    Arduboy2Core::SPItransfer(((color & 0xF) << 4) | (color >> 8));
    Arduboy2Core::SPItransfer(color);
  }

  Arduboy2Core::endSPItransfer();
}


/* Buttons */

uint8_t Arduboy2Core::buttonsState()
{
  uint32_t btns = ~(*portInputRegister(IO_PORT));
  return (
    (((btns & MASK_BUTTON_A) != 0) << A_BUTTON_BIT) |
    (((btns & MASK_BUTTON_B) != 0) << B_BUTTON_BIT) |
    (((btns & MASK_BUTTON_UP) != 0) << UP_BUTTON_BIT) |
    (((btns & MASK_BUTTON_DOWN) != 0) << DOWN_BUTTON_BIT) |
    (((btns & MASK_BUTTON_LEFT) != 0) << LEFT_BUTTON_BIT) |
    (((btns & MASK_BUTTON_RIGHT) != 0) << RIGHT_BUTTON_BIT) |
    (((btns & MASK_BUTTON_START) != 0) << START_BUTTON_BIT) |
    (((btns & MASK_BUTTON_SELECT) != 0) << SELECT_BUTTON_BIT)
  );
}

// delay in ms with 16 bit duration
void Arduboy2Core::delayShort(uint16_t ms)
{
  delay((unsigned long)ms);
}

void Arduboy2Core::exitToBootloader()
{
  noInterrupts();
  while(true) {}
}

void Arduboy2Core::mainNoUSB()
{
  init();

  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);

  // Delay to give time for the pin to be pulled high if it was floating
  delayShort(10);

  // If the down button is pressed
  if (!digitalRead(PIN_BUTTON_DOWN)) {
    exitToBootloader();
  }

  // The remainder is a copy of the Arduino main() function with the
  // USB code and other unneeded code removed.

  setup();

  for (;;) {
    loop();
  }
}
