#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include "pins_arduino.h"
#include <TFT_eSPI.h> // Hardware-specific library. MUST ADJUST user settings file in library



// this function determines the minimum of two numbers
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

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

//#define DISPLAY_WIDTH 480
//#define DISPLAY_HEIGHT 320

WiFiMulti wifiMulti;

TFT_eSPI tft = TFT_eSPI();                   // Invoke custom library with default width and height

uint8_t* imageBuffer = NULL;
size_t imageSize = 0;
uint16_t rawWidth = 0;
uint16_t rawHeight = 0;

//const size_t BMP_SIZE = 102400; // sized for 320*320 bmp
const size_t BMP_SIZE = 153600; // sized for 320*480 bmp
uint16_t *bmpBuffer; 
size_t bmpSize = 0;



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

  // Start screen
  tft.init();
  tft.setRotation(1);

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
    Serial.println(bmpBuffer[51200], HEX);
    // for(int i = 0; i<rawWidth*rawHeight; i++){
    //   //bmpBuffer[i] = RGB565toRBG565(bmpBuffer[i]);
    //   bmpBuffer[i] = RGB565toBGR565(bmpBuffer[i]);
    // }
    

    // Display from bmp buffer
    tft.pushImage(0, 0, rawWidth, rawHeight, bmpBuffer);

    // Free memory
    free(imageBuffer); 
    imageBuffer = NULL;
    // Keep bmp buffer allocated in PSRAM

    http.end();
  }
}


void grabImage(){
  TJpgDec.getJpgSize(&rawWidth, &rawHeight, imageBuffer, imageSize); // Get image width and height
  TJpgDec.drawJpg(0, 0, imageBuffer, imageSize); // Draw the image, top left at 0,0
}


// TJpgDec.drawJpg callback function
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap){
  writeToBuffer(bitmap, x, y, w, h); // Write to buffer
  return 1; // Return 1 to decode next block
}


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


