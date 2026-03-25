#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <SPI.h>
//#include <JPEGDEC.h>
#include <TJpg_Decoder.h>
#include "pins_arduino.h"
//#include <bb_spi_lcd.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>



// this function determines the minimum of two numbers
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

//JPEGDEC jpeg;

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

//#define DISPLAY_WIDTH 480
//#define DISPLAY_HEIGHT 2320

WiFiMulti wifiMulti;
//BB_SPI_LCD tft;
TFT_eSPI tft = TFT_eSPI();                   // Invoke custom library with default width and height

uint8_t* imageBuffer = NULL;
size_t imageSize = 0;
uint16_t rawWidth = 0;
uint16_t rawHeight = 0;

//const size_t BMP_SIZE = 102400; // sized for 320*320 bmp
const size_t BMP_SIZE = 153600; // sized for 320*480 bmp
uint16_t *bmpBuffer; 
size_t bmpSize = 0;

// Method to make sure jpeg decode chunks get written to buffer in right order. 
void writeToBuffer(uint16_t *pixels, int x, int y, int width, int height){
  
  

  // Chunks are width x Height pixel blobs that don't fit nicely into a linear buffer and may come at us out of order
  int numberPixels = width*height; // Number of pixels in this chunk
  
  
  int skipSize = rawWidth - width; // How many pixels to skip in the buffer at end of each chunk row to get back to starting column
  int startAt = (rawWidth*y) + x; // Where in the buffer to start writing
  int index = startAt; // Init index
  int skipIndex = 0; // Keep track of where we are in the chunk row.

  // Debug print
  //Serial.printf("# = %d, skip = %d, start = %d, Draw pos = %d,%d. size = %d x %d\n", numberPixels, skipSize, startAt, x, y, width, height);

  // For every pixel in this chunk...
  for (int i = 0; i < numberPixels; i++) {

    // Convert RGB888 to RGB565
    // uint8_t r8 = pixels[i*3];
    // uint8_t g8 = pixels[1+(i*3)];
    // uint8_t b8 = pixels[2+(i*3)];
    // uint16_t r5, g6, b5, val;
    // b5 =  (b8  >> 3)        & 0x001F;
    // g6 = ((g8  >> 2) <<  5) & 0x07E0;
    // r5 = ((r8  >> 3) << 11) & 0xF800;
    // val = (r5 | g6 | b5);

    // Put values into buffer
    //bmpBuffer[index] = val;
    bmpBuffer[index] = pixels[i]; // Write pixel to puffer
    if(skipIndex<width-1){ // If we're not at the end of a chunk row, continue on as normal
      index++;
      skipIndex++;
    }
    else{ // If we're at the end of a chunk row...
      index += skipSize+1; // Skip!
      skipIndex = 0; // Reset chunk row index (as we're starting a new row of the chunk)
    }
  }
}

// int drawMCUs(JPEGDRAW *pDraw)
// {
//   int iCount;
//   iCount = pDraw->iWidth * pDraw->iHeight; // number of pixels to draw in this call
//   //Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  
//   // Shove chunks into bitmap buffer
//   writeToBuffer(pDraw->pPixels, pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  
//   // Debug: write chunks to display directly from jpg decode output
//   //tft.setAddrWindow(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
//   //tft.pushPixels(pDraw->pPixels, iCount, DRAW_TO_LCD | DRAW_WITH_DMA);
//   //tft.pushPixels(pDraw->pPixels, iCount);

//   return 1; // returning true (1) tells JPEGDEC to continue decoding. Returning false (0) would quit decoding immediately.
// } /* drawMCUs() */

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap){
  
  //tft.setAddrWindow(0, 0, w, h);
  //tft.pushPixels(bitmap, rawWidth*rawHeight);
  //tft.pushImage(x, y, w, h, bitmap);
  writeToBuffer(bitmap, x, y, w, h);
  // Return 1 to decode next block
  return 1;
}

void setup() {

  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting...");
  Serial.println();
  Serial.println();
  Serial.println();

  // PSRAM memory allocation
  if (psramInit()) {
    Serial.println("PSRAM Initialized");
  } 
  else {
    Serial.println("PSRAM not available or failed to initialize");
  }
  bmpBuffer = (uint16_t*) ps_malloc(BMP_SIZE * sizeof(uint16_t));

  //tft.begin(LCD_ILI9488, FLAGS_FLIPX, 80000000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);
  //tft.begin(LCD_ST7796, FLAGS_FLIPX, 80000000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);
  tft.init();
  //tft.setRotation(270);

  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  wifiMulti.addAP("ESP32-CAM", "password");

  tft.fillScreen(TFT_BLACK); // erase display to black
  tft.setCursor(46, 0);
  tft.setTextSize(3);
  //tft.setFont(FONT_12x16);
  tft.setTextColor(TFT_GREEN);
  tft.print("JPEG Test");
}

void loop() {
  // wait for WiFi connection
  if ((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    //Serial.print("[HTTP] begin...\n");

    // configure traged server and url
    http.begin("http://192.168.137.163/capture");  //HTTP

    //Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      //Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        imageSize = http.getSize(); // get payload size
        //Serial.printf("Image size: %d bytes\n", imageSize);
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
          //Serial.println("Image successfully read into buffer");

          // Call jpeg decode method
          grabImage();
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    // Do stuff with bmp buffer
    //-------------- TO DO HERE --------------------------------
    // Take average of all pixels (maybe grab this at for loop during decode)
    // Iterate through buffer doing bool compare. Above = white, else black
    // OR, swap red and blue. Let's try that first.
    //Serial.println(bmpBuffer[51200]);
    // for(int i = 0; i<rawWidth*rawHeight; i++){
    //   //bmpBuffer[i] = RGB565toRBG565(bmpBuffer[i]);
    //   bmpBuffer[i] = RGB565toBGR565(bmpBuffer[i]);
    // }
    

    // Display from bmp buffer
    //tft.setAddrWindow(0, 0, rawWidth, rawHeight);
    //tft.pushPixels(bmpBuffer, rawWidth*rawHeight);
    tft.pushImage(0, 0, 320, 320, bmpBuffer);

    // Free memory
    free(imageBuffer); 
    imageBuffer = NULL;
    // Keep bmp buffer allocated in PSRAM

    http.end();
  }
}

void grabImage(){
  // Open image
  // if (jpeg.openRAM((uint8_t*)imageBuffer, imageSize, drawMCUs)){
  //   //Serial.println("Successfully opened JPEG image");
  //   //Serial.printf("Image size: %d x %d, orientation: %d, bpp: %d\n", jpeg.getWidth(),
  //     //jpeg.getHeight(), jpeg.getOrientation(), jpeg.getBpp());

  //   //jpeg.setPixelType(RGB565_BIG_ENDIAN); // The SPI LCD wants the 16-bit pixels in little-endian order  
  //   jpeg.SetPixelType(RGB8888);
  //   bmpSize = jpeg.getHeight()*jpeg.getWidth();
  //   //Serial.print("Bitmap Size: ");
  //     //Serial.println(bmpSize);
    
  //   // Call decode method from library
  //   jpeg.decode(0,0,0); // triggers callback "drawMCUs" multiple times, once for each decoded chunk
  //   jpeg.close();
  // }
  // else{
  //   Serial.println("Failed to open image!");
  // }

  // Get image width and height
  TJpgDec.getJpgSize(&rawWidth, &rawHeight, imageBuffer, imageSize);

  // Draw the image, top left at 0,0
  TJpgDec.drawJpg(0, 0, imageBuffer, imageSize);
}

uint16_t RGB565toBGR565(uint16_t rgb) {
    // RGB565 format: RRRRRGGGGGGBBBBB
    // Extracts and masks components
    uint16_t red   = (rgb & 0xF800) >> 11; // 5 bits
    uint16_t green = (rgb & 0x07E0);       // 6 bits (keep in place)
    uint16_t blue  = (rgb & 0x001F);       // 5 bits

    // BGR565 format: BBBBBGGGGGGRRRRR
    // Recombine by shifting blue to top, keeping green, and moving red to bottom
    return (blue << 11) | green | red;
}




