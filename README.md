# Complete ESPHome Midea Climate Component

This directory contains the complete, fully functional ESPHome climate component that uses the exact MideaUART_v2 protocol implementation.

It should compile cleanly in esp-idf and arduino frameworks and has been tested with two functional AC units.
It is currently functional in ESP32-C6 devkitm (4M flash)

The only reason this was done is because the original mideauart and midea component is not compilable in esp-idf because of the arduino library that mideauart represents.
