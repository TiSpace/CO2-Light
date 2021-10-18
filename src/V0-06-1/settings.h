#define PIN_NeoPixel        2 // On Trinket or Gemma, suggest changing this to 1
#define PIN_MQ2             A2
#define Pin_Right   10
#define Pin_Left    12
#define Pin_Middle  11

#define NUMstrip 17 // Popular NeoPixel ring size

#define DELAYVAL 500 // Time (in milliseconds) to pause between strip


#define myBaudRate 38400

#define SEALEVELPRESSURE_HPA (1013.25)

#define timeBetweenDisplay 500     // time between display shows next measurements

// sensors
//#define sensorMQ2
#define sensorBME280
#define sensorCCS811

String myFileName;

struct measurement
{
  float lpg;
  float co;
  float smoke;
  float temp;
  float humidity;
  float pressure;
  float temp_NTC;
  float eCO2;
  float TVOC;
} sensorResult;

// Colours https://www.w3schools.com/colors/colors_picker.asp
#define userRed     0xFF0000
#define userOrange  0xFF8000
#define userYellow  0xFFFF00
#define userGreen   0x00FF00
#define userLightGreen 0x00FF80
#define userPink    0xFF00FF

