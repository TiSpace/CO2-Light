
/*
 * *******************************************************************************


     CO2 traffic light
     measure concentration of (e)CO2 indoor and illuminate by colours.
     In addition measured values are shown as clear text on an OLED

     sensors:    - MQ-2
                 - BME280
                 - CSS811

   CO2 Limits
   <   700ppm:          uncritical for class rooms
   <  1000ppm:          hygienic unctirical
      1000ppm-2000ppm:  critical
   >  2000ppm:          inacceptable
   (source: https://www.umweltbundesamt.de/sites/default/files/medien/pdfs/kohlendioxid_2008.pdf)

   https://www.umwelt-campus.de/forschung/projekte/iot-werkstatt/ideen-zur-corona-krise
 * *******************************************************************************
*/


/*   =======================================================
              Includes & Inits
     =======================================================
*/
#include <Arduino.h>
#include "settings.h"
#include <Adafruit_NeoPixel.h>    //https://github.com/adafruit/Adafruit_NeoPixel
#include <Wire.h>
#include <U8x8lib.h>              //https://github.com/olikraus/u8g2   https://github.com/olikraus/u8g2/wiki/u8x8reference
#include <Adafruit_Sensor.h>

#include <Adafruit_BME280.h>

#ifdef sensorMQ2
#include <MQUnifiedsensor.h>
#include "MQ2Lib.h"
#endif

//#include <OneButton.h>          //https://github.com/mathertel/OneButton


#include <Adafruit_CCS811.h>    //https://github.com/adafruit/Adafruit_CCS811

#include <RunningMedian.h>

// create instances
Adafruit_CCS811 ccs;
Adafruit_NeoPixel strip(NUMstrip, PIN_NeoPixel, NEO_GRB + NEO_KHZ800);
Adafruit_BME280 bme; // I2C
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
#ifdef sensorMQ2
MQ2 mq2(PIN_MQ2, true); //instance (true=with serial output enabled)
#endif

RunningMedian samples = RunningMedian(5);

/*
  // define buttons
  OneButton btnRight = OneButton(Pin_Right, true, true);
  OneButton btnLeft = OneButton(Pin_Left, true, true);
  OneButton btnMiddle = OneButton(Pin_Middle, true, true);
*/

// variables will change:
volatile int buttonState = 0;         // variable for reading the pushbutton status


int showDisplay = 0;    //which sensor settings shall be shown

int flagMeasureOnly = 0;
byte flagLEDOff  = 0;

long lastDisplay ;

long lastTime = 0;
int timeBetweenReset = 0;

/*   =======================================================
              Setup
     =======================================================
*/
void setup() {
  pinMode(Pin_Middle, INPUT);
  digitalWrite(Pin_Middle, HIGH); //activate PullUp


  Serial.begin(myBaudRate);
  while (!Serial);   // time to get serial running
  myFileName = String(__FILE__);
  versionsInfo();   //print File information



  Wire.begin();

  checkI2C();   //check for connected I2C devices, debugging/service purpose only

  // initialize OLED
  u8x8.begin();
  //u8x8.setFont(u8x8_font_lucasarts_scumm_subtitle_o_2x2_n);//https://github.com/olikraus/u8g2/wiki/fntlist8x8
  u8x8.setFont(u8x8_font_8x13_1x2_f);
  u8x8.clear();
  u8x8.setCursor(0, 2);
  u8x8.print("Start....");

  //initialize NeoPixel (WS2812B)
  strip.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)

#ifdef sensorMQ2
  //initialize MQ-2 gas sensor
  mq2.begin();
#endif

  /*
    // Button Setup - code to run once key is pressed
    btnRight.attachClick(handleButtonRight);
    btnLeft.attachClick(handleButtonLeft);
    btnMiddle.attachClick(handleButtonMiddle);
  */

  // are the essential sensors connected - if not, show warning and stop here!
  if (!bme.begin() || !ccs.begin()) {
    u8x8.clear(); // clear display to allow error message
    u8x8.setCursor(0, 0);
    u8x8.print("Init failed:");
    if (!bme.begin()) {
      u8x8.setCursor(0, 2);
      u8x8.print("missing BME280");
    }
    if (!ccs.begin()) {
      u8x8.setCursor(0, 4);
      u8x8.print("missing CSS811");
    }

    while (1) {
      theaterChase(strip.Color(127, 127, 127), 50); // White, half brightness
    }
  }




  // display filename/prgram info

  u8x8.setCursor(0, 5);
  u8x8.print(myFileName);
  //versionsInfo();   //print File information


  //calibrate temperature sensor of CCS811
  while (!ccs.available());

  sensorResult.temp_NTC = ccs.calculateTemperature();
  //ccs.setTempOffset(sensorResult.temp_NTC - 25.0);
  ccs.setTempOffset(bme.readTemperature()); //calibrate with BME Temp-reading

  Serial.println("Temp Calib");
  Serial.println(ccs.calculateTemperature());
  Serial.println(bme.readTemperature());


  // Initial colourizing
  strip.clear();
  strip.setBrightness(5);
  rainbow(1);             // Flowing rainbow cycle along the whole strip

  u8x8.clear(); //clear screen once

  //Interrupt
  //attachInterrupt(Pin_Middle,pin_ISR,CHANGE);
}


/*   =======================================================
              Loop
     =======================================================
*/
void loop() {
  showDisplay = 4;

  requestSensorData();
  updateMeasures();

  if (digitalRead(Pin_Middle) == 0) {

    if (buttonState == 0) {
      buttonState = 1;
      strip.clear();
      strip.show();
      Serial.println("############## aus ######");

    }
    else {
      buttonState = 0;
      Serial.println("############## an ######");
    }
  }

  if (buttonState == 0) {
    if (flagLEDOff == 0) {
      switch ((int)sensorResult.eCO2) {
        case 0 ... 399:
          // not valid
          break;
        case 400 ... 700:
          strip.setBrightness(5);
          gaugeColour(userYellow, userLightGreen, map(sensorResult.eCO2, 400, 700, 0, NUMstrip - 1));
          break;
        case 701 ... 1000:
          strip.setBrightness(50);
          gaugeColour(userOrange, userRed, map(sensorResult.eCO2, 701, 1000, 0, NUMstrip - 1));
          break;
        case 1001 ... 2000:
          strip.setBrightness(100);
          gaugeColour(userRed, userPink, map(sensorResult.eCO2, 1001, 2000, 0, NUMstrip - 1));
          break;
        case 2001 ... 10000:
          theaterChase(strip.Color(127,   0,   0), 100); // Red, full brightness
          break;
        default:
          theaterChaseRainbow(50);
          break;
      }
    }
    else {
      strip.clear();
    }
  }

  /*
    if ((millis() - lastTime) > 1000) { // increase every second
      timeBetweenReset++;
      if (timeBetweenReset > 600) {
        timeBetweenReset = 0;
        lastTime = millis();
        ccs.SWReset();
        ccs.begin();
        Serial.println("++++++++++++++++    Sensor reset");
      }
    }
  */

  //
  // if ((millis()-lastTime)>600000){
  //  lastTime=millis();
  //  ccs.SWReset();
  //  ccs.begin();
  //  Serial.println("++++++++++++++++    Sensor reset");
  // }
  delay(1000);

  //    btnRight.tick();
  //    btnLeft.tick();
  //    btnMiddle.tick();

  //
  //
  //  // ------------
  //  strip.clear();
  //  while (1) {
  //    updateMeasures();
  //    delay(4000);
  //  }
  //
  //  printValues();
  //
  //  if (flagMeasureOnly != 0) {
  //    updateMeasures();
  //    btnRight.tick();
  //    btnLeft.tick();
  //    btnMiddle.tick();
  //    strip.clear();
  //  }
  //  else {
  //    u8x8.clear();
  //    u8x8.setCursor(0, 0);
  //    u8x8.print("Farbe");
  //
  //    updateMeasures();
  //
  //    strip.clear(); // Set all pixel colors to 'off'
  //
  //    // The first NeoPixel in a strand is #0, second is 1, all the way up
  //    // to the count of strip minus one.
  //    for (int i = 0; i < NUMstrip; i++) { // For each pixel...
  //
  //      // strip.Color() takes RGB values, from 0,0,0 up to 255,255,255
  //      // Here we're using a moderately bright green color:
  //      strip.setPixelColor(i, strip.Color(0, 150, 0));
  //
  //      strip.show();   // Send the updated pixel colors to the hardware.
  //      // keep watching the push button:
  //      btnRight.tick();
  //      btnLeft.tick();
  //      btnMiddle.tick();
  //      delay(DELAYVAL); // Pause before next pass through loop
  //    }
  //
  //    delay(2000);
  //
  //    u8x8.clear();
  //    u8x8.setCursor(0, 0);
  //    u8x8.print("Colour Wipe");
  //    updateMeasures();
  //
  //
  //    // Fill along the length of the strip in various colors...
  //    colorWipe(strip.Color(255,   0,   0), 50); // Red
  //    colorWipe(strip.Color(  0, 255,   0), 50); // Green
  //    colorWipe(strip.Color(  0,   0, 255), 50); // Blue
  //
  //    u8x8.clear();
  //    u8x8.setCursor(0, 0);
  //    u8x8.print("Theater");
  //    updateMeasures();
  //
  //    // Do a theater marquee effect in various colors...
  //    theaterChase(strip.Color(127, 127, 127), 50); // White, half brightness
  //    theaterChase(strip.Color(127,   0,   0), 50); // Red, half brightness
  //    theaterChase(strip.Color(  0,   0, 127), 50); // Blue, half brightness
  //
  //    u8x8.clear();
  //    u8x8.setCursor(0, 0);
  //    u8x8.print("Rainbow");
  //    updateMeasures();
  //
  //    rainbow(10);             // Flowing rainbow cycle along the whole strip
  //
  //    u8x8.clear();
  //    u8x8.setCursor(0, 0);
  //    u8x8.print("Theater Rainbow");
  //
  //    updateMeasures();
  //
  //
  //    theaterChaseRainbow(50); // Rainbow-enhanced theaterChase variant
  //
  //    delay(2000);
  //  }
}

/*   =======================================================
              Print sensor values on OLED
     =======================================================
*/
void updateMeasures()
{

  if ((millis() - lastDisplay) > timeBetweenDisplay) {
    showDisplay++;


    switch (showDisplay) {
      case 1:

        //BME280
        u8x8.setCursor(0, 0);
        u8x8.clearDisplay();
        u8x8.print("Umweltsensor");
        u8x8.setCursor(0, 2);
        u8x8.print("T: ");
        u8x8.print(sensorResult.temp);
        u8x8.print(" °C  ");

        u8x8.setCursor(0, 4);
        u8x8.print("P: ");
        u8x8.print(sensorResult.pressure);
        u8x8.println(" hPa  ");

        u8x8.setCursor(0, 6);
        u8x8.print("H: ");
        u8x8.print( sensorResult.humidity);
        u8x8.println(" %rH  ");


        Serial.print("Temperature = "); Serial.print(sensorResult.temp); Serial.println(" *C");
        Serial.print("Pressure = "); Serial.print(sensorResult.pressure); Serial.println(" hPa");
        Serial.print("Humidity = "); Serial.print( sensorResult.humidity); Serial.println(" %");
        break;
      case 2:

        //measureMQ2();
        u8x8.clearDisplay();
        u8x8.setCursor(0, 0);
        u8x8.print("Gassensor   ");
        u8x8.setCursor(0, 2);
        u8x8.print("LPG: ");
        u8x8.print(sensorResult.lpg);
        u8x8.print(" ppm  ");

        u8x8.setCursor(0, 4);
        u8x8.print("CO2: ");
        u8x8.print(sensorResult.co);
        u8x8.println(" ppm  ");

        u8x8.setCursor(0, 6);
        u8x8.print("sm: ");
        u8x8.print(sensorResult.smoke);
        u8x8.println(" ppm  ");
        break;
      case 3:

        u8x8.clearDisplay();
        u8x8.setCursor(0, 0);
        u8x8.print("Luftqualitaet");
        // sensorResult.temp_NTC = ccs.calculateTemperature();
        //if (!ccs.readData()) {
        u8x8.setCursor(0, 2);
        u8x8.print("eCO2: ");
        Serial.print("eCO2: ");
        // float eCO2 = ccs.geteCO2();
        u8x8.print(sensorResult.eCO2);
        Serial.print(sensorResult.eCO2);


        u8x8.print(" ppm\nTVOC: ");
        Serial.print(" ppm, TVOC: ");
        // float TVOC = ccs.getTVOC();
        u8x8.print(sensorResult.TVOC );
        Serial.print(sensorResult.TVOC );

        Serial.print(" ppb   Temp:");
        u8x8.setCursor(14, 4);
        u8x8.print(" ppb\nTemp: ");
        Serial.println(sensorResult.temp_NTC);
        u8x8.println(sensorResult.temp_NTC);

        u8x8.display();

        showDisplay = 0;
        break;

      case 5: //eCO2 only

        Serial.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        Serial.print("Median: ");
        Serial.print(samples.getMedian());
        Serial.print("   ermittelt: ");
        Serial.println(sensorResult.eCO2);

        
        // u8x8.clearDisplay();
        u8x8.setCursor(0, 0);
        u8x8.print("eCO2");
        u8x8.setFont(u8x8_font_profont29_2x3_r);
        u8x8.setCursor(5, 0);
        u8x8.print(samples.getMedian(), 0);
        //u8x8.print(sensorResult.eCO2, 0); //ohne Nachkommastellen
        u8x8.print("  ");
        u8x8.setFont(u8x8_font_8x13_1x2_f);
        u8x8.setCursor(13, 0);
        u8x8.print("ppm");
        
        u8x8.setCursor(0, 4);
        u8x8.print("TVOC: ");
        u8x8.print(sensorResult.TVOC );
        u8x8.print(" ");
        u8x8.setCursor(13, 4);
        u8x8.print("ppb\nTemp: ");
        //u8x8.println(sensorResult.temp_NTC);
        u8x8.print(sensorResult.temp,1);
        u8x8.print(" ");
        u8x8.setCursor(13, 6);
        u8x8.print("oC");
        
        break;


    }
    lastDisplay = millis();

  }
}




/*   =======================================================
              Button handler
     =======================================================
*/
void handleButtonRight() {
  Serial.println("rechts gedrueckt");
  //delay(2000);
  flagMeasureOnly = 2;
}
void handleButtonLeft() {
  Serial.println("links gedrueckt");
  // delay(2000);
  flagMeasureOnly = 0;
}
void handleButtonMiddle() {
  Serial.println("Mitte gedrueckt");
  //delay(2000);
  flagMeasureOnly = 1;

  // turn on/off LED so OLED only
  if (flagLEDOff == 0) {
    flagLEDOff = 1;
  } else {
    flagLEDOff = 0;
  }
  Serial.print("LED ");
  Serial.println(flagLEDOff);

}

/* -----------------------------------------------------------------------------
        get data from sensors
    -----------------------------------------------------------------------------
*/
void requestSensorData() {


  // CCS811
  if (!ccs.readData()) {
    sensorResult.temp_NTC = ccs.calculateTemperature();
    sensorResult.eCO2      = ccs.geteCO2();
    sensorResult.TVOC      =  ccs.getTVOC();
    Serial.print(F("eCO2 ermittelt: "));
    Serial.println(sensorResult.eCO2);
    samples.add(sensorResult.eCO2); // get data and add to median filter

    Serial.print(F("Temperatur: "));
    Serial.println(sensorResult.temp_NTC);

  }



#ifdef sensorMQ2
  // MQ-2
  /* Read all values from the sensor - it returns an array which contains 3 values
    1 = LPG in ppm
    2 = CO in ppm
    3 = SMOKE in ppm
  */
  float* values = mq2.read(true); //set it false if you don't want to print the values in the Serial

  //Reading specific values:
  //lpg = values[0];
  sensorResult.lpg = mq2.readLPG();
  //co = values[1];
  sensorResult.co = mq2.readCO();
  //smoke = values[2];
  sensorResult.smoke = mq2.readSmoke();

#endif

  //BME280
  sensorResult.temp = bme.readTemperature();
  sensorResult.humidity = bme.readHumidity();
  sensorResult.pressure = bme.readPressure() / 100.0F + 27; //local correction factor
  Serial.print(F("BME Temp "));
  Serial.println(sensorResult.temp);
}


void pin_ISR()
{
  Serial.print("ISR");
  Serial.println(digitalRead(Pin_Middle));
}
