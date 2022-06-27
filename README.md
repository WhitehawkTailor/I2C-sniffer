# I2C sniffer on ESP32
This code catches I2C communication by running on an ESP32 board that are conected to I2C BUS lines as a passive listener.

## Background story
The code was used to reverse engineer a tool that uses a I2C comunication between main unit and temperature measure board. The task was to catch the communication between the units.

## The setup
The board that runs the program is a ESP32 board, because it works on 240MHz. This allows to execute a few command in one cycle of an I2C data commonication, even the communication goes on 400kHz. Any slower board cannot work, because it cannot execute enough commands between comming bits.

* IDE: Any Arduino IDE - This was developed in Visual Studio Code + PlatformIO.
* Platform: Arduino for ESP32
* Board: Any ESP32 board can be used, just match the pins with the used in the code.
* Used pins: I2C uses two pins, one for clock and one for data. In the program the following pins are in use: GPIO12 for SDA, GPIO13 for SCL. This setup can be changed, but the hardware and the software setup should be in synch.

## Way of working
This is not connecting to the BUS as an I2C device. It is neither a Master, nor a Slave and never puts data on any line.
It just listens and logs the communication. The application does not interrupt the I2C communication.
 
Two pins of the ESP32 panel (GPIO12 for SDA, GPIO13 for SCL) as definet input in the code are attached to SDC and SDA lines of the observed unit.
Since the I2C communication usually operates on 400kHz so, the tool that runs this program should be fast.
This was tested on an ESP32 bord Heltec WiFi Lora32 v2. ESP32 core runs on 240MHz. It means there are about 600 ESP32 cycles during one I2C clock tick.
 
The code uses interrupts to detect the changes of SDA and SCL lines.
* The raise edge of the SCL - means bit transfer during the process
* The falling edge of SDA if SCL is HIGH - means START the process
* The raise edge of SDA if SCL id HIGH - means STOP the process
 
In the interrupt routines there are just a few lines of code that mainly sets the status and stores the incoming bits.
Otherwise the program gets timeout panic in interrupt handler and restarts the CPU.
In the main loop the program checks if there is anything in the incoming buffer and prints it out to the Serial console. 
Meaning of characters:
* S Start 
* s Stop 
* W Master will write
* R Master will read
* \+ ACK
* \- NACK

Output example: 
````sh
S1101101W+00110000+00001000+s
````

# Caution!!!
If you connect the pins directly to the I2C BUS in operation you may harm either the unit you observe or your ESP32 device (anything can go wrong, like mismatching lines, volt levels, inputs or outputs, etc.). Being on the safe side a simple solution can save your devices when you isolate your tool from the observed device. For this purpose I highly recommend the IC that I also use [ISO154x Low-Power Bidirectional I2C Isolators](https://www.ti.com/lit/ds/symlink/iso1540.pdf?ts=1603436321085&ref_url=https%253A%252F%252Fwww.google.de%252F). One side you connects the ground, power and I2C lines that you want to listen and to the other side you connects your independent ESP32 based board that runs the code with its own ground, power and I2C lines.

# How to use it
1. Download the main.cpp and open it in your IDE. If required then create a project for it and rename it. The code requires no additional includes. There is only the Arduino.h as an include. If you are working in pure Arduino ide, then this is not required, so delete or remark it.
2. Select your own ESP32 device in the IDE.
3. Check and change the SDC and SDA pins in the code according to your setup. Any GPIO can do that is free and is not in use with any embedded function of your board (#define PIN_SDA 12 and #define PIN_SCL 13)
4. Set your console speed according to the Serial speed in the code (115200baud)
5. Make sure that the hardware setup and the software setup is in synch, so GPIOs defined in the code are attached on the hardware properly.
6. Compile the code.
7. Download the code to the ESP32 device.
8. Connect your device to the device that you want to listen.
9. Power up your device and power up the device you want to observe.
10. Open the Serial consol to see the output of the ESP32 board that shows the I2C communication flow on the I2C BUS.
