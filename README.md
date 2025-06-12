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

    #endif // SECRETS_H
    ```



### How to connect the electronics?

- [Setup 1](./doc/setup-1.jpg)
