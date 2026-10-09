#include <Arduino.h>
#include <7semi_SCD40.h>
#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include "DFRobot_OxygenSensor.h"
#include "DFRobot_MultiGasSensor.h"
#include <DHT.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "hardware/watchdog.h"
#include <WiFi.h>
#include <WiFiNTP.h>
#include <ThingSpeak.h>
#include <SD.h>
#include "time.h"

// ============================================================
// ECOLOGICAL OBSERVATION STATION
// Raspberry Pi Pico W
//
// TIME ARCHITECTURE
// 1. NTP synchronizes the Pico's internal system clock.
// 2. Every SD record uses time(nullptr) from that clock.
// 3. If WiFi disappears, the internal clock keeps being used.
// 4. When WiFi returns, NTP is started again to correct the clock.
// 5. If the Pico boots with no Internet and has no valid clock,
//    an operator-visible Offline Run ID is generated.
// ============================================================


// ============================================================
// SENSOR DEFINITIONS
// ============================================================

#define Oxygen_IICAddress ADDRESS_3
#define COLLECT_NUMBER 10

DFRobot_OxygenSensor oxygen;

// DFRobot Multi Gas Sensor
#define I2C_ADDRESS 0x74
DFRobot_GAS_I2C gas(&Wire, I2C_ADDRESS);

// DHT22
#define DHTPIN 22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// SCD40 CO2 sensor
SCD40 sensor;

// 20x4 I2C LCD at address 0x27
LiquidCrystal_I2C lcd(0x27, 20, 4);


// ============================================================
// BATTERY VOLTAGE MONITORING
// ============================================================

// Raspberry Pi Pico ADC1 = GPIO27
#define BATTERY_ADC_PIN 27

// Battery+ ---- R1 ---- GP27 ---- R2 ---- GND
const float BATTERY_R1 = 100000.0;
const float BATTERY_R2 = 100000.0;

// RP2040 12-bit ADC
const float ADC_REFERENCE_VOLTAGE = 3.3;
const float ADC_MAX_VALUE = 4095.0;

// Compare with a multimeter and adjust if necessary.
const float BATTERY_CALIBRATION = 1.0224;

// ============================================================
// SD CARD
// ============================================================

#define PIN_SPI0_MISO (16u)
#define PIN_SPI0_MOSI (19u)
#define PIN_SPI0_SCK  (18u)
#define PIN_SPI0_SS   (17u)

const int chipSelect = 17u;

bool sdAvailable = false;

const unsigned long SD_RETRY_INTERVAL = 60000UL; // 60 s
unsigned long lastSDAttempt = 0;


// ============================================================
// WIFI / THINGSPEAK
// ============================================================

// Put your real credentials here locally.
// DO NOT publish them to GitHub.

//Internet info
char ssid[] = "SimpleWIFI"; // network SSID (name) simply: WIFI name
char pass[] = "WIFISimple1234"; // network password
int keyIndex = 0; // your network key Index number (needed only for WEP)
WiFiClient client;

unsigned long myChannelNumber = 27290340; //ThingsSpeak Channel Number
const char * myWriteAPIKey = "Put Your API Key Here"; //ThingsSpeak API Key

String myStatus = "";

// ============================================================
// TIME / NTP
// ============================================================

// India = UTC + 5:30
const long gmtOffset_sec = 19800;

// Not used for automatic DST in India.
const int daylightOffset_sec = 0;

// A sane Unix time after 2023-11-14.
// Used only to decide whether the internal clock is valid.
const time_t VALID_EPOCH = 1700000000;

// Startup NTP wait.
const unsigned long NTP_STARTUP_TIMEOUT = 15000UL;

// Recovery NTP monitoring timeout.
// NTP continues in the background; this is only our wait window.
const unsigned long NTP_RECOVERY_TIMEOUT = 10000UL;

// Retry when WiFi is connected but the clock is still unsynchronized.
const unsigned long NTP_RETRY_INTERVAL = 60000UL;

// Periodically correct clock drift while Internet remains available.
const unsigned long NTP_PERIODIC_SYNC_INTERVAL =
    6UL * 60UL * 60UL * 1000UL;   // 6 hours

bool clockValid = false;
bool ntpResyncInProgress = false;
bool ntpRetryPending = false;

// Set by lwIP only when an actual SNTP response updates the clock.

unsigned long lastNtpAttempt = 0;
unsigned long lastNtpSyncMillis = 0;

enum ClockSource
{
    CLOCK_UNSYNCED,
    CLOCK_NTP,
    CLOCK_INTERNAL
};

ClockSource clockSource = CLOCK_UNSYNCED;


// ============================================================
// WIFI RECOVERY
// ============================================================

const unsigned long WIFI_RECONNECT_INTERVAL = 30000UL; // 30 s
const unsigned long WIFI_CONNECT_TIMEOUT = 8000UL;     // 8 s

unsigned long lastWiFiAttempt = 0;
unsigned long wifiAttemptStart = 0;

bool wifiReconnectInProgress = false;
bool wifiWasConnected = false;


// ============================================================
// SYSTEM / DATA VARIABLES
// ============================================================

int warmingup = 5000;
int waittime = 3000;

int thingSpeakFailCount = 0;

// Sensor arrays are retained to keep the structure close to
// your original program.
float hum[30];
float temp[30];
int co2[30];
float o2[30];
float co[30];

File myFile;


// ============================================================
// RUN NUMBER
// ============================================================

unsigned long bootCounter = 1;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

float readBatteryVoltage();

bool connectWiFi();
void maintainWiFi();
void startWiFiReconnect();

bool synchronizeTimeAtStartup();
void startNTPResync(bool startupMode);
void maintainNTP();

bool isClockValid();

void formatISTDateTime(
    time_t utcTime,
    char *dateBuffer,
    size_t dateBufferSize,
    char *timeBuffer,
    size_t timeBufferSize
);

void getMeasurementDateTime(
    char *dateBuffer,
    size_t dateBufferSize,
    char *timeBuffer,
    size_t timeBufferSize,
    const char **sourceText
);

unsigned long readRunLogCounter();
unsigned long readLegacyBootCounter();
unsigned long getBootCounter();
void saveBootCounter(unsigned long counter);

void generateOfflineTimestamp(
    unsigned long bootNumber,
    char *buffer,
    size_t bufferSize
);

void displayBatteryVoltage(float voltage);
void displayRealTime();
void displayTimeFailure();

void displayOfflineTime(
    const char *timestamp,
    unsigned long bootNumber
);

void recoverI2C();

void maintainSD();
void logSessionHeader(
    float startupBatteryVoltage,
    bool timeWasInitiallySynced,
    const char *offlineTimestamp
);

void logTimeSyncEvent(const char *reason);

void serviceConnectivity();
void serviceNTP();


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(9600);
    delay(1000);

    // --------------------------------------------------------
    // I2C
    // --------------------------------------------------------

    Wire.begin();              // GP20 = SDA, GP21 = SCL
    Wire.setTimeout(3000);
    Wire.setClock(50000);

    // --------------------------------------------------------
    // LCD
    // --------------------------------------------------------

    lcd.init();
    lcd.backlight();
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Ecological");
    lcd.setCursor(0, 1);
    lcd.print("Observation Station");
    delay(2000);

    // --------------------------------------------------------
    // Battery ADC
    // --------------------------------------------------------

    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);

    float batteryVoltage = readBatteryVoltage();

    Serial.println();
    Serial.println("================================");
    Serial.println("BATTERY MONITOR");
    Serial.println("================================");
    Serial.print("Battery Voltage: ");
    Serial.print(batteryVoltage, 2);
    Serial.println(" V");

    displayBatteryVoltage(batteryVoltage);

    // Show battery voltage for 20 seconds
    delay(10000);

    // --------------------------------------------------------
    // SYSTEM READY MESSAGE
    // --------------------------------------------------------

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("SYSTEM READY");

    lcd.setCursor(0, 1);
    lcd.print("Starting...");

    // Show system ready message for 5 seconds
    delay(5000);

    // --------------------------------------------------------
    // SCD40
    // --------------------------------------------------------

    if (sensor.begin())
    {
        Serial.println("SCD40 initialized");
    }
    else
    {
        Serial.println("SCD40 initialization failed");
    }

    // --------------------------------------------------------
    // GPIO
    // --------------------------------------------------------

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // --------------------------------------------------------
    // WiFi
    // --------------------------------------------------------

    WiFi.mode(WIFI_STA);

    // Register once so we can distinguish a fresh NTP sync
    // from an already-valid internal clock.
    WiFi.beginNoBlock(ssid, pass);

    // --------------------------------------------------------
    // Other sensors
    // --------------------------------------------------------

    Serial.println("Testing DHT sensor");
    dht.begin();

    oxygen.begin(Oxygen_IICAddress);

    gas.begin();
    gas.setTempCompensation(gas.ON);
    gas.changeAcquireMode(gas.INITIATIVE);

    // --------------------------------------------------------
    // ThingSpeak
    // --------------------------------------------------------

    ThingSpeak.begin(client);

    // --------------------------------------------------------
    // Sensor warm-up
    // --------------------------------------------------------

    delay(warmingup);

    // --------------------------------------------------------
    // SD CARD
    // --------------------------------------------------------

    Serial.println("Initializing SD card...");

    sdAvailable = SD.begin(chipSelect);
    lastSDAttempt = millis();

    if (!sdAvailable)
    {
        Serial.println("SD initialization failed!");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SD CARD ERROR");
        lcd.setCursor(0, 1);
        lcd.print("Data logging");
        lcd.setCursor(0, 2);
        lcd.print("unavailable");

        delay(2500);
    }
    else
    {
        Serial.println("SD initialization done.");
    }

    // --------------------------------------------------------
    // UNIQUE BOOT / RUN NUMBER
    // --------------------------------------------------------

    bootCounter = getBootCounter();

    Serial.print("Boot / Run Number: ");
    Serial.println(bootCounter);

    // --------------------------------------------------------
    // STARTUP WIFI
    // --------------------------------------------------------

    bool wifiConnected = connectWiFi();

    wifiWasConnected = wifiConnected;
    wifiReconnectInProgress = false;

    if (wifiConnected)
    {
        lastWiFiAttempt = millis();
    }
    else
    {
        // Allow recovery to be attempted immediately in loop().
        lastWiFiAttempt =
            millis() - WIFI_RECONNECT_INTERVAL;
    }

    // --------------------------------------------------------
    // STARTUP NTP
    // --------------------------------------------------------

    bool timeSynced = false;

    if (wifiConnected)
    {
        timeSynced = synchronizeTimeAtStartup();
    }
    else
    {
        Serial.println("WiFi unavailable.");
        Serial.println("Skipping startup NTP synchronization.");
    }

    // --------------------------------------------------------
    // DISPLAY TIME RESULT
    // --------------------------------------------------------

    char offlineTimestamp[32] = "NOT_AVAILABLE";

    if (timeSynced)
    {
        Serial.println("Internet time obtained.");

        displayRealTime();
        delay(4000);
    }
    else
    {
        displayTimeFailure();
        delay(3000);

        generateOfflineTimestamp(
            bootCounter,
            offlineTimestamp,
            sizeof(offlineTimestamp)
        );

        Serial.println();
        Serial.println("================================");
        Serial.println("OFFLINE RUN IDENTIFIER");
        Serial.println("================================");

        Serial.print("Reference Timestamp: ");
        Serial.println(offlineTimestamp);

        Serial.print("Run Number: ");
        Serial.println(bootCounter);

        displayOfflineTime(
            offlineTimestamp,
            bootCounter
        );

        delay(5000);
    }

    // --------------------------------------------------------
    // SESSION HEADER
    // --------------------------------------------------------

    logSessionHeader(
        batteryVoltage,
        timeSynced,
        offlineTimestamp
    );

    Serial.println();
    Serial.println("================================");
    Serial.println("SYSTEM READY");
    Serial.println("================================");
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    int d = 0;

    float stemp = 0.0;
    float shume = 0.0;
    float sco = 0.0;
    float so2 = 0.0;
    float sco2 = 0.0;

    float atemp = NAN;
    float ahume = NAN;
    float aco = NAN;
    float ao2 = NAN;
    float aco2 = NAN;

    int validDHT = 0;
    int validO2 = 0;
    int validCO = 0;
    int validCO2 = 0;

    // --------------------------------------------------------
    // Background services
    // --------------------------------------------------------

    serviceConnectivity();
    serviceNTP();

    Serial.println();
    Serial.println("Data collection started...");
    Serial.println();

    // --------------------------------------------------------
    // Temperature / Humidity / O2 / CO
    // --------------------------------------------------------
    // 10 measurements with 2 s spacing.
    // DHT22 needs adequate spacing between acquisitions.

    for (int i = 0; i < 10; i++)
    {
        delay(2000);

        float dhtTemp = dht.readTemperature();
        float dhtHum = dht.readHumidity();

        if (isfinite(dhtTemp) && isfinite(dhtHum))
        {
            stemp += dhtTemp;
            shume += dhtHum;
            validDHT++;
        }
        else
        {
            Serial.println("DHT22 read failed");
        }

        float o2Value =
            oxygen.getOxygenData(COLLECT_NUMBER);

        if (isfinite(o2Value))
        {
            so2 += o2Value;
            validO2++;
        }
        else
        {
            Serial.println("Oxygen sensor read failed");
        }

        float coValue =
            AllDataAnalysis.gasconcentration;

        if (isfinite(coValue))
        {
            sco += coValue;
            validCO++;
        }
        else
        {
            Serial.println("CO sensor read invalid");
        }

        // Keep WiFi/SD recovery responsive during sampling.
        // NTP is serviced outside the sensor timing loop.
        serviceConnectivity();
    }

    if (validDHT > 0)
    {
        atemp = stemp / validDHT;
        ahume = shume / validDHT;
    }

    if (validO2 > 0)
    {
        ao2 = so2 / validO2;
    }

    if (validCO > 0)
    {
        aco = sco / validCO;
    }

    Serial.println(
        "Temperature, Humidity, O2 and CO data collected."
    );

    // --------------------------------------------------------
    // CO2 - SCD40
    // --------------------------------------------------------

    for (d = 0; d < 3; d++)
    {
        uint16_t co2ppm = 0;

        float scdTemp = NAN;
        float scdHum = NAN;

        if (sensor.readSingleShot(
                co2ppm,
                scdTemp,
                scdHum))
        {
            co2[d] = co2ppm;

            sco2 += co2ppm;
            validCO2++;

            Serial.print("CO2: ");
            Serial.print(co2ppm);
            Serial.println(" ppm");
        }
        else
        {
            Serial.println("SCD40 read failed");
            recoverI2C();
        }

        delay(3000);

        // Keep WiFi/SD recovery responsive.
        serviceConnectivity();
    }

    if (validCO2 > 0)
    {
        aco2 = sco2 / validCO2;
    }
    else
    {
        Serial.println(
            "No valid SCD40 readings in this cycle."
        );
    }

    Serial.println("CO2 data taken");

    // --------------------------------------------------------
    // Print measurements
    // --------------------------------------------------------

    Serial.print("Humidity: ");
    if (isfinite(ahume))
        Serial.print(ahume, 2);
    else
        Serial.print("N/A");

    Serial.print(" %, Temp: ");
    if (isfinite(atemp))
        Serial.print(atemp, 2);
    else
        Serial.print("N/A");

    Serial.print(" Celsius");

    Serial.print("\tCO: ");
    if (isfinite(aco))
        Serial.print(aco, 2);
    else
        Serial.print("N/A");

    Serial.print(" PPM");

    Serial.print("\tCO2: ");
    if (isfinite(aco2))
        Serial.print(aco2, 2);
    else
        Serial.print("N/A");

    Serial.print(" PPM");

    Serial.print("\tO2: ");
    if (isfinite(ao2))
        Serial.print(ao2, 2);
    else
        Serial.print("N/A");

    Serial.println(" %vol");

    // --------------------------------------------------------
    // Battery voltage
    // --------------------------------------------------------

    float currentBatteryVoltage =
        readBatteryVoltage();

    Serial.print("Battery Voltage: ");
    Serial.print(currentBatteryVoltage, 2);
    Serial.println(" V");

    // --------------------------------------------------------
    // Timestamp for this measurement row
    // --------------------------------------------------------
    // This is taken after the complete sensor acquisition cycle.
    // If the clock has been synchronized at least once, this is
    // the Pico's internal running clock.
    // If not, the row is marked UNSYNCED.

    char dateBuffer[16];
    char timeBuffer[16];
    const char *timeSource = "UNSYNCED";

    getMeasurementDateTime(
        dateBuffer,
        sizeof(dateBuffer),
        timeBuffer,
        sizeof(timeBuffer),
        &timeSource
    );

    unsigned long elapsedSeconds = millis() / 1000UL;

    // --------------------------------------------------------
    // SD CARD
    // --------------------------------------------------------

    if (sdAvailable)
    {
        myFile = SD.open(
            "test.txt",
            FILE_WRITE
        );

        if (myFile)
        {
            Serial.print("Writing to test.txt...");

            // Run number
            myFile.print(bootCounter);
            myFile.print("\t");

            // Date
            myFile.print(dateBuffer);
            myFile.print("\t");

            // Time
            myFile.print(timeBuffer);
            myFile.print("\t");

            // Clock source
            myFile.print(timeSource);
            myFile.print("\t");

            // Seconds since this Pico boot
            myFile.print(elapsedSeconds);
            myFile.print("\t");

            // Temperature
            if (isfinite(atemp))
                myFile.print(atemp, 2);
            else
                myFile.print("N/A");
            myFile.print("\t");

            // Humidity
            if (isfinite(ahume))
                myFile.print(ahume, 2);
            else
                myFile.print("N/A");
            myFile.print("\t");

            // CO
            if (isfinite(aco))
                myFile.print(aco, 2);
            else
                myFile.print("N/A");
            myFile.print("\t");

            // CO2
            if (isfinite(aco2))
                myFile.print(aco2, 2);
            else
                myFile.print("N/A");
            myFile.print("\t");

            // O2
            if (isfinite(ao2))
                myFile.print(ao2, 2);
            else
                myFile.print("N/A");
            myFile.print("\t");

            // Battery
            myFile.print(currentBatteryVoltage, 2);
            myFile.println();

            myFile.close();

            Serial.println("done.");
        }
        else
        {
            Serial.println("Error opening test.txt");
            sdAvailable = false;
            lastSDAttempt = millis();
        }
    }
    else
    {
        Serial.println(
            "SD unavailable - measurement not logged."
        );
    }

    // --------------------------------------------------------
    // LCD
    // --------------------------------------------------------

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("O2:");
    if (isfinite(ao2))
        lcd.print(ao2, 1);
    else
        lcd.print("N/A");
    lcd.print("%");

    lcd.setCursor(10, 0);
    lcd.print("T:");
    if (isfinite(atemp))
        lcd.print((int)atemp);
    else
        lcd.print("N/A");
    lcd.print((char)223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("CO2:");
    if (isfinite(aco2))
        lcd.print((int)aco2);
    else
        lcd.print("N/A");
    lcd.print("PPM");

    lcd.setCursor(11, 1);
    lcd.print("CO:");
    if (isfinite(aco))
        lcd.print((int)aco);
    else
        lcd.print("N/A");

    lcd.setCursor(0, 2);
    lcd.print("BAT:");
    lcd.print(currentBatteryVoltage, 2);
    lcd.print("V");

    lcd.setCursor(0, 3);
    lcd.print("RH:");
    if (isfinite(ahume))
        lcd.print(ahume, 1);
    else
        lcd.print("N/A");
    lcd.print("%");

    // --------------------------------------------------------
    // Background services before cloud upload
    // --------------------------------------------------------

    watchdog_update();

    serviceConnectivity();
    serviceNTP();

    // --------------------------------------------------------
    // ThingSpeak
    // --------------------------------------------------------
    // SD is the primary local record.
    // Upload happens only if WiFi is connected AND all five
    // environmental values are valid.
    //
    // Field mapping preserved from your original channel:
    // Field 1 = Temperature
    // Field 2 = Humidity
    // Field 3 = O2
    // Field 4 = CO2
    // Field 5 = CO
    //
    // Battery is logged to SD but NOT sent to ThingSpeak here
    // so your existing 5-field channel configuration is preserved.

    if (WiFi.status() == WL_CONNECTED)
    {
        if (isfinite(atemp) &&
            isfinite(ahume) &&
            isfinite(ao2) &&
            isfinite(aco2) &&
            isfinite(aco))
        {
            ThingSpeak.setField(1, atemp);
            ThingSpeak.setField(2, ahume);
            ThingSpeak.setField(3, ao2);
            ThingSpeak.setField(4, aco2);
            ThingSpeak.setField(5, aco);

            myStatus =
                String("Environmental data uploaded");

            ThingSpeak.setStatus(myStatus);

            int x = ThingSpeak.writeFields(
                myChannelNumber,
                myWriteAPIKey
            );

            if (x == 200)
            {
                Serial.println(
                    "Channel update successful."
                );

                thingSpeakFailCount = 0;

                digitalWrite(
                    LED_BUILTIN,
                    HIGH
                );

                delay(200);

                digitalWrite(
                    LED_BUILTIN,
                    LOW
                );
            }
            else
            {
                Serial.print(
                    "ThingSpeak upload failed. "
                    "HTTP error code: "
                );

                Serial.println(x);

                thingSpeakFailCount++;

                if (WiFi.status() != WL_CONNECTED)
                {
                    wifiWasConnected = false;
                }
            }
        }
        else
        {
            Serial.println(
                "ThingSpeak skipped: invalid sensor value."
            );
        }
    }
    else
    {
        Serial.println(
            "WiFi unavailable - SD data saved locally."
        );
    }

    // --------------------------------------------------------
    // Next cycle
    // --------------------------------------------------------

    delay(waittime);
}


// ============================================================
// BATTERY VOLTAGE
// ============================================================

float readBatteryVoltage()
{
    const int samples = 20;
    uint32_t total = 0;

    for (int i = 0; i < samples; i++)
    {
        total += analogRead(BATTERY_ADC_PIN);
        delay(5);
    }

    float adcAverage =
        (float)total / samples;

    float adcVoltage =
        (adcAverage / ADC_MAX_VALUE) *
        ADC_REFERENCE_VOLTAGE;

    float batteryVoltage =
        adcVoltage *
        ((BATTERY_R1 + BATTERY_R2) / BATTERY_R2);

    batteryVoltage *= BATTERY_CALIBRATION;

    return batteryVoltage;
}


// ============================================================
// DISPLAY BATTERY
// ============================================================

void displayBatteryVoltage(float voltage)
{
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Bat Vol: ");
    lcd.print(voltage, 2);
    lcd.print(" V");

    lcd.setCursor(0, 1);
    lcd.print("Battery check...");
}


// ============================================================
// STARTUP WIFI CONNECTION
// ============================================================

bool connectWiFi()
{
    Serial.println();
    Serial.println("Connecting to WiFi...");

    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startTime >
            20000UL)
        {
            Serial.println();
            Serial.println(
                "WiFi connection timeout."
            );

            digitalWrite(
                LED_BUILTIN,
                LOW
            );

            return false;
        }

        delay(500);

        Serial.print(".");

        digitalWrite(
            LED_BUILTIN,
            !digitalRead(LED_BUILTIN)
        );
    }

    digitalWrite(
        LED_BUILTIN,
        LOW
    );

    Serial.println();
    Serial.println("WiFi connected.");

    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    return true;
}


// ============================================================
// START WIFI RECOVERY
// ============================================================

void startWiFiReconnect()
{
    Serial.println();
    Serial.println(
        "Starting WiFi recovery..."
    );

    WiFi.disconnect();
    delay(100);

    WiFi.beginNoBlock(
        ssid,
        pass
    );

    wifiReconnectInProgress = true;
    wifiAttemptStart = millis();
    lastWiFiAttempt = millis();

    digitalWrite(
        LED_BUILTIN,
        HIGH
    );
}


// ============================================================
// MAINTAIN WIFI
// ============================================================

void maintainWiFi()
{
    bool connected =
        (WiFi.status() == WL_CONNECTED);

    // --------------------------------------------------------
    // Connected
    // --------------------------------------------------------

    if (connected)
    {
        if (!wifiWasConnected)
        {
            Serial.println();
            Serial.println(
                "WiFi connection recovered."
            );

            Serial.print("SSID: ");
            Serial.println(WiFi.SSID());

            Serial.print("IP: ");
            Serial.println(WiFi.localIP());

            // We had a WiFi outage.
            // Start NTP again to correct the internal clock.
            startNTPResync(false);
        }

        wifiWasConnected = true;
        wifiReconnectInProgress = false;

        digitalWrite(
            LED_BUILTIN,
            LOW
        );

        return;
    }

    // --------------------------------------------------------
    // Detect connection loss
    // --------------------------------------------------------

    if (wifiWasConnected)
    {
        Serial.println();
        Serial.println(
            "WARNING: WiFi connection lost."
        );
        Serial.println(
            "Continuing data collection "
            "using internal clock + SD card."
        );

        wifiWasConnected = false;
        wifiReconnectInProgress = false;

        // Once WiFi is gone, explicitly describe the current
        // timestamp source as the Pico's internal running clock.
        if (clockValid)
        {
            clockSource = CLOCK_INTERNAL;
        }

        digitalWrite(
            LED_BUILTIN,
            LOW
        );

        lastWiFiAttempt = millis();
    }

    // --------------------------------------------------------
    // Recovery attempt in progress
    // --------------------------------------------------------

    if (wifiReconnectInProgress)
    {
        if (millis() - wifiAttemptStart >=
            WIFI_CONNECT_TIMEOUT)
        {
            Serial.println(
                "WiFi recovery attempt timed out."
            );

            WiFi.disconnect();

            wifiReconnectInProgress = false;

            digitalWrite(
                LED_BUILTIN,
                LOW
            );

            lastWiFiAttempt = millis();
        }

        return;
    }

    // --------------------------------------------------------
    // Start a new recovery attempt
    // --------------------------------------------------------

    if (millis() - lastWiFiAttempt >=
        WIFI_RECONNECT_INTERVAL)
    {
        startWiFiReconnect();
    }
}


// ============================================================
// START NTP RESYNCHRONIZATION
// ============================================================

void startNTPResync(bool startupMode)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    Serial.println();

    if (startupMode)
    {
        Serial.println(
            "Starting startup NTP synchronization..."
        );
    }
    else
    {
        Serial.println(
            "Requesting NTP clock resynchronization..."
        );
    }

    // Native Arduino-Pico NTP API.
    // NTP updates the Pico's internal system clock in the background.
    NTP.begin(
        "pool.ntp.org",
        "time.nist.gov"
    );

    ntpResyncInProgress = true;
    ntpRetryPending = false;
    lastNtpAttempt = millis();

    if (startupMode)
    {
        lastNtpSyncMillis = 0;
    }

    Serial.println("NTP client started.");
}


// ============================================================
// STARTUP NTP SYNCHRONIZATION
// ============================================================

bool synchronizeTimeAtStartup()
{
    Serial.println();
    Serial.println(
        "Synchronizing time from Internet..."
    );

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fetching Internet");
    lcd.setCursor(0, 1);
    lcd.print("Date & Time...");

    // A valid clock may already exist after a software restart
    // while the Pico has remained powered.
    bool hadValidClock = isClockValid();

    startNTPResync(true);

    if (hadValidClock)
    {
        clockValid = true;
        clockSource = CLOCK_INTERNAL;
        ntpResyncInProgress = false;
        ntpRetryPending = false;
        lastNtpSyncMillis = millis();

        Serial.println(
            "Existing Pico clock is valid; NTP correction runs in background."
        );

        displayRealTime();
        return true;
    }

    // Fresh power-up: the clock is invalid, so wait for NTP to set it.
    bool ntpSuccess = NTP.waitSet(
        NTP_STARTUP_TIMEOUT
    );

    ntpResyncInProgress = false;

    if (ntpSuccess && isClockValid())
    {
        clockValid = true;
        clockSource = CLOCK_NTP;
        ntpRetryPending = false;
        lastNtpSyncMillis = millis();

        Serial.println();
        Serial.println(
            "NTP synchronization successful."
        );

        displayRealTime();
        return true;
    }

    clockValid = false;
    clockSource = CLOCK_UNSYNCED;
    ntpRetryPending = true;
    lastNtpAttempt = millis();

    Serial.println();
    Serial.println(
        "Startup NTP synchronization failed."
    );

    return false;
}


// ============================================================
// MAINTAIN NTP
// ============================================================

void maintainNTP()
{
    // No WiFi: keep using the Pico's internal running clock.
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    // NTP runs in the background after NTP.begin().
    // Arduino-Pico 5.6.0 does not expose the ESP/lwIP SNTP
    // completion callback used by some other environments, so
    // we do not falsely claim a particular NTP packet completed.
    if (ntpResyncInProgress)
    {
        if (millis() - lastNtpAttempt >=
            NTP_RECOVERY_TIMEOUT)
        {
            ntpResyncInProgress = false;

            if (isClockValid())
            {
                clockValid = true;
                ntpRetryPending = false;
                lastNtpSyncMillis = millis();

                // Measurement timestamps remain based on the Pico's
                // internal running clock. NTP corrects that clock in
                // the background.
                if (clockSource == CLOCK_UNSYNCED)
                {
                    clockSource = CLOCK_INTERNAL;
                }

                Serial.println(
                    "NTP background synchronization window completed."
                );
            }
            else
            {
                ntpRetryPending = true;
                Serial.println(
                    "NTP time not obtained; retry pending."
                );
            }
        }

        return;
    }

    // No valid clock yet, or a previous NTP request did not obtain time.
    if (!clockValid ||
        !isClockValid() ||
        ntpRetryPending)
    {
        if (millis() - lastNtpAttempt >=
            NTP_RETRY_INTERVAL)
        {
            startNTPResync(false);
        }

        return;
    }

    // Periodically restart NTP so the internal clock can be corrected.
    if (millis() - lastNtpSyncMillis >=
        NTP_PERIODIC_SYNC_INTERVAL)
    {
        startNTPResync(false);
    }
}


// ============================================================
// CHECK CLOCK VALIDITY
// ============================================================

bool isClockValid()
{
    return (
        time(nullptr) >= VALID_EPOCH
    );
}


// ============================================================
// SET CLOCK SOURCE
// ============================================================

// ============================================================
// FORMAT IST DATE / TIME
// ============================================================
// time(nullptr) is a Unix timestamp.
// We keep the clock itself as UTC epoch time and apply
// +05:30 only when formatting for display/logging.
// This avoids depending on platform-specific timezone behavior.

void formatISTDateTime(
    time_t utcTime,
    char *dateBuffer,
    size_t dateBufferSize,
    char *timeBuffer,
    size_t timeBufferSize
)
{
    time_t istTime =
        utcTime + gmtOffset_sec;

    struct tm *timeinfo =
        gmtime(&istTime);

    if (!timeinfo)
    {
        snprintf(
            dateBuffer,
            dateBufferSize,
            "INVALID"
        );

        snprintf(
            timeBuffer,
            timeBufferSize,
            "INVALID"
        );

        return;
    }

    snprintf(
        dateBuffer,
        dateBufferSize,
        "%02d-%02d-%04d",
        timeinfo->tm_mday,
        timeinfo->tm_mon + 1,
        timeinfo->tm_year + 1900
    );

    snprintf(
        timeBuffer,
        timeBufferSize,
        "%02d:%02d:%02d",
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec
    );
}


// ============================================================
// GET MEASUREMENT DATE / TIME
// ============================================================

void getMeasurementDateTime(
    char *dateBuffer,
    size_t dateBufferSize,
    char *timeBuffer,
    size_t timeBufferSize,
    const char **sourceText
)
{
    if (clockValid &&
        isClockValid())
    {
        time_t now =
            time(nullptr);

        formatISTDateTime(
            now,
            dateBuffer,
            dateBufferSize,
            timeBuffer,
            timeBufferSize
        );

        if (clockSource == CLOCK_NTP)
        {
            *sourceText = "NTP";
        }
        else
        {
            *sourceText = "INTERNAL";
        }

        return;
    }

    snprintf(
        dateBuffer,
        dateBufferSize,
        "UNSYNCED"
    );

    snprintf(
        timeBuffer,
        timeBufferSize,
        "--:--:--"
    );

    *sourceText = "UNSYNCED";
}


// ============================================================
// DISPLAY REAL INTERNET / INTERNAL CLOCK
// ============================================================

void displayRealTime()
{
    if (!clockValid ||
        !isClockValid())
    {
        displayTimeFailure();
        return;
    }

    time_t now =
        time(nullptr);

    char dateBuffer[16];
    char timeBuffer[16];

    formatISTDateTime(
        now,
        dateBuffer,
        sizeof(dateBuffer),
        timeBuffer,
        sizeof(timeBuffer)
    );

    lcd.clear();

    lcd.setCursor(0, 0);

    if (clockSource == CLOCK_NTP)
    {
        lcd.print("Internet Time OK");
    }
    else
    {
        lcd.print("Internal Clock");
    }

    lcd.setCursor(0, 1);
    lcd.print(dateBuffer);

    lcd.setCursor(0, 2);
    lcd.print(timeBuffer);

    lcd.setCursor(0, 3);
    lcd.print("IST  UTC+05:30");

    Serial.println();
    Serial.println("Current Pico clock:");

    Serial.print(dateBuffer);
    Serial.print(" ");
    Serial.println(timeBuffer);

    Serial.print("Clock source: ");

    if (clockSource == CLOCK_NTP)
    {
        Serial.println("NTP synchronized");
    }
    else
    {
        Serial.println("Internal running clock");
    }
}


// ============================================================
// TIME FETCH FAILURE
// ============================================================

void displayTimeFailure()
{
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("TIME FETCH FAILED");

    lcd.setCursor(0, 1);
    lcd.print("Internet time");

    lcd.setCursor(0, 2);
    lcd.print("not available");

    lcd.setCursor(0, 3);
    lcd.print("Creating Run ID...");
}


// ============================================================
// BOOT COUNTER
// ============================================================

unsigned long readLegacyBootCounter()
{
    if (!sdAvailable)
    {
        return 0;
    }

    File file = SD.open(
        "bootcnt.txt",
        FILE_READ
    );

    if (!file)
    {
        return 0;
    }

    String value =
        file.readStringUntil('\n');

    file.close();

    long parsed = value.toInt();

    return (parsed < 0) ? 0UL : (unsigned long)parsed;
}


unsigned long readRunLogCounter()
{
    if (!sdAvailable)
    {
        return 0;
    }

    File file = SD.open(
        "runlog.txt",
        FILE_READ
    );

    if (!file)
    {
        return 0;
    }

    unsigned long maximum = 0;

    while (file.available())
    {
        String value =
            file.readStringUntil('\n');

        long parsed = value.toInt();

        if (parsed >= 0 &&
            (unsigned long)parsed > maximum)
        {
            maximum = (unsigned long)parsed;
        }
    }

    file.close();

    return maximum;
}


// ============================================================
// BOOT COUNTER
// ============================================================

unsigned long getBootCounter()
{
    if (!sdAvailable)
    {
        return 1;
    }

    // Prefer the append-only run log. The old bootcnt.txt is
    // read as a migration path from earlier firmware versions.
    unsigned long counter = readRunLogCounter();
    unsigned long legacy = readLegacyBootCounter();

    if (legacy > counter)
    {
        counter = legacy;
    }

    counter++;
    saveBootCounter(counter);

    return counter;
}


// ============================================================
// SAVE BOOT COUNTER
// ============================================================

void saveBootCounter(unsigned long counter)
{
    if (!sdAvailable)
    {
        return;
    }

    // Append-only storage preserves the previous run number if
    // power is interrupted during a new write.
    File file = SD.open(
        "runlog.txt",
        FILE_WRITE
    );

    if (file)
    {
        file.println(counter);
        file.close();
    }
}


// ============================================================
// GENERATE OFFLINE REFERENCE TIMESTAMP
// ============================================================
// This is NOT real date/time.
// It is only a repeatable operator reference based on
// firmware compile date/time + run number.
//
// Actual measurements are marked UNSYNCED until NTP has
// successfully set the Pico's internal clock.

void generateOfflineTimestamp(
    unsigned long bootNumber,
    char *buffer,
    size_t bufferSize
)
{
    const char *compileDate =
        __DATE__;

    const char *compileTime =
        __TIME__;

    struct tm baseTime = {};

    char monthString[4];

    memcpy(
        monthString,
        compileDate,
        3
    );

    monthString[3] = '\0';

    const char *months[] =
    {
        "Jan", "Feb", "Mar", "Apr",
        "May", "Jun", "Jul", "Aug",
        "Sep", "Oct", "Nov", "Dec"
    };

    int month = 0;

    for (int i = 0; i < 12; i++)
    {
        if (strcmp(
                monthString,
                months[i]) == 0)
        {
            month = i;
            break;
        }
    }

    int day = 1;
    int year = 2026;

    sscanf(
        compileDate + 4,
        "%d %d",
        &day,
        &year
    );

    int hour = 0;
    int minute = 0;
    int second = 0;

    sscanf(
        compileTime,
        "%d:%d:%d",
        &hour,
        &minute,
        &second
    );

    baseTime.tm_year =
        year - 1900;

    baseTime.tm_mon =
        month;

    baseTime.tm_mday =
        day;

    baseTime.tm_hour =
        hour;

    baseTime.tm_min =
        minute;

    baseTime.tm_sec =
        second;

    time_t pseudoTime =
        mktime(&baseTime);

    pseudoTime +=
        bootNumber;

    struct tm *pseudo =
        localtime(&pseudoTime);

    if (!pseudo)
    {
        snprintf(
            buffer,
            bufferSize,
            "UNAVAILABLE"
        );

        return;
    }

    snprintf(
        buffer,
        bufferSize,
        "%02d-%02d-%04d %02d:%02d:%02d",
        pseudo->tm_mday,
        pseudo->tm_mon + 1,
        pseudo->tm_year + 1900,
        pseudo->tm_hour,
        pseudo->tm_min,
        pseudo->tm_sec
    );
}


// ============================================================
// DISPLAY OFFLINE RUN REFERENCE
// ============================================================

void displayOfflineTime(
    const char *timestamp,
    unsigned long bootNumber
)
{
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("OFFLINE RUN ID");

    char datePart[11];
    char timePart[9];

    memcpy(
        datePart,
        timestamp,
        10
    );

    datePart[10] = '\0';

    memcpy(
        timePart,
        timestamp + 11,
        8
    );

    timePart[8] = '\0';

    lcd.setCursor(0, 1);
    lcd.print(datePart);

    lcd.setCursor(0, 2);
    lcd.print(timePart);

    lcd.setCursor(0, 3);
    lcd.print("RUN ID:");
    lcd.print(bootNumber);
}


// ============================================================
// SESSION HEADER
// ============================================================

void logSessionHeader(
    float startupBatteryVoltage,
    bool timeWasInitiallySynced,
    const char *offlineTimestamp
)
{
    if (!sdAvailable)
    {
        return;
    }

    myFile =
        SD.open(
            "test.txt",
            FILE_WRITE
        );

    if (!myFile)
    {
        Serial.println(
            "Error opening test.txt "
            "for session header."
        );

        sdAvailable = false;
        lastSDAttempt = millis();

        return;
    }

    myFile.println();
    myFile.println(
        "========================================"
    );

    myFile.println(
        "ECOLOGICAL DATA COLLECTION SESSION"
    );

    myFile.println(
        "========================================"
    );

    myFile.print(
        "Battery Voltage at Startup: "
    );

    myFile.print(
        startupBatteryVoltage,
        2
    );

    myFile.println(" V");

    myFile.print(
        "Boot / Run Number: "
    );

    myFile.println(
        bootCounter
    );

    if (timeWasInitiallySynced)
    {
        time_t current =
            time(nullptr);

        char dateBuffer[16];
        char timeBuffer[16];

        formatISTDateTime(
            current,
            dateBuffer,
            sizeof(dateBuffer),
            timeBuffer,
            sizeof(timeBuffer)
        );

        myFile.print(
            "Initial Internet Date: "
        );

        myFile.println(
            dateBuffer
        );

        myFile.print(
            "Initial Internet Time: "
        );

        myFile.println(
            timeBuffer
        );
    }
    else
    {
        myFile.println(
            "WARNING: Initial Internet time unavailable"
        );

        myFile.print(
            "Offline Reference Timestamp: "
        );

        myFile.println(
            offlineTimestamp
        );
    }

    myFile.println();

    myFile.println(
        "Run_ID\tDate\tTime\tClock_Source\tElapsed_s"
        "\tTemperature\tHumidity\tCO\tCO2\tO2\tBattery_V"
    );

    myFile.close();

    Serial.println(
        "SD session header written."
    );
}


// ============================================================
// LOG NTP EVENT
// ============================================================

void logTimeSyncEvent(
    const char *reason
)
{
    if (!sdAvailable)
    {
        return;
    }

    myFile =
        SD.open(
            "test.txt",
            FILE_WRITE
        );

    if (!myFile)
    {
        sdAvailable = false;
        lastSDAttempt = millis();

        return;
    }

    time_t now =
        time(nullptr);

    char dateBuffer[16];
    char timeBuffer[16];

    if (clockValid)
    {
        formatISTDateTime(
            now,
            dateBuffer,
            sizeof(dateBuffer),
            timeBuffer,
            sizeof(timeBuffer)
        );

        myFile.println();

        myFile.print(
            "# TIME_SYNC_EVENT\t"
        );

        myFile.print(
            reason
        );

        myFile.print("\t");

        myFile.print(
            dateBuffer
        );

        myFile.print("\t");

        myFile.println(
            timeBuffer
        );
    }

    myFile.close();
}


// ============================================================
// MAINTAIN SD CARD
// ============================================================

void maintainSD()
{
    if (sdAvailable)
    {
        return;
    }

    if (millis() - lastSDAttempt <
        SD_RETRY_INTERVAL)
    {
        return;
    }

    lastSDAttempt =
        millis();

    Serial.println(
        "Retrying SD card initialization..."
    );

    if (SD.begin(chipSelect))
    {
        sdAvailable = true;

        Serial.println(
            "SD card recovered."
        );

        // Write an event marker. Do not change the
        // existing run number when the card returns.
        myFile =
            SD.open(
                "test.txt",
                FILE_WRITE
            );

        if (myFile)
        {
            myFile.println();
            myFile.println(
                "# SD_CARD_RECOVERED"
            );
            myFile.close();
        }
    }
    else
    {
        Serial.println(
            "SD card still unavailable."
        );
    }
}


// ============================================================
// BACKGROUND SERVICE
// ============================================================

void serviceConnectivity()
{
    maintainSD();
    maintainWiFi();
    watchdog_update();
}


void serviceNTP()
{
    maintainNTP();
    watchdog_update();
}


// ============================================================
// I2C RECOVERY
// ============================================================

void recoverI2C()
{
    Serial.println(
        "Resetting I2C bus..."
    );

    Wire.end();
    delay(100);

    Wire.begin();

    Wire.setTimeout(3000);
    Wire.setClock(50000);

    if (!sensor.begin())
    {
        Serial.println(
            "SCD40 restart failed"
        );
    }
    else
    {
        Serial.println(
            "SCD40 restarted."
        );
    }

    lcd.init();
    lcd.backlight();

    oxygen.begin(
        Oxygen_IICAddress
    );

    gas.begin();

    gas.setTempCompensation(
        gas.ON
    );

    gas.changeAcquireMode(
        gas.INITIATIVE
    );
}
