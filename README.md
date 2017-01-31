# ADSO (Arduino Digital Storage Oscilloscope)

## What is ADSO?
ADSO is a digital storage oscilloscope running on an arduino microcontroller. It samples, stores and analyses an electrical signal.

## How do I run ADSO?
Buy an arduino, some harware (OLED-display, keys, some resistors for a probe), solder all parts, install the arduino software (IDE  including appropriate libraries) on your PC and compile/upload "adso.ino" to your arduino.

## What does ADSO perform?
* Samples signals on one channel up to 500 Hz and 50V
* Shows signal graphically (80x64 pixel respective 10x8 divisions)
* Selectable scales (Volts per division, ms per division)
* 1:1 and 10:1 probe
* Triggering of periodic signals (selectable trigger level)
* Selectable x and y offset
* Reference signals: rectangular, PWD-output, 5V, 3.3V, GND
* Hold/Save/Load signal (permanent via EEPROM)
* Simple fourier transformation (frequency analysis)

## Where are the limits?
* Accuracy: As the signal is shown with a pixmap with 80x64 pixels - a difference of one pixel implies a deviation of more than 1.5%.
* Bandwidth: Due to limited sample/converting-performance in addition to do some calculations the arduino microprocessor can handle signals up to 500 Hz. Professional oscilloscopes (above $400) can sample up to 100 MHz - ADSO costs less than $20.
* Measures up to 50V input voltage - more at your own risk with another probe (i.e. 100:1 with 1MOhm-resistor to 10:1-input)

## Are there some pictures of ADSO?

## Is there a video that shows ADSO at work?

## Is there a circuit diagram and some manuals?
