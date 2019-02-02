# Lazy Grid Clock controller

This is a "fork", so to speak, of the controller for the Lazy Grid Clock model in Thingiverse (can be found [here](https://www.thingiverse.com/thing:3241802/)). The author wasn't interested in changing anything in his code so I uploaded my modified version here.

The author provides sample code for the clock, but it requires extra hardware. Mainly a DS3232 RTC, which I didn't have, and some extra buttons to set the time.

So I modified the code to run on an ESP8266. This can easily run on a tiny WeMos D1 off a cell phone charger, or you can roll your custom solution. It requires no extra hardware

## Main differences with the original code
The default version of the program won't "just run". There are three key differences:
* It doesn't run on Arduino Uno. This is pretty obvious. It needs an ESP8266-based Arduino
* The LED strip I used isn't a WS2812. I had  an APA102 strip, so that's what I used. The change is trivial, though. The line that says `FastLED.addLeds<APA102,D1,D2, BGR>(leds, LED_COUNT);` needs to be changed to the particular strip you're using.
* The model has several "invisible LEDs", which the author just tucked inside the model. Since I'm cheap, I removed these and bridged with some wires. If you come from the original firmware, you'd have to replace the `setPixel()` and `checkPixel()` functions with the original version.
* The auto brightness is all commented out. I didn't add an LDR yet so this is nonfunctional at the moment. 


## Required libraries
There are a few required libraries:
* FastLED, to control LEDs
* ArduinoOTA, to upgrade the firmware over WiFi, handy for customizing!
* TimeLib, Paul Stoffregen's version of it.
* WiFiManager, makes life SO much easier for setting WiFi credentials.

## Required parameters
Change `const int timeZone = -3;` to match your timezone. It defaults to Argentina.
Change `static const char ntpServerName[] = "pool.ntp.org";` to a NTP server near you.
Optionally you can change `setSyncInterval(86400);` to a higher number. Usually NTP servers ask to be requested once a day at most. If you notice your time is drifting too much, you can reduce this interval to update the time a little more often. Don't go too low. It's bad nettiquete and some servers may block you.

## Hooking it up
Connect your LED strips to the right pins, the ones you specified in the `FastLED.addLeds()` call.
**IMPORTANT:** The D1 and D2 lines used in the code aren't there by chance. The ESP8266 changes state quickly on some lines at boot (see [here](http://rabbithole.wwwdotorg.org/2017/03/28/esp8266-gpio.html)) and this makes the LED strip change colors rather quickly. In my case it set all leds to white and blew a fuse in my WeMos board... Anyways, D1 and D2 stay low on boot. You can further improve it with pulling these lines to GND with a couple resistors of, say, 1K.

## Starting it up
You'll need a smartphone or a laptop. Power it up and after a couple of seconds a new WiFi network will appear, with the name ESP-aabbcc, with aabbcc being an hex number. Connect to this network. It will ask for your WiFi credentials. Once you enter them, the ESP will connect to your network, request the time, and start displaying it.

## Hacking
Feel free to hack the code as much as you want, and submit any PR you think may be interesting.
