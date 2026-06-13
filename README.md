# Smart Ecological Observation Station

An IoT-enabled ecological monitoring system based on the Raspberry Pi Pico W for real-time collection, storage, display, and cloud transmission of environmental data.

The system continuously monitors atmospheric carbon dioxide (CO₂), oxygen concentration (O₂), carbon monoxide (CO), temperature, and relative humidity. Measurements are displayed locally on an LCD screen, stored on an SD card for offline analysis, and uploaded to ThingSpeak for remote visualization and long-term ecological data collection.

---

## Overview

Environmental monitoring plays an important role in ecological studies, air quality assessment, greenhouse management, and environmental research. This project provides a low-cost and portable solution for collecting atmospheric data using commercially available sensors and cloud-based data logging.

The station performs:

* Real-time environmental monitoring
* Local SD card data logging
* Cloud-based data storage using ThingSpeak
* WiFi-enabled remote access
* NTP time synchronization
* Multi-sensor ecological data collection

---

## Measured Parameters

| Parameter            | Sensor                         |
| -------------------- | ------------------------------ |
| Carbon Dioxide (CO₂) | SCD40 Photoacoustic CO₂ Sensor |
| Oxygen (O₂)          | DFRobot Oxygen Sensor          |
| Carbon Monoxide (CO) | DFRobot MultiGas Sensor        |
| Temperature          | DHT22                          |
| Relative Humidity    | DHT22                          |

---

## Hardware Components

### Microcontroller

* Raspberry Pi Pico W

### Sensors

* SCD40 CO₂ Sensor
* DFRobot Oxygen Sensor
* DFRobot MultiGas Sensor (CO)
* DHT22 Temperature and Humidity Sensor

### Peripherals

* 20×4 I²C LCD Display
* MicroSD Card Module

### Communication

* WiFi (Pico W onboard)

---

## System Architecture

Environmental Sensors
↓
Raspberry Pi Pico W
↓
├── LCD Display
├── SD Card Logging
└── ThingSpeak Cloud Upload

---

## Features

* Real-time environmental monitoring
* Multi-sensor ecological data acquisition
* Cloud data visualization using ThingSpeak
* Local data backup on SD card
* WiFi connectivity
* Automatic time synchronization using NTP
* LCD-based local display
* I²C recovery routine for robust operation

---

## Data Collection Workflow

1. Establish WiFi connection.
2. Synchronize time using NTP servers.
3. Read temperature and humidity from DHT22.
4. Read oxygen concentration from DFRobot Oxygen Sensor.
5. Read carbon monoxide concentration from DFRobot MultiGas Sensor.
6. Acquire three consecutive CO₂ measurements from the SCD40 sensor and calculate the average value.
7. Display the measured values on the LCD.
8. Save measurements to the SD card.
9. Upload measurements to ThingSpeak.
10. Repeat the cycle continuously.

---

## ThingSpeak Channel Fields

| Field   | Parameter                   |
| ------- | --------------------------- |
| Field 1 | Temperature (°C)            |
| Field 2 | Relative Humidity (%)       |
| Field 3 | Oxygen Concentration (%Vol) |
| Field 4 | Carbon Dioxide (ppm)        |
| Field 5 | Carbon Monoxide (ppm)       |

---

## Example Output

Temperature: 33.88 °C

Humidity: 82.85 %

CO₂: 505 ppm

O₂: 23.21 %Vol

CO: 0.00 ppm

Channel update successful.

---

## Libraries Used

* 7semi_SCD40
* ThingSpeak
* WiFi
* DHT Sensor Library
* LiquidCrystal_I2C
* DFRobot Oxygen Sensor Library
* DFRobot MultiGas Sensor Library
* SD
* SPI

---

## Applications

* Ecological Data Collection
* Environmental Monitoring
* Air Quality Monitoring
* Smart Agriculture
* Greenhouse Monitoring
* Educational Projects
* IoT Research
* Citizen Science

---

## Future Improvements

* PM2.5 and PM10 monitoring
* GPS-based geotagging
* Solar-powered deployment
* Battery backup system
* LoRaWAN communication
* VOC monitoring
* AI-based environmental trend prediction

---

## Author

Soumen Mondal

Research Scholar

Research Interests:

* Computational Chemistry
* Environmental Monitoring
* Embedded Systems
* Internet of Things (IoT)
* Machine Learning

---

## License

MIT License
