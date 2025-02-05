# Air Quality Checker

## Inspiration

- https://www.hackster.io/mcmchris/visual-co2-indoor-air-quality-sensor-509d4c
- https://howtomechatronics.com/projects/diy-air-quality-monitor-pm2-5-co2-voc-ozone-temp-hum-arduino-meter/

## How to

- Install Platform IDE Extension
- Install USB Driver
  - See: https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/using-arduino-ide for download
  - Open `Device Manager`
  - Install driver locally from PC
  - Select the unzipped folder
- Create `src/secrets.h` and replace placeholders:

    ```c
    #ifndef SECRETS_H
    #define SECRETS_H

    #define WIFI_SSID "<<YourWiFiSSID>>"
    #define WIFI_PASSWORD "<<YourWiFiPassword>>"
    #define SERVER_URL "<<http://dein-server.com/upload>>"

    #endif // SECRETS_H
    ```



### How to connect the electronics?

- [Setup 1](./doc/setup-1.jpg)

## Thoughts / Discussions / TODOs

- There are sensors that already have some calculation capabilities - is this a benefit?
  > The BME680 doesn't have built-in air quality calcualtion capabilities like other sensors like the SGP30 or CCS811. Instead, you only get temperature, pressure, humidity and gas resistance (the raw resistance value of the sensor in the BME60. So we have to use a separate library from Bosch to perform the conversion to get Air Quality values like the VOC and equivalent CO2.
  - https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas/bsec-air-quality-library
- Next Project: Weather station
  - https://learn.adafruit.com/wifi-weather-station-with-tft-display

## Components

### Assembled Feather HUZZAH w/ ESP8266 WiFi With Stacking Headers

- https://www.adafruit.com/product/3213
- https://learn.adafruit.com/adafruit-feather-huzzah-esp8266
- [Pinout](./doc/adafruit_products_Huzzah_ESP8266_Pinout_v1.2-1.png)
  - https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/pinouts

**Spec:**
- Measures 2.0" x 0.9" x 0.28" (51mm x 23mm x 8mm) without headers soldered in
- Light as a (large?) feather - 6 grams
- ESP8266 @ 80MHz or 160 MHz with 3.3V logic/power
- 4MB of FLASH (32 MBit)
- 3.3V regulator with 500mA peak current output
- CP2104 USB-Serial converter onboard with 921600 max baudrate for uploading
- Auto-reset support for getting into bootload mode before firmware upload
- 9 GPIO pins - can also be used as I2C and SPI
- 1 x analog inputs 1.0V max
- Built in 100mA lipoly charger with charging status indicator LED
- Pin #0 red LED for general purpose blinking. Pin #2 blue LED for bootloading debug & general purpose blinking
- Power/enable pin
- 4 mounting holes
- Reset button

#### ESP8266

- Data-Sheet: https://cdn-shop.adafruit.com/datasheets/ESP8266_Specifications_English.pdf
- Arduino ESP8266 Core Documentation: https://arduino-esp8266.readthedocs.io/en/latest/

### Adafruit BME680

- Board: https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas/overview
- Original Bosch Library: https://github.com/BoschSensortec/BSEC-Arduino-library
- BME680 Data-Sheet: https://cdn-shop.adafruit.com/product-files/3660/BME680.pdf
  - measure humidity with ±3% accuracy
  - barometric pressure with ±1 hPa absolute accuracy
    - can be used to calculate the altimeter ±1 meter or better accuracy
  - temperature with ±1.0°C accuracy

- Connect via `STEMMA QT` with the Feather (see [STEMMA](#stemma-qt-cables-stemma))
  - SCK breakout pin to the I2C clock SCL pin on your Arduino compatible (yellow wire on STEMMA QT version)
  - SDI breakout pin to the I2C data SDA pin on your Arduino compatible (blue wire on STEMMA QT version)

- What configuration we like to run the sensor?
  - What power mode? (Page 21 DataSheet)
    - => "Ultra low power (ULP) mode that is designed for battery-powered and/or frequency-coupled devices over extended 
periods of time. This mode features an update rate of 300 seconds and an average current consumption of <0.1 mA"

- **Attention** to VOC / gas sensor calibration
  - https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas/overview
    > Please note, this sensor, like all VOC/gas sensors, has variability and to get precise measurements you will want to calibrate it against known sources!
    > That said, for general environmental sensors, it will give you a good idea of trends and comparisons.
    > We recommend that you run this sensor for 48 hours when you first receive it to "burn it in", and then 30 minutes in the desired mode every time the sensor is in use.
    > This is because the sensitivity levels of the sensor will change during early use and the resistance will slowly rise over time as the MOX warms up to its baseline reading.

### Servo SG92R

https://towerpro.com.tw/product/sg92r-7/

## Knowledge

### Adafruit STEMMA & STEMMA QT (I2C)

- Technical Specs: https://learn.adafruit.com/introducing-adafruit-stemma-qt/technical-specs

#### STEMMA QT cables
- Black for **GND**
- Red for **V+**
- Blue for **SDA**
- Yellow for **SCL**