// Lazy Grid Clock - Software update v4
// https://www.thingiverse.com/thing:3241802
// 27/11/2018 - Daniel Cikic
#define FASTLED_ALLOW_INTERRUPTS 0
#include <TimeLib.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
#include <FastLED.h>
#include <EEPROM.h>

#define RESX 7                        // 7 columns
#define RESY 11                       // 11 rows
#define DIGITX 3                      // width of digits (3 pixels)
#define DIGITY 5                      // height of digits (5 pixels)
#define LED_COUNT (RESX * RESY)    // total number of leds

CRGB leds[LED_COUNT];

CRGBPalette16 currentPalette;

//int buttonA = D5;                          // 6x6 switch, 1 pin to gnd, 1 pin to 3
//int buttonB = D6;                          // 6x6 switch, 1 pin to gnd, 1 pin to 4

byte brightnessMin = 160;                 // brightness levels for min/medium/max
byte brightnessMed = 190;                 // don't change all three values and keep the one that's stored to EEPROM! (value gets stored to EEPROM, not index to brightness level)
byte brightnessMax = 240;                 // do not set brightnessMax above 254!
byte brightness = brightnessMed;          // brightness always at medium value when powering on/resetting (or not yet stored to EEPROM)
byte brightnessAuto = 0;                  // 1 = enable brightness corrections using a photo resistor/readLDR();
//int pinLDR = D8;                           // LDR connected to A1
byte intervalLDR = 50;                    // read value from LDR every 50ms
unsigned long valueLDRLastRead = 0;       // time when we did the last readout
byte avgLDR = 0;                          // we will average this value somehow somewhere in readLDR();
byte lastAvgLDR = 0;

byte colorMode = 0;                       // colorMode 0 uses HSV, 1 uses palettes
byte selectedPalette = 0;                 // palette to use when starting up in colorMode 1
byte saturation = 255;                    // saturation in hsv mode, 0 = no saturation, white
byte startHue = 0;                        // hsv color 0 = red (also used as "index" for the palette color when colorMode = 1)
byte displayMode = 1;                     // 0 = 12h, 1 = 24h (will be saved to EEPROM once set using buttons)
byte overlayMode = 1;                     // switch on/off (1/0) to use void colorOverlay(); (will be saved to EEPROM once set using buttons)
int overlayInterval = 500;                // interval (ms) to change colors in overlayMode

byte colorOffset = 32;                    // default distance between colors on the color wheel/palette used between segments/leds
int colorChangeInterval = 2500;           // interval (ms) to change colors when not in overlayMode

int paletteChangeInterval =  0;           // interval (minutes, 0 = disable) to select a random palette using randomPalette();
unsigned long paletteLastChange = 0;      // this var will store the time when the last change has been done

byte btnRepeatCounter = 1;
byte lastKeyPressed = 0;
int btnRepeatDelay = 150;
unsigned long btnRepeatStart = 0;

byte lastSecond = 0;
unsigned long lastLoop = 0;
unsigned long lastColorChange = 0;
unsigned long lastButtonPress = 0;


static const char ntpServerName[] = "pool.ntp.org";

const int timeZone = -3;     // Central European Time
/* these values will be stored to the EEPROM when set using the buttons:
  0 = selectedPalette
  1 = selectedBrightness
  2 = displayMode
  3 = overlayMode
*/

// let's define 0-9 to be displayed in a 3x5 grid (digitX & digitY)
// we'll start with 0 because that way our index to the array will correspond to the digit (0 = 0, 1 = 1...)
// "0" has line breaks to see how 1/0 are used to turn on/off pixels

byte digits[10][DIGITX * DIGITY] = {
  { 1, 1, 1,
    1, 0, 1,
    1, 0, 1,
    1, 0, 1,
    1, 1, 1
  },                                            // 0
  { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },        // 1
  { 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1 },        // 2
  { 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1 },        // 3
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1 },        // 4
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // 5
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // 6
  { 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },        // 7
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // 8
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 }         // 9
};



void setup() {


  WiFiManager wifiManager;
  wifiManager.autoConnect();
  Udp.begin(localPort);

  //  if (brightnessAuto == 1) pinMode(pinLDR, OUTPUT);
  //  pinMode(buttonA, INPUT_PULLUP);
  //  pinMode(buttonB, INPUT_PULLUP);
  Serial.begin(57600);
  Serial.println("Lazy Grid Clock v4 starting up...");
  delay(250);
  //  Wire.begin();
  setSyncProvider(getNtpTime);
  setSyncInterval(86400);

//  loadValuesFromEEPROM();
//
//
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  FastLED.addLeds<APA102,D1,D2, BGR>(leds, LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);  // Limit power usage to 5v/500mA
  FastLED.setBrightness(brightness);
  FastLED.clear();
  FastLED.show();
 

}

void loop() {
//  for (byte i = 0; i <= 9; i++) {
//    showDigit(i, random(2, 7), random(4, 11), 20 + i * 8);
//    FastLED.show();
//    delay(350);
//    FastLED.clear();
//  }
  if (  (lastLoop - lastColorChange >= colorChangeInterval) && (overlayMode == 0)
        || (lastLoop - lastColorChange >= overlayInterval) && (overlayMode == 1)    ) {        // update display if colorChangeInterval or overlayInterval has been reached
    startHue++;
    updateDisplay(startHue, colorOffset);
    lastColorChange = millis();
  }

  if (lastSecond != second()) {                                                             // update display if current second is different from last second drawn
    updateDisplay(startHue, colorOffset);
    lastSecond = second();
  }

  //  if (lastKeyPressed == 1) {
  //    if (brightness == brightnessMin) brightness = brightnessMed;                            // switch between brightness levels (color spacing effect when colorMode = 0 / HSV)
  //    else if (brightness == brightnessMed) brightness = brightnessMax;
  //    else if (brightness == brightnessMax) brightness = brightnessMin;
  //    updateDisplay(startHue, colorOffset);
  //    if (btnRepeatCounter >= 20) {                                                           // if button is held for a few seconds change overlayMode 0/1 (per segment, per led using void colorOverlay();
  //      if (overlayMode == 0) overlayMode = 1; else overlayMode = 0;
  //      updateDisplay(startHue, colorOffset);
  //      EEPROM.write(3, overlayMode);
  //      btnRepeatStart = millis();
  //    }
  //    EEPROM.write(1, brightness);                                                            // write brightness to EEPROM only when set using the buttons and this loop
  //  }

  //  if (lastKeyPressed == 2) {                                                                // switch between color palettes
  //    if (selectedPalette <= 3) selectedPalette += 1; else selectedPalette = 0;
  //    selectPalette(selectedPalette);
  //    updateDisplay(startHue, colorOffset);
  //    if (btnRepeatCounter >= 20) {                                                           // if button is held for a few seconds change displayMode 0/1 (12h/24h)
  //      if (displayMode == 0) displayMode = 1; else displayMode = 0;
  //      updateDisplay(startHue, colorOffset);
  //      EEPROM.write(2, displayMode);
  //      btnRepeatStart = millis();
  //    }
  //    EEPROM.write(0, selectedPalette);                                                       // write selectedPalette to EEPROM only when set using the buttons and this loop
  //  }

  if ( ( (lastLoop - paletteLastChange) / 1000 / 60 >= paletteChangeInterval) && (paletteChangeInterval > 0) ) {
    randomPalette(3);                                                                       // only interested in random 0-3 because 4 is single colored and boring ;)
    paletteLastChange = millis();
  }

  //  if ( (lastLoop - valueLDRLastRead >= intervalLDR) && (brightnessAuto == 1) ) {            // if LDR enabled and sample interval has been reached...
  //    readLDR();                                                                              // ...call readLDR();
  //    if (avgLDR != lastAvgLDR) {                                                             // only adjust current brightness if avgLDR (sample over 5 values of LDR readout) has changed.
  //      updateDisplay(startHue, colorOffset);                                                 // When using this for the first time/connnecting a new LDR I recommend commenting this out and do a
  //      // Serial.println(avgLDR); to adjust values in readLDR();
  //      lastAvgLDR = avgLDR;
  //    }
  //    valueLDRLastRead = millis();
  //  }
  //  if (lastKeyPressed == 12) setupClock();                                                   // enter setup
  //  lastKeyPressed = readButtons();
  lastLoop = millis();

  ArduinoOTA.handle();


}

//void readLDR() {                                                                                                  // read LDR value and add to tmp 5 times. The average is then assigned to avgLDR for use in updateDisplay();
//  int readOut = 0;
//  static byte runCounter = 1;
//  static int tmp = 0;
//  readOut = map(analogRead(pinLDR), 0, 1023, 220, 0);
//  /* analog readout from 0-1023 mapped to 220-0. From 220-0 instead of 0-255 because
//    we want the value to increase when it gets darker - this can easily be substracted from
//    the current brightness.
//    The LDR I've used gave me an avgLDR of about 40-0 at medium ambient room lighting and
//    went down to ~150-200 very quickly when blocking light from it. You may need to adjust this.
//  */
//  tmp += readOut;
//  if (runCounter == 5) {
//    avgLDR = tmp / 5;
//    tmp = 0; runCounter = 0;
//    //Serial.print("average LDR value: "); Serial.println(avgLDR);                                                // uncomment this line and watch your avgLDR. We want a value between ~20 at room lighting and ~200 in the dark
//  }
//  runCounter++;
//}

void randomPalette(byte palette) {
  byte newPalette = selectedPalette;                                                                              // set newPalette to the currently selected one...
  while (newPalette == selectedPalette) newPalette = random(palette);                                             // ...and randomize until we have a new one, so we don't get the same palettes twice in a row
  selectPalette(newPalette);
}

void colorOverlay(byte color) {                                                                                   // example of how to "project" colors on already drawn time/pixels before showing leds in updateDisplay();
  for (byte y = 0; y < RESY; y++) {                                                                               // Check each row...
    for (byte x = 0; x < RESX; x++) {                                                                             // ...and each column...
      if (checkPixel(x, y)) setPixel(x, y, color + y * 12);                                                       // and if current pixel is lit, draw it again in another color.
    }
  }
}

void updateDisplay(byte color, byte colorSpacing) {                                                               // this is what redraws the "screen"
  FastLED.clear();                                                                                                // clear whatever the leds might have assigned currently...
  displayTime(color, colorSpacing);                                                                               // ...set leds to display the time...
  if (overlayMode == 1) colorOverlay(color);                                                                      // ...and if using overlayMode = 1 draw custom colors over single leds
  if (brightnessAuto == 1) {                                                                                      // If brightness is adjusted automatically by using readLDR()...
    if (brightness - avgLDR >= 20) {                                                                            // check if resulting brightness stays above 20...
      FastLED.setBrightness(brightness - avgLDR);                                                               // ...and set brightness to current value - avgLDR
    } else {
      FastLED.setBrightness(20);                                                                                // or set to 20 (make sure to not go < 1)
    }
  } else {                                                                                                        // If not using LDR/auto just...
    FastLED.setBrightness(brightness);                                                                            // ...assign currently selected brightness...
  }
  FastLED.show();                                                                                                 // ...and finally make the leds light up/change visibly.
}

//byte readButtons() {
//  byte activeButton = 0;
//  byte retVal = 0;
//  if ( digitalRead(buttonA) == 0 || digitalRead(buttonB) == 0) {
//    if (digitalRead(buttonA) == 0) activeButton = 1;
//    else if (digitalRead(buttonB) == 0) activeButton = 2;
//    if ( digitalRead(buttonA) == 0 && digitalRead(buttonB) == 0 ) activeButton = 12;
//    if (millis() - lastButtonPress >= btnRepeatDelay) {
//      btnRepeatStart = millis();
//      btnRepeatCounter = 0;
//      retVal = activeButton;
//    } else if (millis() - btnRepeatStart >= btnRepeatDelay * (btnRepeatCounter + 1) ) {
//      btnRepeatCounter++;
//      if (btnRepeatCounter > 5) retVal = activeButton;
//    }
//    lastButtonPress = millis();
//  }
//  return retVal;
//}

//void setupClock() {
//  byte prevColorMode = colorMode;               // store current colorMode and switch back after setup
//  colorMode = 0;                                // switch to hsv mode for setup
//  brightness = brightnessMax;                   // switch to maximum brightness
//  tmElements_t setupTime;
//  setupTime.Hour = 12;
//  setupTime.Minute = 0;
//  setupTime.Second = 0;
//  setupTime.Day = 27;
//  setupTime.Month = 11;
//  setupTime.Year = 2018 - 1970;                 // yes... .Year is years since 1970.
//  byte setting = 1;
//  byte blinkStep = 0;
//  int blinkInterval = 500;
//  unsigned long lastBlink = millis();
//  Serial.println("Entering setup mode...");
//  FastLED.clear();
//  FastLED.show();
////  while (digitalRead(buttonA) == 0 || digitalRead(buttonB) == 0) delay(20);     // this will keep the display blank while any of the keys is still pressed
//  while (setting <= 2) {
//    while (setting == 1) {                                                      // hour setup loop
//      if ( lastKeyPressed == 1 ) setting += 1;                                  // set and display hour in green and continue to minutes
//      if ( lastKeyPressed == 2 ) {
//        if (setupTime.Hour < 23) setupTime.Hour += 1; else setupTime.Hour = 0;  // increase hour when buttonB is pressed
//      }
//      if (millis() - lastBlink >= blinkInterval) {                              // pretty sure there is a much easier and nicer way
//        if (blinkStep == 0) {                                                   // to get the switch between min and max brightness (boolean?)...
//          brightness = brightnessMax;
//          blinkStep = 1;
//        } else {
//          brightness = brightnessMin;
//          blinkStep = 0;
//        }
//        lastBlink = millis();
//      }
//      drawSetupTime(setupTime.Hour, setupTime.Minute, 48, 0);
//      lastKeyPressed = readButtons();
//    }
////    while (digitalRead(buttonA) == 0 || digitalRead(buttonB) == 0) delay(20);
//    while (setting == 2) {                                                      // minute setup loop
//      if ( lastKeyPressed == 1 ) {                                              // set and display hour and minutes in green and end setup
//        //        RTC.write(setupTime);                                                   // store new time to rtc
//        setting += 1;                                                           // end setup
//      }
//      if ( lastKeyPressed == 2 ) {  // increase minute when buttonB is pressed
//        if (setupTime.Minute < 59) setupTime.Minute += 1; else setupTime.Minute = 0;
//      }
//      if (millis() - lastBlink >= blinkInterval) {
//        if (blinkStep == 0) {
//          brightness = brightnessMax;
//          blinkStep = 1;
//        } else {
//          brightness = brightnessMin;
//          blinkStep = 0;
//        }
//        lastBlink = millis();
//      }
//      lastKeyPressed = readButtons();
//      drawSetupTime(setupTime.Hour, setupTime.Minute, 96, 48);
//    }
//  }
//  Serial.println("Setup done...");
//  drawSetupTime(setupTime.Hour, setupTime.Minute, 96, 96);
//  time_t sync = now();                                                         // create variable sync to synchronize arduino clock to rtc
////  while (digitalRead(buttonA) == 0 || digitalRead(buttonB) == 0) delay(20);
//  colorMode = prevColorMode;
//}
//
//void drawSetupTime(byte setupHour, byte setupMinute, byte hourColor, byte minuteColor) {
//  // Ugly routine and basically a copy of updateDisplay only used while in setup. Should be remade using some kind of time_t structure
//  // and merged to updateDisplay somehow.
//  FastLED.clear();
//  if (displayMode == 0) {
//    if (setupHour == 0) setupHour = 12;
//    if (setupHour >= 13) setupHour -= 12;
//    if (setupHour >= 10) showDigit(1, 2, 10, hourColor);
//    showDigit((setupHour % 10), 6, 10, hourColor);
//  } else if (displayMode == 1) {
//    showDigit(setupHour / 10, 2, 10, hourColor);
//    showDigit(setupHour % 10, 6, 10, hourColor);
//  }
//  showDigit((setupMinute / 10), 2, 4, minuteColor);
//  showDigit((setupMinute % 10), 6, 4, minuteColor);
//  FastLED.show();
//}

void displayTime(byte color, byte colorSpacing) {

  time_t t = now();                                                   // store current time in variable t to prevent changes while updating

  if (displayMode == 0) {
    if (hourFormat12(t) >= 10) showDigit(1, 2, 10, color + colorSpacing * 2);
    showDigit((hourFormat12(t) % 10), 6, 10, color + colorSpacing * 3);
  } else if (displayMode == 1) {
    showDigit(hour(t) / 10, 2, 10, color + colorSpacing * 2);
    showDigit(hour(t) % 10, 6, 10, color + colorSpacing * 3);
  }
  showDigit((minute(t) / 10), 2, 4, color + colorSpacing * 4);
  showDigit((minute(t) % 10), 6, 4, color + colorSpacing * 5);
}

void selectPalette(byte colorPalette) {                     // for a simple custom color have a look at case 4
  switch (colorPalette) {
    case 0:
      currentPalette = CRGBPalette16(                       // 0 - 255, intensity for (R)ed, (G)reen, (B)lue
                         CRGB(224,   0,   0),
                         CRGB(  0,   0, 244),
                         CRGB(128,   0, 128),
                         CRGB(224,   0,  64) );
      colorOffset = 48;
      break;
    case 1:
      currentPalette = CRGBPalette16(
                         CRGB(224,   0,   0),
                         CRGB(192,  64,   0),
                         CRGB(128, 128,   0),
                         CRGB(224,  32,   0) );
      colorOffset = 64;
      break;
    case 2:
      fill_rainbow(currentPalette, 16, 0, 255 / 16);        // create a simple rainbow palette, could also be done using rainbow_p, see FastLED documentation for instructions
      colorOffset = 16;
      break;
    case 3:
      currentPalette = CRGBPalette16(
                         CRGB(  0, 224,   0),
                         CRGB(  0, 192,  64),
                         CRGB(  0, 160, 192),
                         CRGB(  0, 192,  64) );
      colorOffset = 32;
      break;
    case 4:
      fill_solid(currentPalette, 16, CRGB::CornflowerBlue);  // This palette will only have a single color, CornflowerBlue (HTMLColorCode) in this case
      colorOffset = 1;                                       // in overlayMode this will be the mode with the lowest color spread between leds
      break;
  }
  selectedPalette = colorPalette;
}

void showDigit(byte digit, byte x, byte y, byte color) {
  for (byte i = 0; i < 15; i++) {
    if (digits[digit][i] == 1) setPixel(x + (i - ((i / DIGITX) * DIGITX)) - 2, y - (i / DIGITX), color);
  }
}

void setPixel(byte x, byte y, byte color) {
  byte pixel = 0;

  if (x < RESX && y < RESY) {
    if ((x % 2) == 0)
      pixel = ((x) * RESY) + y;
    else
      pixel = ((x) * RESY) + (RESY - 1 - y);
    if (colorMode == 1)
      leds[pixel] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
    else
      leds[pixel].setHSV(color, saturation, brightness);
  }
}

bool checkPixel(byte x, byte y) {
  byte pixel = 0;
  if (x < RESX && y < RESY) {
    if ((x % 2) == 0)
      pixel = ((x) * RESY) + y;
    else
      pixel = ((x) * RESY) + (RESY - 1 - y);
  }
  if (leds[pixel]) return true; else return false;
}

void loadValuesFromEEPROM() {
  byte tmp = 0;
  tmp = EEPROM.read(0);
  if (tmp == 255) selectPalette(0); else selectPalette(tmp);  // Unwritten values return 255. So we know it's never been written and saved to EEPROM, so let's choose palette 0. Also...
  tmp = EEPROM.read(1);
  if (tmp != 255) brightness = tmp;                           // ...this is the reason why you should know exactly what you're doing when setting brightnessMax = 255!
  tmp = EEPROM.read(2);
  if (tmp != 255) displayMode = tmp;
  tmp = EEPROM.read(3);
  if (tmp != 255) overlayMode = tmp;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();

  while (millis() - beginWait < 1500) {
    delay(1);
    yield();

    int recvsz = Udp.parsePacket();

    if (recvsz >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }

  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
