#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_LSM303_U.h>
#include <Adafruit_NeoPixel.h>

// Setup the NeoPixels
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_RGB     Pixels are wired for RGB bitstream
//   NEO_GRB     Pixels are wired for GRB bitstream
//   NEO_KHZ400  400 KHz bitstream (e.g. FLORA pixels)
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(20, 10, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pants = Adafruit_NeoPixel(23, 6, NEO_GRB + NEO_KHZ800);

// Initalise the LSM303 Accelerometer
/* Assign a unique ID to this sensor at the same time */
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);
 
// Initalise the TCS34725 RGB Colour Sensor 
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// This defines the threshold for acceleromter movement
#define MOVE_THRESHOLD 300

/* Assign a unique ID to this sensor at the same time */
Adafruit_LSM303_Mag_Unified mag = Adafruit_LSM303_Mag_Unified(12345);

// Define Pi
const float Pi = 3.14159;

// Movement Threshold
int movementThreshold = 15;

// Max sensor readings for Smoothing
const int smoothReadMax = 10;

// Max colours to remember
const int colMemMax = 10;

// Colour sampling switch
int colourSample = 0;

// Onboad LED Pin
const int floraLED = 7;

// Onboard LED blink interval
int floraLEDint = millis();

// LED State for Flora onboard LED
int floraLEDstate = LOW;

// our RGB -> eye-recognized gamma color (uses too much ram)
//byte gammatable[256];

// Colour index
int colIndex = 0;

// Colour picker (red) array
int colRed[colMemMax];

// Colour picker (green) array
int colGreen[colMemMax];

// Colour picker (blue) array
int colBlue[colMemMax];

// colour sensor check interval
long nextColourCheck = millis();

// colour check interval
int colourCheckInterval = 1000;

// start/end colourCheck
int startEndColourCheck = 0;

// wait between pattern changes
long nextPatternChange = 0;

// Accelerometer based pattern change interval
long nextAccelPattern = millis();

// LSM303 check interval
long nextLSMcheck = millis();

// Data array index
int accelIndex = 0;

// Accelerometer smoothed averages
int accelAverageX = 0;
int smoothAccelX = 0;
int accelAverageY = 0;
int smoothAccelY = 0;
int accelAverageZ = 0;
int smoothAccelZ = 0;

// Accelerometer X array
int accelX[smoothReadMax];

// Accelerometer Y array
int accelY[smoothReadMax];

// Accelerometer Z array
int accelZ[smoothReadMax];

// Magnetic X array
int magX[smoothReadMax];

// Magnetic Y array;
int magY[smoothReadMax];

// Colour fade multiplier
int cMult = 0;

void setup() {
  // Setup serial for debugging
  Serial.begin(9600);
  
  Serial.println("Squidivisim gloves startup sequence");
  
  // Set the pin mode for the onboard LED
  pinMode(floraLED, OUTPUT);

  // Set the strip brightness
  strip.setBrightness(80);
  pants.setBrightness(80);
  
  // Setup the strips by clearing them
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'  
  
  // Setup the pants by clearning them
  pants.begin();
  pants.show();
  
  // Run a pattern on the RGB pixels to indicate initalisation
  pixelRandomSweep();
  pantsDanceRandom();
  
  /* Initalise the LSM303 Accelrometer sensor */
  if (!accel.begin()) {
    /* There was a problem detecting the LSM303 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    //while(1); // this will hang on no sensor detect
  }
  
  /* Initialise the LSM303 Magnetic sensor */
  if(!mag.begin()) {
    /* There was a problem detecting the LSM303 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    //while(1); // this will hang on no sensor detect
  }
  
  /* Initalise the TCS34725 colour sensor */
  if (tcs.begin()) {
    Serial.println("Found TCS34725 sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    //while(1); // this will hang on no sensor detect
  }
  
  // Set all the sensor reading arrays to 0
  for (int i = 0; i < smoothReadMax ; i++) {
    accelX[i] = 0;
    accelY[i] = 0;
    accelZ[i] = 0;
  }
  
  // Set all the colours in the colour index
  for (int i = 0 ; i < colMemMax ; i++) {
    colRed[i] = i;
    colGreen[i] = i;
    colBlue[i] = i;
  }
  
  // Run another pattern to indicate startup complete
  pixelRandomSweep();
  pantsDanceRandom();
  
  Serial.println("Squidivism gloves booted");
  
}

// Colour fading function
void colourFade(int nextRed, int nextGreen, int nextBlue) {
  // get the last colour of pixel 0
  uint32_t lastColour = strip.getPixelColor(0);
  
  // Convert 32-Bit merged colours to R/G/B
  int lastRed = (uint8_t)((lastColour & 0xFF0000) >> 16);
  int lastGreen = (uint8_t)((lastColour & 0x00FF00) >> 8);
  int lastBlue = (uint8_t)(lastColour & 0x0000FF);
  
  // fade each pixel between their previous and new colours
  for (int c = 0 ; c <= 10 ; c++) {
    // multiply each colour by a proportion of the current intermediate step, add the then devide by 2 times the max multiplication factor
    //uint32_t thisColour = ((lastColour * (10 - c)) + (nextColour * c)) / 20;
    
    int thisRed = ((lastRed * (10 - c)) + (nextRed * c)) / 20;
    int thisGreen = ((lastGreen * (10 - c)) + (nextGreen * c)) / 20;
    int thisBlue = ((lastBlue * (10 - c)) + (nextBlue * c)) / 20;
    
    for (int p = 0; p < strip.numPixels(); p++) {
      // set this pixel to this colour
      strip.setPixelColor(p,thisRed,thisGreen,thisBlue);
    }
    
    // show the latest change to the pixel colours
    strip.show();
  }
}

void pantsFadePattern(int nextRed, int nextGreen, int nextBlue) {
  // get the last colour of pixel 0
  uint32_t lastColour = pants.getPixelColor(0);
  
  // Convert 32-Bit merged colours to R/G/B
  int lastRed = (uint8_t)((lastColour & 0xFF0000) >> 16);
  int lastGreen = (uint8_t)((lastColour & 0x00FF00) >> 8);
  int lastBlue = (uint8_t)(lastColour & 0x0000FF);
  
  // fade each pixel between their previous and new colours
  for (int c = 0 ; c <= 10 ; c++) {
    int thisRed = ((lastRed * (10 - c)) + (nextRed * c)) / 20;
    int thisGreen = ((lastGreen * (10 - c)) + (nextGreen * c)) / 20;
    int thisBlue = ((lastBlue * (10 - c)) + (nextBlue * c)) / 20;
    
    for (int p = 0; p < strip.numPixels(); p++) {
      // set this pixel to this colour
      pants.setPixelColor(p,thisRed,thisGreen,thisBlue);
    }
    
    // show the latest change to the pixel colours
    pants.show();
  }
}

// pixel random sweep pattern
void pixelRandomSweep() {
  // generate some random values
  int randRed = random(10,70);
  int randGreen = random(11,70);
  int randBlue = random(12,70);
  
  for (int p = 0; p < strip.numPixels(); p++) {
    strip.setPixelColor(p,randRed,randGreen,randBlue);
    strip.show();
    // turn off the previous pixel if > 0
    if (p > 0) {
      strip.setPixelColor(p-1,0,0,0);
    }
    delay(10);
  }
  //int lastPixel = strip.numPixels();
  //strip.setPixelColor(lastPixel,0,0,0);
  //strip.show();
}

// Run a pattern down the pants
void pantsDanceRandom() {
  // generate some random values
  int randRed = random(10,70);
  int randGreen = random(11,70);
  int randBlue = random(12,70);
  
  for (int p = 0; p < pants.numPixels(); p++) {
    pants.setPixelColor(p,randRed,randGreen,randBlue);
    pants.show();
    // turn off the previous pixel if > 0
    if (p > 0) {
      pants.setPixelColor(p-1,0,0,0);
    }
    delay(10);
  }
}

void loop() {
  //Serial.println("Loop started");
  /*
  // Blink the onboard LED every 500 ms
  if (millis() - floraLEDint > 500) {
    if (floraLEDstate == LOW) {
      floraLEDstate = HIGH;
    }else{
      floraLEDstate = LOW;
    }
    
    // Write to the digital pin to turn the LED off/on
    digitalWrite(floraLED, floraLEDstate);
    
    // Set the interval ahead 1s
    floraLEDint = millis();
  }
  */
  
  // Limit the intervals that we do LSM303 readings
  if (millis() - nextLSMcheck > 200) {
     Serial.println("Running LSM303 sensor check");
     
     // Get a new sensor event 
     sensors_event_t event; 
     accel.getEvent(&event);
     
     // subtract the last reading
     smoothAccelX = smoothAccelX - accelX[accelIndex];
     smoothAccelY = smoothAccelY - accelY[accelIndex];
     smoothAccelZ = smoothAccelZ - accelZ[accelIndex];
     
     // Drop the accelerometer data into arrays
     accelX[accelIndex] = event.acceleration.x;
     accelY[accelIndex] = event.acceleration.y;
     accelZ[accelIndex] = event.acceleration.z;
     
     // Add the reading to the total
     smoothAccelX = smoothAccelX + accelX[accelIndex];
     smoothAccelY = smoothAccelY + accelY[accelIndex];
     smoothAccelZ = smoothAccelZ + accelZ[accelIndex];
     
     // iterate the accelerometer smoothing index
     accelIndex++;
     
     // check if we've reached the end of the smoothing size
     if (accelIndex >= smoothReadMax) {
       accelIndex = 0;
     }
     
     // Calculate the average
     accelAverageX = smoothAccelX / smoothReadMax;
     accelAverageY = smoothAccelY / smoothReadMax;
     accelAverageZ = smoothAccelZ / smoothReadMax;
     
     // Display the results (acceleration is measured in m/s^2) 
     Serial.println("Accelerometer Data: ");
     Serial.print(" X: "); Serial.print(event.acceleration.x);
     Serial.print(" Y: "); Serial.print(event.acceleration.y);
     Serial.print(" Z: "); Serial.print(event.acceleration.z);
     Serial.println(" ");
     
     // Get the magnitude (length) of the 3 axis vector
     // http://en.wikipedia.org/wiki/Euclidean_vector#Length
     double storedVector = event.acceleration.x*event.acceleration.x;
     storedVector += event.acceleration.y*event.acceleration.y;
     storedVector += event.acceleration.z*event.acceleration.z;
     storedVector = sqrt(storedVector);
     Serial.print("Len: "); Serial.println(storedVector);
     
     // If the length exceeds 11 - we're making a big movement
     if (int(storedVector) > movementThreshold) {
       // Switch between colour sampling and colour generating
       if (colourSample > 0) {
         Serial.println("###### Switching to Accelerometer Pattern Mode #####");
         colourSample = 0;
         pixelRandomSweep();
       }else{
         Serial.println("###### Switching to Colour Reading Mode #####");
         colourSample = 1;
         pixelRandomSweep();
       }
     }
     
     Serial.println(" Averaged: ");
     Serial.print(" X: "); Serial.print(accelAverageX);
     Serial.print(" Y: "); Serial.print(accelAverageY);
     Serial.print(" Z: "); Serial.println(accelAverageZ);
     Serial.println(" After abs conversion: ");
     Serial.print(" X: "); Serial.print(abs(accelAverageX));
     Serial.print(" Y: "); Serial.print(abs(accelAverageY));
     Serial.print(" Z: "); Serial.println(abs(accelAverageZ));
     
     // If colour sample mode is off
     if (colourSample == 0) {
       // Fade to colours generated by the accelerometer average values
       if (millis() - nextAccelPattern > 500) {
         Serial.println("Fading Accelerometer pattern");
         
         int redX = abs(accelAverageX) + 10;
         int greenY = abs(accelAverageY) + 10;
         int blueZ = abs(accelAverageZ) + 10;
         
         // run the colour fade function
         colourFade(redX,greenY,blueZ);
         pantsFadePattern(redX,greenY,blueZ);
         
         // Schedule the next pattern change
         nextAccelPattern = millis();
       }
     }
  
     // Load the Magnetic sensor event
     mag.getEvent(&event);
    
    /*
    // Drop the Magnetic data into arrays
     magX[index] = event.magnetic.x;
     magY[index] = event.magnetic.y;
    */ 
     // Calculate the angle of the vector y,x
     float heading = (atan2(event.magnetic.y,event.magnetic.x) * 180) / Pi;
     
     // Normalize to 0-360
     if (heading < 0) {
       heading = 360 + heading;
     }
     
    
    // Output the compass data to the serial console
    Serial.println("Compass Data: ");
    Serial.print(" X: "); Serial.print(event.magnetic.x);
    Serial.print(" Y: "); Serial.print(event.magnetic.y);
    Serial.print(" Heading: ");
    Serial.println(heading);
    
    // delay the next LSM303 check by 50ms
    nextLSMcheck = millis();
    
    Serial.println("LSM Sensor check completed");
  }
  
  // If colour sample mode is enabled
  if (colourSample == 1) {
    // if the next colour check is scheduled
    if (millis() - nextColourCheck > colourCheckInterval) {
      if (startEndColourCheck == 0) {
        Serial.println("Start taking colour reading");
        
        // turn on the LED for the sensor
        tcs.setInterrupt(false);      // turn on LED
        colourCheckInterval = 60;
        startEndColourCheck = 1;
      }else{
        Serial.println("finish taking colour reading");
        
        // Setup the vars for the raw sensor data
        uint16_t clear, red, green, blue;
        
        // Get the RGB data from the sensor
        tcs.getRawData(&red, &green, &blue, &clear);
        
        // turn the LED for the sensor off
        tcs.setInterrupt(true);  // turn off LED
        
        // Print the data out to the serial console for debugging
        Serial.println("Raw Colour Data:");
        Serial.print(" C: "); Serial.print(clear);
        Serial.print(" R: "); Serial.print(red);
        Serial.print(" G: "); Serial.print(green);
        Serial.print(" B: "); Serial.print(blue);
        Serial.println(" ");
        
        // Figure out some basic hex code for visualization
        uint32_t sum = red;
        sum += green;
        sum += blue;
        sum = clear;
        float r, g, b;
        r = red; r /= sum;
        g = green; g /= sum;
        b = blue; b /= sum;
        r *= 256; g *= 256; b *= 256;
        
        // Output the hex data to the serial console for debugging
        Serial.println("Processed Colour Data:\t");
        //Serial.print((int)r, HEX); Serial.print((int)g, HEX); Serial.print((int)b, HEX);
        Serial.print("R: "); Serial.print((int)r ); Serial.print(" G: "); Serial.print((int)g);Serial.print(" B: ");  Serial.println((int)b );
        //Serial.print("R:\t"); Serial.print(r); Serial.print("\tG:\t"); Serial.print(g); Serial.print("\tB:\t"); Serial.print(b);
        Serial.println(" ");
        
        // check the size of the colour index
        if (colIndex > colMemMax) {
          colIndex = 0;
        }
        
        colRed[colIndex] = int(r);
        colGreen[colIndex] = int(g);
        colBlue[colIndex] = int(b);
        
        colourFade(colRed[colIndex],colGreen[colIndex],colBlue[colIndex]);
        pantsFadePattern(colRed[colIndex],colGreen[colIndex],colBlue[colIndex]);
        
        colIndex++;
        
        Serial.println("Colour sensor check completed");
        
        // schedule the next colour sensor check
        nextColourCheck = millis();
        colourCheckInterval = 1000;
        startEndColourCheck = 0;
      }
      
      Serial.println("Fading between last 10 colour samples");
      
      // Cycle through the colour samples
      for (int i = 0; i < colMemMax ; i++) {
        // Fade between the last colour and the next colour in the table
        colourFade(colRed[i],colGreen[i],colBlue[i]);
        pantsFadePattern(colRed[i],colGreen[i],colBlue[i]);
      }
      
      Serial.println("Finished colour fades");
    }
  }
}


