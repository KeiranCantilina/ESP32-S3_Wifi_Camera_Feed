#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include "pins_arduino.h"
#include <TFT_eSPI.h> // Hardware-specific library. MUST ADJUST user settings file in library
#include "SPIFFS.h"



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
uint64_t sum = 0;
uint64_t xSum = 0;
uint64_t ySum = 0;
uint64_t numberTargets = 0;



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
  tft.setRotation(3);

  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(200);
  }

  wifiMulti.addAP("ESP32-CAM", "password");

  tft.fillScreen(TFT_BLACK); // erase display to black
  tft.setCursor(46, 0);
  tft.setTextSize(3);
  //tft.setFont(FONT_12x16);
  tft.setTextColor(TFT_RED);
  tft.print("Dot Finder V1.0");
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
    //bin16print(bmpBuffer[51200]);

    // Bit order and endian-ness test
    //uint16_t test_bits = 1;
    // for(int i = 0; i<(rawWidth*rawHeight); i++){
    //   bmpBuffer[i] = leftRotate(test_bits, i/6400);
    //   if (i%320 == 0){bmpBuffer[i] = 0b1111111111111111;}
    //   if (i%6400 == 0){bmpBuffer[i] = 0b0000000000000000;}
    //   if (i < 100){bmpBuffer[i] = 0b0000000010000000;}
    //   //bin16print(bmpBuffer[i]);
    // }//--------------0b0000000000000000;

    //Binary Thresholding
    long average = sum/102400;
    
    // Serial.print(sum);
    // Serial.print(", ");
    // Serial.println(average);
    for(int i = 0; i<rawWidth*rawHeight; i++){
      if(getGreen(bmpBuffer[i])>(average/2)){
        
      }
      else{
        bmpBuffer[i] = 0b1110000000000111;
        // Calculate coordinates
        xSum += i % rawWidth;
        ySum += i / rawWidth;
        numberTargets++;
      }
    }

    // Display from bmp buffer
    tft.pushImage(0, 0, rawWidth, rawHeight, bmpBuffer);
    tft.setCursor(23, 23);
    if(numberTargets>0){
      Serial.print(xSum/numberTargets);
      Serial.print(", ");
      Serial.println(ySum/numberTargets);
      //Serial.print("Number Targets: ");
      //Serial.println(numberTargets);
      tft.print(xSum/numberTargets);
      tft.print(", ");
      tft.print(ySum/numberTargets);
      tft.drawCircle((int32_t)xSum/numberTargets, (int32_t)ySum/numberTargets, 10, TFT_RED);
      }
    else{
      tft.print("NA");
    }
  
    // Free memory
    free(imageBuffer); 
    imageBuffer = NULL;
    sum = 0;
    numberTargets = 0;
    xSum = 0;
    ySum = 0;

    // Keep bmp buffer allocated in PSRAM

    http.end();

    //while(true);
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

    // Do single channel conversion to take average
    sum += getGreen(pixels[i]);
  }
}


void bin16print(uint16_t value) {
  for (int i = 15; i >= 0; i--) { // Loop from the most significant bit (15) down to 0
    // Use bitRead() to check the value of each bit
    Serial.print(bitRead(value, i));
    if (i==5 || i == 11){
      Serial.print(" ");
    }
  }
  Serial.println(" ");
  Serial.println(value, HEX);
}


uint16_t leftRotate(uint16_t bits, uint8_t positions) {
    // Left shift the number and OR it with the bits shifted in from the right
    return (bits << positions) | (bits >> (16 - positions)); 
}


uint8_t rgb565_to_grayscale(uint16_t color) {
  // Extract the components from the 16-bit RGB565 value
  // R component (5 bits)
  uint8_t r = getRed(color);
  // G component (6 bits)
  uint8_t g = getGreen(color);
  // B component (5 bits)
  uint8_t b = getBlue(color);

  // Scale the 5-bit R and B components and 6-bit G component to an 8-bit range (0-255)
  // This is a simplified scaling, more precise scaling can be used, but this is a good starting point
  uint8_t r8 = (r * 527 + 23) >> 6; // ~255/31
  uint8_t g8 = (g * 259 + 33) >> 6; // ~255/63
  uint8_t b8 = (b * 527 + 23) >> 6; // ~255/31

  // Apply a weighted average (luma) formula to get the grayscale value (0-255)
  // Using integer math with coefficients scaled to 1000 for precision
  uint16_t gray = (r8 * 212 + g8 * 715 + b8 * 72) / 1000;
  
  return (uint8_t)gray;
}

uint8_t getRed(uint16_t color){
  uint8_t r = (color >> 3) & 0b0000000000011111;
  return r;
}

uint8_t getGreen(uint16_t color){
  uint8_t g = (leftRotate(color, 3)) & 0b0000000000111111;
  return g;
}

uint8_t getBlue(uint16_t color){
  uint8_t b = (color >> 8) & 0b0000000000011111;
  return b;
}



