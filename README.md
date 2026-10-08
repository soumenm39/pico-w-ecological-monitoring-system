# 🌿 Smart Ecological Observation Station

A portable, multi-parameter environmental monitoring system developed for **field-based ecological and environmental observations**.

The system is built around a **Raspberry Pi Pico W** and integrates multiple environmental sensors to measure atmospheric parameters including **CO₂, CO, O₂ concentration, temperature, and relative humidity**. Measurements are stored locally on an **SD card** and can also be transmitted to the **ThingSpeak IoT cloud platform** through Wi-Fi.

The system is designed for long-duration operation with emphasis on **reliable sensor acquisition, local data preservation, Wi-Fi recovery, time synchronization, and battery monitoring**.

---

## 🌱 Project Overview

Environmental and ecological field measurements often require monitoring several atmospheric parameters simultaneously over extended periods.

This project addresses that requirement by combining multiple sensors, local data logging, wireless telemetry, and battery monitoring into a single portable embedded system.

The station can continue collecting and storing measurements locally even when the Wi-Fi connection is unavailable. When connectivity is restored, the system automatically attempts to reconnect and resume cloud communication.

### Main measured parameters

- 🌡️ Temperature
- 💧 Relative humidity
- 🫁 Oxygen concentration (O₂)
- 🏭 Carbon monoxide (CO)
- 🌫️ Carbon dioxide (CO₂)
- 🔋 Battery voltage

---

## ✨ Key Features

### Multi-parameter environmental monitoring

The station integrates multiple sensors through the I²C interface and periodically collects:

| Parameter | Sensor |
|---|---|
| CO₂ | Sensirion SCD40 |
| Temperature | DHT22 |
| Relative Humidity | DHT22 |
| O₂ | DFRobot Oxygen Sensor |
| CO | DFRobot MultiGas Sensor |
| Battery Voltage | RP2040 ADC |

---

### 💾 Local SD Card Data Logging

The **SD card acts as the primary data storage system**.

Each measurement cycle records:

- Run ID
- Date
- Time
- Clock source
- Elapsed time
- Temperature
- Relative humidity
- CO
- CO₂
- O₂
- Battery voltage

Local storage allows the station to continue operating even when Internet connectivity is unavailable.

---

### ☁️ ThingSpeak Cloud Connectivity

Environmental measurements can be uploaded to **ThingSpeak** when Wi-Fi connectivity is available.

The current channel mapping is:

| ThingSpeak Field | Parameter |
|---|---|
| Field 1 | Temperature |
| Field 2 | Relative Humidity |
| Field 3 | O₂ |
| Field 4 | CO₂ |
| Field 5 | CO |

Battery voltage is currently recorded locally on the SD card.

---

### 📡 Wi-Fi Recovery

The system is designed so that loss of Wi-Fi does not stop environmental data collection.

If Wi-Fi is lost:

```text
Wi-Fi disconnected
        ↓
Continue sensor measurements
        ↓
Continue SD-card logging
        ↓
Attempt Wi-Fi recovery
        ↓
Connection restored
        ↓
Resume cloud communication
```

This makes the system more suitable for unattended field operation.

---

### 🕐 NTP Time Synchronization

The Raspberry Pi Pico W synchronizes its internal system clock using Internet NTP.

The firmware follows this architecture:

```text
Internet available
       ↓
NTP synchronization
       ↓
Pico internal clock
       ↓
Measurement timestamps
```

If Wi-Fi temporarily disappears, the Pico continues using its internal running clock.

When Wi-Fi returns, the system can initiate another NTP synchronization to correct the clock.

For a complete power loss followed by startup without Internet access, the firmware generates an **offline run identifier/reference** because the RP2040 does not have a battery-backed real-time clock.

---

## 🔋 Battery Monitoring

Battery voltage is measured using the RP2040 ADC.

Current hardware configuration:

```text
Battery +
    │
   10 kΩ
    │
    ├──────── GP27 / ADC1
    │
   10 kΩ
    │
   GND
```

This provides a 1:2 voltage divider.

For a battery voltage of approximately 4.1 V:

```text
ADC voltage ≈ 4.1 / 2
            ≈ 2.05 V
```

which remains within the RP2040 ADC input range.

The firmware averages multiple ADC samples to reduce measurement noise.

A calibration factor is also included so that the ADC measurement can be corrected against a calibrated multimeter measurement.

---

## 🖥️ LCD Startup Display

The station uses an I²C LCD for local status information.

The startup sequence displays the battery voltage before the rest of the system initialization continues.

Example:

```text
Bat Vol: 4.11 V
Battery check...
```

The battery voltage is displayed for approximately **20 seconds**, followed by a short startup status message.

---

## 🔌 Hardware

### Main Controller

- Raspberry Pi Pico W

### Sensors

- Sensirion SCD40 CO₂ sensor
- DFRobot Oxygen Sensor
- DFRobot MultiGas Sensor
- DHT22 temperature/humidity sensor

### Storage

- MicroSD card module

### Display

- I²C LCD

### Power

- Single-cell Li-ion battery
- Battery voltage divider
- External regulated power distribution

---

## 🔗 Communication Interfaces

### I²C

Multiple sensors share the Pico's I²C bus.

Current device addresses include:

| Device | I²C Address |
|---|---:|
| LCD | `0x27` |
| SCD40 | `0x62` |
| Oxygen Sensor | `0x73` |
| MultiGas Sensor | `0x74` |

The firmware also includes an I²C recovery routine intended to reinitialize the bus and connected sensors after communication problems.

---

### SPI

The SD card uses the Pico's SPI interface.

Current configuration:

| Signal | Pico GPIO |
|---|---:|
| MISO | GP16 |
| MOSI | GP19 |
| SCK | GP18 |
| CS | GP17 |

---

### DHT22

The DHT22 data line is connected to:

```text
GPIO22
```

---

### Battery ADC

Battery voltage is measured using:

```text
GPIO27 / ADC1
```

with a 10 kΩ / 10 kΩ voltage divider.

---

## 🧠 Firmware Architecture

The firmware is organized into several functional sections:

```text
                    ┌──────────────────────┐
                    │   Raspberry Pi Pico W│
                    └──────────┬───────────┘
                               │
             ┌─────────────────┼─────────────────┐
             │                 │                 │
             ▼                 ▼                 ▼
          Sensors           SD Card           Wi-Fi
             │                 │                 │
             │                 │                 ▼
             │                 │             ThingSpeak
             │                 │
             ▼                 ▼
       Measurement        Local Storage
             │
             ▼
        LCD Display
```

The main firmware responsibilities are:

1. Sensor initialization
2. Environmental data acquisition
3. Battery voltage measurement
4. LCD status display
5. SD-card data logging
6. Wi-Fi management
7. NTP synchronization
8. ThingSpeak telemetry
9. I²C recovery
10. Watchdog servicing

---

## 📊 Measurement Cycle

A typical measurement cycle follows this sequence:

```text
Start measurement cycle
        │
        ▼
Read DHT22
        │
        ▼
Read O₂
        │
        ▼
Read CO
        │
        ▼
Read SCD40
        │
        ▼
Calculate averaged values
        │
        ▼
Read battery voltage
        │
        ▼
Generate timestamp
        │
        ▼
Write data to SD card
        │
        ├───────────────┐
        │               │
        ▼               ▼
    Wi-Fi available   Wi-Fi unavailable
        │               │
        ▼               ▼
 ThingSpeak upload   Continue locally
        │
        ▼
Next measurement cycle
```

---

## 🛡️ Reliability Features

The firmware includes several mechanisms intended for long-duration operation:

- Sensor averaging
- ADC averaging for battery measurement
- I²C bus recovery
- Wi-Fi reconnection
- SD-card recovery attempts
- NTP resynchronization
- Local SD storage during network outages
- Watchdog servicing
- Measurement validity checks
- Run/session identification

The design prioritizes **local data preservation** so that loss of Internet connectivity does not automatically result in loss of environmental measurements.

---

## 🔋 Power Consumption

The current system consumes approximately:

```text
~170 mA
```

under the observed operating conditions.

The battery voltage measurement divider uses:

```text
100 kΩ + 100 kΩ
```

which draws approximately:

```text
~0.02 mA
```

from a 3.7 V Li-ion battery.

Therefore, the voltage-divider power consumption is very small compared with the overall system consumption.

---

## 📁 Suggested Repository Structure

```text
Smart-Ecological-Observation-Station/
│
├── README.md
│
├── firmware/
│   └── Ecological_Observation_Station.ino
│
├── hardware/
│   ├── wiring/
│   └── circuit_diagram/
│
├── data/
│   └── example/
│
├── documentation/
│   └── system_architecture.md
│
└── images/
    ├── station.jpg
    ├── wiring.jpg
    └── lcd_display.jpg
```

---

## ⚙️ Software Requirements

The firmware is developed using:

- Arduino IDE
- Arduino-Pico core for RP2040
- Raspberry Pi Pico W

### Required libraries

```text
7semi_SCD40
LiquidCrystal_I2C
DFRobot Oxygen Sensor
DFRobot MultiGas Sensor
DHT sensor library
WiFi
ThingSpeak
SD
WiFiNTP
```

Install the required libraries through the Arduino IDE Library Manager where available.

---

## 🚀 Getting Started

### 1. Install Arduino IDE

Install Arduino IDE and configure the RP2040 board support.

### 2. Select the board

Select:

```text
Raspberry Pi Pico W
```

using the Arduino-Pico RP2040 core.

### 3. Connect the hardware

Connect the sensors according to the wiring configuration described in the repository.

### 4. Configure Wi-Fi

Update the firmware with your own Wi-Fi credentials:

```cpp
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";
```

### 5. Configure ThingSpeak

Add your own channel number and Write API Key:

```cpp
unsigned long myChannelNumber = YOUR_CHANNEL_NUMBER;
const char *myWriteAPIKey = "YOUR_WRITE_API_KEY";
```

**Do not commit real Wi-Fi passwords or ThingSpeak API keys to GitHub.**

### 6. Upload the firmware

Compile and upload the firmware to the Raspberry Pi Pico W.

### 7. Insert an SD card

Insert a suitable FAT-formatted microSD card.

The system will create and update the local measurement file during operation.

---

## 📄 Example Data Format

The SD-card data contains fields similar to:

```text
Run_ID    Date    Time    Clock_Source    Elapsed_s    Temperature    Humidity    CO    CO2    O2    Battery_V
```

Example:

```text
12    08-10-2026    18:42:10    NTP    125    27.4    64.2    1.2    425    20.8    4.11
```

---

## 🔬 Intended Applications

The system can be adapted for:

- Ecological field surveys
- Environmental monitoring
- Atmospheric measurements
- Greenhouse monitoring
- Outdoor air-quality observations
- Long-duration sensor deployment
- IoT-based environmental research
- Educational embedded-systems projects
- Field data acquisition

The actual suitability for a scientific application depends on sensor calibration, environmental conditions, deployment design, and validation against appropriate reference instruments.

---

## 🔮 Future Improvements

Possible future developments include:

- [ ] GPS-based geotagging
- [ ] Additional particulate-matter sensors
- [ ] Solar charging
- [ ] Improved battery management
- [ ] Low-power operating modes
- [ ] External RTC with backup battery
- [ ] Automatic sensor calibration routines
- [ ] Local web dashboard
- [ ] Telegram-based monitoring
- [ ] MQTT support
- [ ] Data visualization dashboard
- [ ] Enclosure for field deployment
- [ ] Automated sensor health diagnostics
- [ ] Long-term data analysis and anomaly detection

---

## 👨‍🔬 Project Background

This project was developed as a **portable environmental data-acquisition platform for ecological research applications**, integrating embedded systems, sensor interfacing, local data logging, and IoT connectivity.

The system was developed under the guidance of:

**Dr. Narayan Chandra Karmakar**  
Professor of Botany and Ecology  
Barasat Government College

---

## 📜 License

Add the appropriate open-source license for your intended use.

For example:

```text
MIT License
```

if you want others to freely use, modify, and redistribute the software subject to the MIT license terms.

---

## ⭐ Acknowledgements

This project uses hardware and software libraries provided by the respective sensor and open-source communities.

Special acknowledgement to the developers and manufacturers of the Raspberry Pi Pico W, Sensirion SCD40, DFRobot sensors, Arduino-Pico ecosystem, ThingSpeak, and the open-source libraries used by this project.
