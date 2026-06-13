#include <Arduino.h>
#include <7semi_SCD40.h>
#include <Wire.h>
#include <SPI.h> //Library for Serial Peripheral Interface (SPI) , required for SD card
#include <LiquidCrystal_I2C.h>
#include "DFRobot_OxygenSensor.h"
#include "DFRobot_MultiGasSensor.h"
#include <DHT.h>
#include <stdio.h>
#include "hardware/watchdog.h"
#include <WiFi.h> //library for wifi communication
#include <ThingSpeak.h> //library for thingspeak IOT cloud
#include <SD.h>
#include "time.h"

#define Oxygen_IICAddress ADDRESS_3
#define COLLECT_NUMBER 10
// collect number, the collection range is 1-100.
DFRobot_OxygenSensor oxygen;

#define I2C_COMMUNICATION
#define I2C_ADDRESS 0x74
DFRobot_GAS_I2C gas(&Wire, I2C_ADDRESS);

#define DHTPIN 22 // Conncet GPIO 22, Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22 (AM2302), AM2321

//Assinging SD card pin
#define PIN_SPI0_MISO (16u)
#define PIN_SPI0_MOSI (19u)
#define PIN_SPI0_SCK (18u)
#define PIN_SPI0_SS (17u)

//Internet info
char ssid[] = "SimpleWIFI"; // network SSID (name) simply: WIFI name
char pass[] = "WIFISimple1234"; // network password
int keyIndex = 0; // your network key Index number (needed only for WEP)
WiFiClient client;

unsigned long myChannelNumber = 2789340; //ThingsSpeak Channel Number
const char * myWriteAPIKey = "Put Your API Key Here"; //ThingsSpeak API Key

const int daylightOffset_sec = 0;
const long gmtOffset_sec = 19800; // 5.5 hours

String myStatus = "";

DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27,20,4); // set the LCD address to 0x27 for a 16 chars and 2 line display

SCD40 sensor;

static int16_t error;

//Initialize array for data storing
float hum[30]; //Store humidity value
float temp[30]; //Store temperature value
int co2[30];
char t[32];
float o2[30]; //Store o2 value
float co[30]; //Store o2 value

// MKRZero SD: SDCARD_SS_PIN
const int chipSelect = 17u;

//initializing misilinious variable
float p = 3.1415926;
int warmingup = 5000; //5 second warmin up
int waittime = 3000; //3 second rest time of all sensors
int number4 = 0; //for counting
int j=0;

File myFile;

void setup() {
Wire.begin(); // GP20/21
Wire.setTimeout(3000);
Wire.setClock(50000);

Serial.begin(9600);
delay(1000);

if (sensor.begin()) {
Serial.println("SCD40 initialized");
}
else {
Serial.println("SCD40 initialization failed");
}
//watchdog_enable(12000, 1); // 120 second watchdog
//sensor.begin(Wire, SCD30_I2C_ADDR_61);
//analogReadResolution(12);
pinMode(LED_BUILTIN, OUTPUT);
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, pass);
Serial.print("\t");
lcd.init(); // initialize the lcd
lcd.init();// Print a message to the LCD.
lcd.backlight();
Serial.print("Testing DHT sensor");
Serial.println();
dht.begin();
oxygen.begin(Oxygen_IICAddress);
gas.begin();
gas.setTempCompensation(gas.ON);
gas.changeAcquireMode(gas.INITIATIVE);
ThingSpeak.begin(client); // Initialize ThingSpeak
delay(warmingup);
configTime(gmtOffset_sec, daylightOffset_sec, "time.nist.gov", "pool.ntp.org");
delay(2000);
Serial.println("\nWaiting for time");
unsigned timeout = 5000;
unsigned start = millis();
while (!time(nullptr))
{
Serial.print(".");
delay(1000);
}
delay(1000);
Serial.println("Time...");

                             //Warming up sensors

Serial.print("Initializing SD card...");
if (!SD.begin(chipSelect)) {
Serial.println("initialization failed!");
return;
}
time_t current = time(nullptr);
Serial.print(ctime(&current));
Serial.println("initialization done.");
myFile = SD.open("test.txt", FILE_WRITE);
myFile.print("\n");
myFile.print("Data taking stated");
myFile.print(ctime(&current));
myFile.print("\n");
myFile.print("Temperature");
myFile.print("\t\t");
myFile.print("Humidity");
myFile.print("\t\t");
myFile.print("CO2");
myFile.print("\t\t");
myFile.print("O2");
myFile.print("\t\t");
myFile.print("CO");
myFile.close();
}

void loop() {
//Initilize variable for "for loop"
int i = 0;
int j = 0;
int d = 0;
int count = 0;
//Initilize variable for data storing
float stemp = 0;
float shume = 0;
float sco =0;
float so2 = 0;
float sco2 = 0;
float atemp = 0;
float ahume = 0;
float aco = 0;
float ao2 = 0;
float aco2 = 0;

// Connect or reconnect to WiFi
//WiFi.begin(ssid, pass);
// Print periods on monitor while establishing connection
while (WiFi.status() != WL_CONNECTED) {
//watchdog_update();
delay(500); // wait for half a second
Serial.print("."); //print dot in screen
digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
delay(500); // wait for half a second
digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
if (count > 30)
{
goto skip;
}
count = count + 1;
}
//Serial.begin(9600); //Initialize serial
// Connection established
Serial.println("\nConnected.");
Serial.println("");
Serial.print("Pico W is connected to WiFi network ");
Serial.println(WiFi.SSID()); //print wifi ssid details

// Print IP Address
Serial.print("Assigned IP Address: ");
Serial.println(WiFi.localIP());
//skip section
skip:
Serial.println("data collection stared.........");
Serial.println("");

// loop for temperature and humidity data collection
for (i=0; i<=10; i++) {
delay(500);
// Read temperature as Celsius (the default)
temp[i] = dht.readTemperature();
hum[i] = dht.readHumidity();
o2[i] = oxygen.getOxygenData(COLLECT_NUMBER);
co[i] = AllDataAnalysis.gasconcentration;
stemp = stemp + temp[i];
shume = shume + hum[i];
so2 = so2 + o2[i];
sco = sco + co[i];
}
atemp = stemp/10; //store average temeprature data
ahume = shume/10; //store average humidity data
ao2 = so2/10;
aco = sco/10;
Serial.println("Tempressure, Humidity and O2 Data Collected");
//lcd.clear();
uint16_t co2Concentration = 0;
// loop for CO2 concentration data collection

for (d = 0; d < 3; d++) {

uint16_t co2ppm;
float scdTemp;
float scdHum;

if (sensor.readSingleShot(co2ppm, scdTemp, scdHum)) {

    co2[d] = co2ppm;
    sco2 += co2ppm;

    Serial.print("CO2: ");
    Serial.print(co2ppm);
    Serial.println(" ppm");
}
else {

    Serial.println("SCD40 read failed");
    recoverI2C();
}

delay(3000);

}

aco2 = sco2/3; //store average temeprature data
Serial.println("CO2 data taken");
//Print temp and humidity values to serial monitor
Serial.print("Humidity: ");
Serial.print(ahume);
Serial.print(" %, Temp: ");
Serial.print(atemp);
Serial.print(" Celsius ");
Serial.print("\t");
Serial.println(" CO gasconcentration:");
Serial.print(aco);
Serial.print(" PPM");
Serial.print("\t");
Serial.print("co2Concentration: ");
Serial.print(aco2);
Serial.println(" PPM");
Serial.print(" oxygen concentration is ");
Serial.print(ao2);
Serial.println(" %vol");
Serial.println();
// Writing to SD card
//SD.begin(chipSelect);

myFile = SD.open("test.txt", FILE_WRITE);
if (myFile) {
Serial.print("Writing to test.txt...");
myFile.print(atemp);
myFile.print("\t\t");
myFile.print(ahume);
myFile.print("\t\t");
myFile.print(aco);
myFile.print("\t\t");
myFile.print(aco2);
myFile.print("\t\t");
myFile.print(ao2);
// close the file:
myFile.close();
Serial.println("done.");
} else {
// if the file didn't open, print an error:
Serial.println("error opening test.txt");
}
//LCD
lcd.setCursor(0,0);
lcd.print("O2:");
lcd.print(ao2);
lcd.print("%");
lcd.setCursor(10,0);
lcd.print("T:");
lcd.print(int(atemp));
lcd.print((char)223);
lcd.print("C");
lcd.setCursor(0,1);
lcd.print("CO2:");
lcd.print(int(aco2));
lcd.print("PPM");
lcd.setCursor(11,1);
lcd.print("CO:");
lcd.print(int(aco));
watchdog_update();
//lcd.print("PPM");
//lcd.setCursor(5,1);
// set the fields with the values
retry: //retry when data is not uploaded to ThingsSpeak
ThingSpeak.setField(1, atemp);
ThingSpeak.setField(2, ahume);
ThingSpeak.setField(3, ao2);
ThingSpeak.setField(4, aco2);
ThingSpeak.setField(5, aco);

// figure out the status message
myStatus = String("Temprature and humidity data uploded");
// set the status
ThingSpeak.setStatus(myStatus);

// write to the ThingSpeak channel
int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
if(x == 200){
Serial.println("Channel update successful.");
digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
delay(500); // wait for a second
digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
delay(100);
}
else{
Serial.println("Problem updating channel. HTTP error code " + String(x));
j = j + 1;
if (j > 10)
{
goto stop;
}
goto retry;
}
stop:
// for counting pourpous
//digitalWrite(relay_pin,HIGH); //poweroff of sensors
delay(waittime);
}

void recoverI2C() {

Serial.println("Resetting I2C bus...");

Wire.end();
delay(100);

Wire.begin();
Wire.setTimeout(3000);
Wire.setClock(50000);

if (!sensor.begin()) {
    Serial.println("SCD40 restart failed");
}

lcd.init();
lcd.backlight();

oxygen.begin(Oxygen_IICAddress);

gas.begin();

}
