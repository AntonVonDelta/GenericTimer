# GenericTimer

A simple timer for powering any device. Very useful for garden projects e.g. scheduling water system.

The microcontroller used is an attiny85 which is a perfect candidadte because it's very cheap and has enough power for this application. The code can be easily updated for a different one.

The arduino code also contains wrappers for the objects and constants specific to arduino libraries in order for it to compile in VC++. Using this method the code can be tested locally and compiled normally in C++.

The board requires a few components:
* Two 7-segment displays
* The corresponding resistors for powering the segments
* Two push-buttons for controlling and setting the time
* Two shift registers: 74hc595 for extending the pin capability of the attiny85
