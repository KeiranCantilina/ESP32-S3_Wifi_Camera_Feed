#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <JPEGDEC.h>
#include "pins_arduino.h"
#include <bb_spi_lcd.h>




JPEGDEC jpeg;

// These are 'flexible' lines that can be changed
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8 // RST can be set to -1 if you tie it to Arduino's reset

#define CS_PIN TFT_CS
#define DC_PIN TFT_DC
#define LED_PIN -1
#define RESET_PIN TFT_RST
#define MISO_PIN MISO
#define MOSI_PIN MOSI
#define SCK_PIN SCK

// SoftSPI - note that on some processors this might be *faster* than hardware SPI!
//Adafruit_ST7796S_kbv tft = Adafruit_ST7796S_kbv(TFT_CS, TFT_DC, MOSI, SCK, TFT_RST, MISO);

#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 2320

WiFiMulti wifiMulti;
BB_SPI_LCD tft;

uint8_t* imageBuffer = NULL;
size_t imageSize = 0;


int drawMCUs(JPEGDRAW *pDraw)
{
  int iCount;
  iCount = pDraw->iWidth * pDraw->iHeight; // number of pixels to draw in this call
//  Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  tft.setAddrWindow(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  tft.pushPixels(pDraw->pPixels, iCount, DRAW_TO_LCD | DRAW_WITH_DMA);
  return 1; // returning true (1) tells JPEGDEC to continue decoding. Returning false (0) would quit decoding immediately.
} /* drawMCUs() */

void setup() {

  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting...");
  Serial.println();
  Serial.println();
  Serial.println();

  tft.begin(LCD_ST7796, FLAGS_FLIPX, 40000000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);


  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  wifiMulti.addAP("ESP32-CAM", "password");

  tft.fillScreen(TFT_BLACK); // erase display to black
  tft.setCursor(46, 0);
  tft.setTextSize(3);
  tft.setFont(FONT_12x16);
  tft.setTextColor(TFT_GREEN);
  tft.print("JPEG Test");
}

void loop() {
  // wait for WiFi connection
  if ((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    // configure traged server and url
    //http.begin("https://www.howsmyssl.com/a/check", ca); //HTTPS
    http.begin("http://192.168.137.163/capture");  //HTTP

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        imageSize = http.getSize(); // get payload size
        Serial.printf("Image size: %d bytes\n", imageSize);
        if (imageSize > 0) {
          // Allocate memory for the buffer
          // Use heap_caps_malloc if using ESP-IDF or for PSRAM on WROVER
          // For standard Arduino, `new` or `malloc` is generally used.
          imageBuffer = (uint8_t*) malloc(imageSize);
          if (imageBuffer == NULL) {
            Serial.println("Error: Failed to allocate memory for image buffer");
            return;
          }

          // Read the data into the buffer
          http.getStream().readBytes((uint8_t*)imageBuffer, imageSize);
          Serial.println("Image successfully read into buffer");

          // Call display method
          grabImage();
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    free(imageBuffer); 
    imageBuffer = NULL;
    http.end();
  }
}

void grabImage(){
  if (jpeg.openRAM((uint8_t*)imageBuffer, imageSize, drawMCUs)){
    Serial.println("Successfully opened JPEG image");
    Serial.printf("Image size: %d x %d, orientation: %d, bpp: %d\n", jpeg.getWidth(),
      jpeg.getHeight(), jpeg.getOrientation(), jpeg.getBpp());
    jpeg.setPixelType(RGB565_BIG_ENDIAN); // The SPI LCD wants the 16-bit pixels in little-endian order  

    jpeg.decode(0,0,0);
    jpeg.close();
  }
  else{
    Serial.println("Failed to open image!");
  }
}
