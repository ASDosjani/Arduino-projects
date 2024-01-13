#include "OneButton.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <avr/sleep.h>

#define PIN_BUTTON PB2
#define PIN_STRIP PB0  // Digital IO pin connected to the NeoPixels.

#define PIXEL_COUNT 47  // Number of NeoPixels

#define DEFAULT_COLOR 255, 250, 215

#define INACTIVITY_TIMEOUT 5400000 // Automatic LED turning off after this in ms (1.5h)

// Setup a new OneButton on pin PIN_BUTTON
// The 2. parameter activeLOW is true, because external wiring sets the button to LOW when pressed.
OneButton button(PIN_BUTTON, true);

Adafruit_NeoPixel strip(PIXEL_COUNT, PIN_STRIP, NEO_GRB + NEO_KHZ800);

uint8_t lastmode = 2;
uint8_t pulsatingBrightness = 0;
unsigned long lastLedUpdate = 0, lastPressEEPROM = 0, lastPressDeepSleep = 0;

struct LEDConfiguration {
  uint8_t mode = 2;  // 0, 1 - disabled led
  uint8_t brightness = 20;
  uint8_t speed = 5;
  long colorHue = 0;
};

LEDConfiguration ledconf;

void setup() {

  button.attachClick(changeMode);
  button.attachDoubleClick(changeColor);
  button.attachMultiClick(multiclick);
  button.attachLongPressStart(turnOffLed);

  ADCSRA = 0;                           // ADC disabled
  GIMSK |= _BV(PCIE);                   // Enable Pin Change Interrupts
  PCMSK |= _BV(PIN_BUTTON);             // Use INT0 (PB2) as interrupt pin
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // replaces above statement

  LEDConfiguration conf;
  EEPROM.get(20, conf);

  if (conf.mode != 0xff && conf.mode > 1) {
    lastmode = conf.mode;
    ledconf.mode = 0;
  }
  if (conf.brightness >= 20) {
    ledconf.brightness = conf.brightness;
  }
  if (conf.speed != 0xff && conf.speed >= 5) {
    ledconf.speed = conf.speed;
  }
  if (conf.colorHue != 0xff) {
    ledconf.colorHue = conf.colorHue;
  }

  strip.begin();  // Initialize NeoPixel strip object (REQUIRED)
  strip.setBrightness(ledconf.brightness);
  strip.show();  // Initialize all pixels to 'off'
}

void system_sleep() {
  sleep_enable();  // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
  sei();           // Enable interrupts
  sleep_cpu();     // sleep

  cli();            // Disable interrupts
  sleep_disable();  // Clear SE bit

  sei();  // Enable interrupts
}

ISR(PCINT0_vect) {
  lastPressEEPROM = millis();
  lastPressDeepSleep = lastPressEEPROM;
}

void loop() {
  if (lastPressEEPROM != 0 && lastPressEEPROM + 5000 < millis()) {
    lastPressEEPROM = 0;
    EEPROM.put(20, ledconf);  //save config
  }

  if (lastPressDeepSleep + INACTIVITY_TIMEOUT < millis()) {
    lastPressDeepSleep = 0;
    turnOffLed();
  }

  switch (ledconf.mode) {
    case 0:
      ledconf.mode = 1;
      strip.clear();
      strip.show();
      system_sleep();
      break;
    case 2:
      staticColor();
      strip.show();
      break;
    case 3:
      scannerColor();
      strip.show();
      break;
    case 4:
      pulsatingColors();
      strip.show();
      break;
    case 5:
      pulsatingRainbow();
      strip.show();
      break;
    case 6:
      rainbow();
      strip.show();
      break;
  }
  button.tick();
}

void changeMode() {
  if (ledconf.mode == 1) {
    ledconf.mode = lastmode;
  } else {
    ledconf.mode++;
    if (ledconf.mode == 7) {
      ledconf.mode = 2;
    }
  }
  strip.clear();
}

void changeColor() {
  if (ledconf.colorHue == -1) ledconf.colorHue = 0;
  else {
    ledconf.colorHue += 8192;
    if (ledconf.colorHue == 65536) ledconf.colorHue = -1;
  }
}

void multiclick() {
  if (button.getNumberClicks() == 3) {
    if (ledconf.brightness == 20) {
      ledconf.brightness = 117;
    } else if (ledconf.brightness == 117) {
      ledconf.brightness = 255;
    } else if (ledconf.brightness == 255) {
      ledconf.brightness = 20;
    }
    strip.setBrightness(ledconf.brightness);
  } else if (button.getNumberClicks() == 4) {
    ledconf.speed += 10;
    if (ledconf.speed == 35) {
      ledconf.speed = 5;
    }
  }
}

void turnOffLed() {
  if (ledconf.mode > 1) lastmode = ledconf.mode;
  ledconf.mode = 0;
}

//----------------------Led Effects----------------------

void staticColor() {
  if (ledconf.colorHue == -1) {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, DEFAULT_COLOR);
    }
  } else {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(ledconf.colorHue)));
    }
  }
}

uint8_t index = 0;
bool right = true;

void scannerColor() {
  if (lastLedUpdate + ledconf.speed * 4 <= millis()) {
    lastLedUpdate = millis();

    if (right) {
      index++;
      if (index == strip.numPixels()) {
        right = false;
      }
    } else {
      index--;
      if (index == 0) {
        right = true;
      }
    }

    if (ledconf.colorHue == -1) {
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, DEFAULT_COLOR);
      }
      strip.setPixelColor(index - 2, 170, 170, 170);
      strip.setPixelColor(index - 1, 110, 110, 110);
      strip.setPixelColor(index - 2, 60, 60, 60);
      strip.setPixelColor(index + 1, 120, 120, 120);
      strip.setPixelColor(index + 2, 180, 180, 180);
    } else {
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(ledconf.colorHue)));
      }
      strip.setPixelColor(index - 2, strip.gamma32(strip.ColorHSV(ledconf.colorHue, 255, 170)));
      strip.setPixelColor(index - 1, strip.gamma32(strip.ColorHSV(ledconf.colorHue, 255, 110)));
      strip.setPixelColor(index, strip.gamma32(strip.ColorHSV(ledconf.colorHue, 255, 60)));
      strip.setPixelColor(index + 1, strip.gamma32(strip.ColorHSV(ledconf.colorHue, 255, 110)));
      strip.setPixelColor(index + 2, strip.gamma32(strip.ColorHSV(ledconf.colorHue, 255, 170)));
    }
  }
}

long pulsatingColorHue = 0;
bool rising = true;

void pulsatingColors() {
  if (lastLedUpdate + ledconf.speed * 2 <= millis()) {
    lastLedUpdate = millis();

    if (rising) {
      pulsatingBrightness += 5;
      if (pulsatingBrightness >= 255) {
        rising = false;
      }
    } else {
      pulsatingBrightness -= 5;
      if (pulsatingBrightness <= 0) {
        rising = true;

        if (pulsatingColorHue == -1) pulsatingColorHue = 0;
        else {
          pulsatingColorHue += 4096;
          if (pulsatingColorHue == 65536) pulsatingColorHue = -1;
        }
      }
    }

    if (ledconf.brightness == 20) {
      strip.setBrightness(pulsatingBrightness / 12);
    } else if (ledconf.brightness == 117) {
      strip.setBrightness(pulsatingBrightness / 2);
    } else {
      strip.setBrightness(pulsatingBrightness);
    }

    if (pulsatingColorHue == -1) {
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, DEFAULT_COLOR);
      }
    } else {
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pulsatingColorHue)));
      }
    }
  }
}

long pulsatingRainbowHue = 0;

void pulsatingRainbow() {
  if (lastLedUpdate + ledconf.speed <= millis()) {
    lastLedUpdate = millis();

    if (pulsatingRainbowHue < 65536) {
      pulsatingRainbowHue += 128;
    } else pulsatingRainbowHue = 0;

    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pulsatingRainbowHue)));
    }
  }
}

long rainbowHue = 0;

void rainbow() {
  if (lastLedUpdate + ledconf.speed <= millis()) {
    lastLedUpdate = millis();

    if (rainbowHue < 65536) {
      rainbowHue += 128;
    } else rainbowHue = 0;

    for (int i = 0; i < strip.numPixels(); i++) {
      int pixelHue = rainbowHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
  }
}