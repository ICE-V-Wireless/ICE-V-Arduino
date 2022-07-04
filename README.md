# ICE-V-Arduino
Arduino-based ESP32 firmware for the ICE-V Wireless board

## Abstract
This project is a version of the ICE-V Wireless firmware ported to the Ardunio
ESP32 environment. It includes the well-known WiFiManager SoftAP captive portal
to do the initial SSID/Password setup as well as an option to restart the setup
at any time by pressing the BOOT button for three secones. It supports power-on
initialization of the ICE40UP5k FPGA from a stored default as well as wireless
programming of the FPGA, SPI access after configuration, PSRAM read/write and
battery voltage monitoring.

## Prerequisites
You'll need the following resources:
* Arduino IDE. Get it here: https://www.arduino.cc/en/software
* ESP32 for Arduino. Get it here: https://github.com/espressif/arduino-esp32
* WiFiManager library. Get it here: https://github.com/tzapu/WiFiManager

Go to each of the above sites and follow the directions for a normal
installation. Once that's complete you should be able to build this project.

## Building
Open up the ICE-V.ino project in the Arduino IDE.

Prior to building you need to do the board configuration in the Arduino IDE.
Navigate to the "Tools" menu and set the following options:
* Board: ESP32C3 Dev Module
* Upload Speed: (anything - the USB port max speed will be used)
* USB CDC On Boot: Enabled
* CPU Frequency: 160MHz (WiFi)
* Flash Frequency: 80MHz
* Flash Mode: QIO
* Flash Size: 4MB
* Partition Scheme: Default 4MB with spiffs
* Core Debug: Verbose
* Port: (see below)

Connect the ICE-V board to your build host by USB and drag down to the
"Tools->Port" menu item - drag right and select the port your device has
connected to.

Now open up the "Tools->Serial Monitor" so you can see the log messages when
you firmware boots up.

Click the (->) Upload button to compile and flash the project to your hardware.
This may take some time and you will see progress messages scrolling by in the
IDE status window. Once the board boots up new firmware you should see progress
in the Serial Monitor window telling you what's going on. On the first boot
there will be a slight delay while the SPIFFS file system is initialized and
then the WiFiManager SoftAP should start up.

## Provisioning
Use any device with a web browser and Wifi connection to configure the WiFi
credentials. Open up your WiFi network browser and search for a network named
`ICE-V_AP`. Connect to that network and provide the password `ice-vpwd`.

After the captive portal webpage appears you will see several buttons - press
the "Configure WiFi" button and a list of available networks will appear.
Select your network and provide the password. The captive portal will shut down
and the ICE-V board will join your network.

Note: after the SoftAP/Captive Portal shuts down your ICE-V board will
have a name of "esp32c3-XXXXXX.local". On subsequent boots with good network
credentials it will be named "ice-v_XXXXXX.local" so once your board is able
to access your network you may want to touch reset to restart with a more
normal network name.

### Resetting Credentials
If you want to clear out the network credentials for any reason you can do so
by pressing the BOOT button on the ICE-V board for approximately 3 seconds. This
will wipe the existing network SSID and password and force a restart into the
SoftAP/Captive Portal to re-enter new credentials.

## Default FPGA configuration
After installing the firmware for the first time, the SPIFFS filesystem will be
empty and the FPGA will not be configured on power-up. If you want to have a
default FPGA configuration at power-up then you need to use the host-side
python script `send-c3sock.py` to send the desired configuration to the ICE-V
board. That python script is available in the baseline ICE-V firmware repository
here:

https://github.com/ICE-V-Wireless/ICE-V-Wireless/tree/main/python




