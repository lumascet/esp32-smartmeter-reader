# esp32-smartmeter-reader
Arduino sketch to read and decrypt the data from my Smart Meter (Landis+Gyr E450 and Iskra AM550 / Wiener Netze) with an ESP32. Integrates into Home Assistant via MQTT.

Tested with Iskra AM550.

You have to enable the "Kundenschnittstelle" and get the encryption key from https://smartmeter-web.wienernetze.at.

then edit the config.h to fit your setup. Connect the serial interface of the IR Adapter RX TO RX2 on the esp32.
