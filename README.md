This is work in progress...

# Wall Controller

This is a Wall Controller based on the AZ-Touch module with ESP32.

TODO list:
* DONE - DHT Sensor interface
* DONE Air quality sensor interface
* RS485 interface
* RS485 commands
* GUI
* DONE WiFi OTA Updates

## Temperature and Relative Humidity

Using an DHT22 for this purpose. People usually used the AdaFruit sensor
library to handle it, but it looks that it won't work with the ESP32 RevC Kit
I have. Besides, the implementation looks not to be very efficient. I've
looked for other implementations - no luck. So I decided to spin my own
implementation, based on interrupts handling. No polling loops like in the Ada
Fruit library, so the CPU is more efficiently used. In the future this logic
will help for the low-power state.

