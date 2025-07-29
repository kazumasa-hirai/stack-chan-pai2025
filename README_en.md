# stack-chan-tester

[日本語](README.md) | English

This is a program for Stack-chan using the M5Stack CoreS3. It performs face tracking using the built-in camera of the CoreS3.

# Supported kits
This program has been tested only with the Stack-chan assembled using the  [Stack-Chan M5GoBottom version kit](https://mongonta.booth.pm/) distributed by Takao Akaki on BOOTH, combined with an M5Stack CoreS3 and an SG90 servo.


# Adjustment of servo offset

PWM servos like the SG90 often have individual differences, so even when set to 90°, the actual angle may be slightly off. In such cases, please adjust the angle settings defined in the following library:

As a reference, the values used by the author of this program are:
- _servo[AXIS_X].start_degree = 80;
- _servo[AXIS_Y].start_degree = 105;
- _servo[AXIS_Y].lower_limit = 65;
- _servo[AXIS_Y].upper_limit = 105;
These settings can be found in:
- .pio/libdeps/m5stack-cores3/stackchan-arduino/src/Stackchan_system_config.cpp
Note: This library is added via PlatformIO. Be aware that updates to the library may overwrite your custom settings with default values.


# Usage
* Button A: Rotates the X-axis and Y-axis servos to 90°. Use this button to rotate the servo 90° before fixing.
* Button B: Faces the head to the initial position and starts face tracking. Pressing Button A during tracking will stop it.<br>Double-click to toggle Grove's 5V (ExtPower) output ON/OFF; turn OFF when checking power feed from behind Stack-chan_Takao_Base.
* Button C: Moves in random mode.
    * Button C: Stop random mode.
* Button A long press: Enter the mode to adjust and examine the offset.
    * Button A: Decrease offset.
    * Button B: toggles between X and Y axis
    * Button C: Increase offset
    * Button B long press: Out the mode to adjust. 

## Button handling in CoreS3
CoreS3 has changed the handling of buttons because the BtnA, B, and C parts of Core2 have been replaced by cameras and microphones. <br>
The screen is divided vertically into three parts: left: BtnA, center: BtnB, and right: BtnC.

# Requirement Libraries
※ If it does not work with the latest version, try matching the library version.
- [M5Unified](https://github.com/m5stack/M5Unified) v0.0.7
- [M5Stack-Avatar](https://github.com/meganetaaan/m5stack-avatar) v0.8.0<br> Prior to v0.7.4, M5Unified is not supported, so M5 double definition errors occur at build time.
- [ServoEasing](https://github.com/ArminJo/ServoEasing) v2.4.0
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) v0.11.0
- [esp32-camera](https://github.com/espressif/esp32-camera)


# How to build

This program has only been tested with PlatformIO.

# About stack chan
Stack chan is [meganetaaan](https://github.com/meganetaaan) is an open source project.

https://github.com/meganetaaan/stack-chan

# References
This program is based on the following projects:
- [stack-chan-tester](https://github.com/mongonta0716/stack-chan-tester)
- [M5CoreS3_FaceDetect](https://github.com/ronron-gh/M5CoreS3_FaceDetect)

# author
 Kazumasa Hirai

# LICENSE
 MIT
 LGPL
