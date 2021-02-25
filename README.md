# RZSensors
Getting started with ESP32... Goal is to have a very flexible sketch to measure different values for our monitoring - while keeping it as simple and low-weight as possible. \
Currently measured:
* Temperature, Air Pressure and Humidity using BME280
* Light using BH1750
* CO2/eCO2 and TVOC zsubg CCS811
* Some debugging values of the ESP32: WiFi RSSI, Heap Size, Free Heap, Voltage of power supply

## Used Hardware
The Sketch is not limited to that hardware, but this is the hardware we're currently using. We'll also try to not vary that much with the sensors/breakout boards to have a reliable implementation (means: We're also using that links to re-order new sensors ;)).

| Name | Description | Links* |
| ---- | ----------- | ---- |
| ESP32 NodeMCU | ESP32 Dev Board from AZDelivery | [Amazon (pack of 3)](https://amzn.to/3dHtIr6)
| BH1750 | Light meter | [Amazon (pack of 3)](https://amzn.to/37MLqWh) |
| CCS811 | CO2/TVOC sensor for CO2 values >= 400 ppm | [Amazon](https://amzn.to/3pRPnzj) |
| BME280 | Temperature, Pressure and Humidity sensor | [Amazon (pack of 3)](https://amzn.to/3uyqmMS) |

\* Probably affiliate links, we'll get a little support and the price doesn't change for you. :) 