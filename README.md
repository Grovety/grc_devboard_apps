# Example applications

## VoiceRelay

A simple application demonstrating how a voice-controlled relay operates.

When the word “Robot” is spoken, the relay activates and sends a signal to a specific pin for 10 seconds. It can be stopped prematurely using the “Stop” command.

## Sound Events Detection

A simple application that demonstrates how to detect various sound events.

When a certain event is detected, the relay is activated and sends a signal to a certain pin.

### Supported sound events:

- baby crying
- glass breaking
- barking
- coughing

## Teacher (english)

A simple application that draws objects on the screen and waits for the user to pronounce them out loud. After several unsuccessful attempts, the board will give a hint and play a reference pronunciation.

### Objects:

- cat
- dog
- car
- house
- one
- two
- three

# Build instructions

ESP-IDF version: v5.2.1

## Supported devices

- GRC Devboard

## Configure the project

```bash
idf.py menuconfig
```

In the `App configuration` menu choose `Target device` and `Example application`. For `Sound Events Detection` application, additianaly select the type of sounds to detect.

### Build, Flash, and Run

Fetch submodules:

```
git submodule update --init
```

(or clone the repository with --recursive option)

Build the project and flash it to the board:

```
idf.py -p PORT build flash monitor
```

(Replace PORT with the name of the serial port to use)

