# ESP-FPV

Not the best, but interesting as proof-of-concept FPV system.
***

## Who's it for ?
For enthusiasts, hobbyists and DIY persons.
And for anyone, who think next statements are *ok* to deal:

#### Pros:
- Easy of use;
- Acceptable level of latency as 23±4ms. (Measured with 240fps slow-motion camera)
- No additional hardware is required (like cell phone)
- No PSRAM is required (i.e. it's possible to use simple bare ESP32 modules with enough pins)
- Primary QVGA resolution (320x240)
- Not a slide-show framerate with at least 16fps and up to 25fps! (*Even better with HQVGA or 240x176*)
- A few programmable GPIOs on ESP-CAM
- Dirty cheap! Could be done with duct tape, but not literally ;)
- Reusable parts (In case you don't want play with it anymore)
- And written on C

#### Cons:
- Not the best FPV image quality
- Short range distance (approx. ???m. in best case)
- No 5GHz support (for now?)
- High power consumption (ESP-CAM uses approx. 2.5W on 100% Tx power)
- Extreme heat of *Transmitter* at maximum Tx power (like +70C easy-peasy)


## How it's work ?

### Data flow
*Transmitter* always capture an OV2640 Jpeg images.
Split them to the small chunk to fit into ESP-NOW's 250 bytes of playload.
Add an extra proprietary protocol salt and a pinch of encryption.

*Receiver* all the time awaits for incoming packets.
All Video packets is assembled, decrypted, and passed to the Jpg decoder.
As Jpg decoder works at one core it provide decoded blocks to be drawn on Display by another core.

### Forced Frame Sync
Sometimes a few packets could be loss, in that case *Transmitter* awaits for ACK from
the *Receiver* every time when the whole Jpg image has been sent.
If this doesn't happen within special interval (approx. 100ms.), *Transmitter* will start to
send another Jpg image.

### Communication
Please note, by default Devices talk on Wi-Fi CH6, if it's overloaded with a lot AP,
you could change that by holding down BUTTON_1 when powering-on *Receiver*, 
this will start an automatic AirScanner mode.

AirScanner will try to peek most empty and silent channel.
Dot this if you want to get the best possible experience.

### How to start ?
Before using this FPV system both devices must be paired.
For the pairing you will need just a few wires.

#### Pairing
To do so, follow next steps:
  - Connect together Grounding Pins on both *Receiver* and *Transmitter*.
  - Connect together UART pins. *IO12* for *Transmitter* and *IO19* for *Receiver*.
  - First power-on ESP-CAM, then *Receiver*.
  - Wait until Video stream will appear on *Receiver* Screen.
  - Remove UART connection from *Receiver* and *Transmitter*. 
  - Connect UART pins on both devices to the Ground (like terminate pin).

If Video doesn't appear check all wirings, and check if you burned the right Firmware ;)

#### After Pairing
Once initial pairing is done, ensure what UART terminator/jumper is attached to the GND.

As before, first power-on ESP-CAM, then *Receiver*.
Once both devices got synced, Video stream should start automatically!


### Possible future plans
Once Espressif will announce dual-core (Not the ESP32-C5) controller with 5GHz support it would be an ultimate beast!
It's well know what 5GHz is way much better than 2.4GHz in FPV ;)

Move from ILI9341 TFT to a couple ST7789 IPS displays.


### Ok, i've got it, what's need ?
Nothing special is required!

#### Hardware:
- ESP-CAM with ESP32 as *Transmitter* (See Ai-Thinker)
- ESP32-S3-DevKitC or anything with ESP32-S3 onboard as *Receiver*
- ILI9341 TFT display or similar with resolution at least 320x240
- SSD1306 OLED with resolution 128x64
- External antenna for ESP32-S3 and/or ESP-CAM (Optional only for better range)
- At least one CP2102 or similar (to burn the FW)



#### Software:
- ESP-IDF not lower than v5.0
- VSCode with ESP-IDF plugin (Please, use Google-fu for howto setup it)


#### Steps:
- Import project (any of two) to VSCode
- Build the FW with: idf.py build
- Connect USB-UART 
- run: idf.py -p /dev/tty.usbserial-0001 flash
- Repeat for the second project
- ~Profit!~
- When everything is wired do the Pairing (see description above)


Main design and development for the *Receiver* is done for ESP32-S3.
An a ESP32 is also possible to use, but with lower resolution and/or framerate.

Please note, what Single-core ESP32/S2 is highly not recommended to use,
as the whole architecture has been designed ONLY for the dual-core controllers!
That's because Jpg decoding takes a LOT of horsepower and controller must be juicy and beefy!


#### Special notes
Thank's to @jeanlemotan and his improved for low latency ESP-CAM library!
Link to his project [here](https://github.com/jeanlemotan/esp32-cam-fpv. "esp32-cam-fpv by @jeanlemotan")


***
### Lists of known issues:
- *Receiver* sometimes crash (Tasks/Queue sync fail, ESP-NOW send fail, heap fail, LwIP fail)
- *Receiver* sometimes show ''glitchy'' Video for a few frames
- Restarting *Receiver* without restarting *Transmitter* will crash *Receiver*
- OSD stops refreshing, but everything else continues to work



## WARNING AND SAFETY NOTES :exclamation:
- Use ONLY external 3.3V power supply (in my case mini560 DC-DC with 2S LiPo)!
- By any costs AVOID on-board ESP-CAM's AMS1117 regulator or Camera will burn and unleash the ''magic smoke''!
- Place at least 10x10x10 heatsink on top of ESP-CAM module or it will burn!
- Use active cooling for ESP-CAM module when using Tx power higher than 30% and longer than a few minutes!
- Please, CHANGE ''DEFAULT_WIFI_COUNTRY_CODE'' in wireless_conf.h to match your country!
- Use it ONLY according to your's country and local laws (Do not use as a spy-cam or anything what goes against the law)!
- Do not use banned/illegal Wi-Fi Channels or absurd amount of Transmission power!
- Remember CH14 is allowed to use ONLY in Japan!
- Everything what you're doing is on your own risk!
