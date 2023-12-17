# esplock
The firmware for the ESP32 implementation of the OpenLock Project

# Purpose
This esplock is responsible for connecting existing door lock systems with internet access so that it can be controlled by various applications of the users.

# Implementation
The first version of this hardware is connected in parallel to the release button switch of existing systems via the COM and GND. This way, we do not have to rebuilt any components of the system but allow the door lock to be controlled by the ESP. 

The firmware will send a request to the API Endpoint every 2 seconds and the server will respond a boolean value with 1 being 'unlock' and 0 being 'lock'. Whenever a user triggers the third party button to unlock, the server will return 1. Else, it should always be 0.

The ESP32 will send a HIGH signal to GPIO Pin 1 to unlock the door whenever the request response returns 1.

# Trade Off
All interface will be done via the web as there is no access from the ESP to the door system's controller. Hence, it is not possible for the ESP to programatically control or manage the PINs and Access Cards.
