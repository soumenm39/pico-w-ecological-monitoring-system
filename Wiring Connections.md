| From | To | Description |
|---|---|---|
| **External 3.3 V supply** | Common VCC rail | External power for all sensors/modules; **do not take VCC from Pico W** |
| **External GND** | Common GND rail | Common ground for the complete system |
| **Common VCC rail** | LCD VCC | 20×4 I²C LCD power |
| **Common VCC rail** | SCD40 VCC | CO₂ sensor power |
| **Common VCC rail** | DFRobot Oxygen Sensor VCC | O₂ sensor power |
| **Common VCC rail** | DFRobot Multi Gas Sensor VCC | CO sensor power |
| **Common VCC rail** | MicroSD VCC | SD card module power |
| **Common GND rail** | LCD GND | LCD ground |
| **Common GND rail** | SCD40 GND | CO₂ sensor ground |
| **Common GND rail** | Oxygen Sensor GND | O₂ sensor ground |
| **Common GND rail** | Multi Gas Sensor GND | CO sensor ground |
| **Common GND rail** | MicroSD GND | SD card ground |
| **Common GND rail** | Pico W GND | Establishes common ground with Pico |
| **Pico GP20** | Common SDA rail | Shared I²C data line |
| **Common SDA rail** | LCD SDA | LCD I²C data |
| **Common SDA rail** | SCD40 SDA | CO₂ sensor I²C data |
| **Common SDA rail** | Oxygen Sensor SDA | O₂ sensor I²C data |
| **Common SDA rail** | Multi Gas Sensor SDA | CO sensor I²C data |
| **Pico GP21** | Common SCL rail | Shared I²C clock line |
| **Common SCL rail** | LCD SCL | LCD I²C clock |
| **Common SCL rail** | SCD40 SCL | CO₂ sensor I²C clock |
| **Common SCL rail** | Oxygen Sensor SCL | O₂ sensor I²C clock |
| **Common SCL rail** | Multi Gas Sensor SCL | CO sensor I²C clock |
| **DHT22 VCC** | Common VCC rail | DHT22 power |
| **DHT22 GND** | Common GND rail | DHT22 ground |
| **DHT22 DATA** | **Pico GP22** | Temperature and humidity data |
| **MicroSD MISO** | **Pico GP16** | SPI data from SD card |
| **MicroSD MOSI** | **Pico GP19** | SPI data to SD card |
| **MicroSD SCK** | **Pico GP18** | SPI clock |
| **MicroSD CS/SS** | **Pico GP17** | SD card chip-select |
| **Battery +** | **100 kΩ R1** | Battery-voltage sensing divider |
| **100 kΩ R1** | **Pico GP27 / ADC1** | Divided battery-voltage measurement |
| **Pico GP27 / ADC1** | **100 kΩ R2** | Lower half of voltage divider |
| **100 kΩ R2** | Battery − / Common GND | Voltage-divider return |
| **Pico W Wi-Fi** | ThingSpeak | Wireless cloud telemetry |
