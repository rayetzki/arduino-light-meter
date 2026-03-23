#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include "battery.h"
#include "light_meter.h"

#define SCREEN_WIDTH                128
#define SCREEN_HEIGHT               64

#define OLED_RESET                  -1
#define SCREEN_ADDRESS              0x3C

#define MeteringButtonPin           11
#define PlusButtonPin               10
#define MinusButtonPin              8
#define MenuButtonPin               7
#define MeteringModeButtonPin       6
#define ModeButtonPin               2

#define DefaultApertureIndex        12
#define DefaultISOIndex             11
#define DefaultModeIndex            0
#define DefaultShutterSpeedIndex    40

#define MaxISOIndex                 57
#define MaxApertureIndex            70
#define MaxShutterSpeedIndex        80
#define MaxNDIndex                  13
#define MaxFlashMeteringTime        5000

#define BatteryInterval             10000

// EEPROM memory cells
#define ISOIndexAddr                1
#define ApertureIndexAddr           2
#define ShutterSpeedIndexAddr       3
#define ModeIndexAddr               4
#define MeteringModeIndexAddr       5

int ModeButtonState = HIGH;
int MeteringButtonState = HIGH;
int MeteringModeButtonState = HIGH;
int MenuButtonState = HIGH;
int PlusButtonState = HIGH;
int MinusButtonState = HIGH;

double lastBatteryTime = 0;
int vcc;
float lux;
long iso;
float aperture;
float shutterSpeed;
float EV;

uint8_t ISOIndex =          EEPROM.read(ISOIndexAddr);
uint8_t apertureIndex =     EEPROM.read(ApertureIndexAddr);
uint8_t shutterSpeedIndex = EEPROM.read(ShutterSpeedIndexAddr);
uint8_t modeIndex =         EEPROM.read(ModeIndexAddr);
uint8_t meteringMode =      EEPROM.read(MeteringModeIndexAddr);

bool isMainScreen = true;
bool isISOView = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void updateCurrentSettings() {
  EV = luxToEV(lux);
  iso = getISOByIndex(ISOIndex, MaxISOIndex);
  aperture = getApertureByIndex(apertureIndex, MaxApertureIndex);
  shutterSpeed = getShutterSpeedByIndex(shutterSpeedIndex, MaxShutterSpeedIndex);
}

void renderMainScreen() {
  isMainScreen = true;
  isISOView = false;

  display.clearDisplay();
  display.setTextColor(WHITE);
  // battery indicator
  display.drawRect(122, 1, 6, 8, WHITE);
  display.drawLine(124, 0, 125, 0, WHITE);

  // battery indicator for 4 elements of 1.5v each.
  if (vcc > 540) {
    // full
    display.fillRect(123, 1, 4, 7, WHITE);
  } else if (vcc > 480) {
    // medium
    display.fillRect(123, 4, 4, 5, WHITE);
  } else if (vcc > 420) {
    // minimum
    display.fillRect(123, 6, 4, 3, WHITE);
  }

  // show iso
  display.setTextSize(1);
  display.setCursor(13, 1);
  display.print(F("ISO:"));

  if (iso > 999999) {
    display.print(iso / 1000000.0, 2);
    display.print(F("M"));
  } else if (iso > 9999) {
    display.print(iso / 1000.0, 0);
    display.print(F("K"));
  } else {
    display.print(iso);
  }

  display.drawLine(0, 10, 128, 10, WHITE); // LINE DIVISOR

  float A = aperture;
  float T = shutterSpeed;

  if (lux > 0) {
    if (modeIndex == 0) {
      // Aperture priority
      T = formatShutterSpeed(100 * pow(A, 2) / iso / pow(2, EV), MaxShutterSpeedIndex);
    } else if (modeIndex == 1) {
      // Shutter speed priority
      A = formatAperture(sqrt(pow(2, EV) * iso * T / 100), MaxApertureIndex);
    }
  } else {
    if (modeIndex == 0) {
      T = 0;
    } else {
      A = 0;
    }
  }

  // show aperture
  display.setCursor(10, 16);
  display.setTextSize(2);
  display.print(F("f/"));
  
  if (A > 0) {
    if (A >= 100) {
      display.print(A, 0);
    } else {
      display.print(A, 1);
    }
  } else {
    display.println(F("--"));
  }

  display.setTextSize(1);
  display.setCursor(90, 16);
  display.print(F("EV:"));

  if (lux > 0) {
    display.println(EV, 0);
  } else {
    display.println(0, 0);
  }

  // show shutter speed
  uint8_t Tdisplay = 0; // Flag for shutter speed display style (fractional, seconds, minutes)
  double  Tfr = 0;
  float   Tmin = 0;

  if (T >= 60) {
    Tdisplay = 0;  // Exposure is in minutes
    Tmin = T / 60;
  } else if (T < 60 && T >= 0.5) {
    Tdisplay = 2;  // Exposure in in seconds
  } else if (T < 0.5) {
    Tdisplay = 1;  // Exposure is in fractional form
    Tfr = round(1 / T);
  }

  display.setTextSize(2);
  display.setCursor(10, 37);
  display.print(F("T:"));

  if (Tdisplay == 0) {
    display.print(Tmin, 1);
    display.print(F("m"));
  } else if (Tdisplay == 1) {
    if (T > 0) {
      display.print(F("1/"));
      display.print(Tfr, 0);
    } else {
      display.println(F("--"));
    }
  } else if (Tdisplay == 2) {
    display.print(T, 1);
    display.print(F("s"));
  } else if (Tdisplay == 3) {
    display.println(F("--"));
  }

  // Metering mode icon
  display.setTextSize(1);
  display.setCursor(0, 1);
  if (meteringMode == 0) {
    // Ambient light
    display.print(F("A"));
  } else if (meteringMode == 1) {
    // Flash light
    display.print(F("F"));
  }
  // End of metering mode icon

  // priority marker (shutter or aperture priority indicator)
  display.setTextSize(1);
  if (modeIndex == 0) {
    display.setCursor(0, 21);
  } else {
    display.setCursor(0, 42);
  }
  display.print(F("*"));

  display.display();
}

void readButtons() {
  ModeButtonState = digitalRead(ModeButtonPin);
  MeteringButtonState = digitalRead(MeteringButtonPin);
  MenuButtonState = digitalRead(MenuButtonPin);
  MeteringModeButtonState = digitalRead(MeteringModeButtonPin);
  PlusButtonState = digitalRead(PlusButtonPin);
  MinusButtonState = digitalRead(MinusButtonPin);
}

void renderISOView() {
  isISOView = true;
  isMainScreen = false;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(50, 1);
  display.println(F("ISO"));
  display.setTextSize(3);

  long iso = getISOByIndex(ISOIndex, MaxISOIndex);

  if (iso > 999999) {
    display.setCursor(0, 40);
  } else if (iso > 99999) {
    display.setCursor(10, 40);
  } else if (iso > 9999) {
    display.setCursor(20, 40);
  } else if (iso > 999) {
    display.setCursor(30, 40);
  } else if (iso > 99) {
    display.setCursor(40, 40);
  } else {
    display.setCursor(50, 40);
  }

  display.print(iso);

  display.display();
}

void render() {
  if (isMainScreen) {
    if (MenuButtonState == LOW) {
      renderISOView();
    }
    
    if (PlusButtonState == LOW) {
      if (modeIndex == 0) {
        apertureIndex++;

        if (apertureIndex > MaxApertureIndex) {
          apertureIndex = 0;
        }
      } else if (modeIndex == 1) {
        shutterSpeedIndex++;

        if (shutterSpeedIndex > MaxShutterSpeedIndex) {
          shutterSpeedIndex = 0;
        }
      }

      updateCurrentSettings();
      renderMainScreen();
    }

    if (MinusButtonState == LOW) {
      if (modeIndex == 0) {
        if (apertureIndex > 0) {
          apertureIndex--;
        } else {
          apertureIndex = MaxApertureIndex;
        }
      } else if (modeIndex == 1) {
        if (shutterSpeedIndex > 0) {
          shutterSpeedIndex--;
        } else {
          shutterSpeedIndex = MaxShutterSpeedIndex;
        }
      }

      updateCurrentSettings();
      renderMainScreen();
    }

    if (ModeButtonState == LOW) {
      modeIndex = modeIndex == 0 ? 1 : 0;
      renderMainScreen();
    }

    if (MeteringButtonState == LOW) {
      lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE_2);
      lux = getCurrentLuxValue();
      updateCurrentSettings();
      renderMainScreen();
    }

    if (MeteringModeButtonState == LOW) {
      meteringMode = meteringMode == 0 ? 1 : 0;
      renderMainScreen();

      if (meteringMode == 0) {
        // Ambient light meter mode.
        lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE_2);
        lux = getCurrentLuxValue();
        renderMainScreen();
        delay(200);
      } else if (meteringMode == 1) {
        lux = 0;
        // Flash light metering
        lightMeter.configure(BH1750::CONTINUOUS_LOW_RES_MODE);

        unsigned long startTime = millis();
        volatile uint16_t currentLux = 0;

        while (true) {
          // check max flash metering time
          if (startTime + MaxFlashMeteringTime < millis()) {
            updateCurrentSettings();
            break;
          }

          currentLux = getCurrentLuxValue();
          delay(16);

          if (currentLux > lux) {
            lux = currentLux;
          }
        }

        renderMainScreen();
        delay(200);
      }
    }
  } else if (isISOView) {
    if (MenuButtonState == LOW) {
      renderMainScreen();
    }

    if (PlusButtonState == LOW) {
      ISOIndex++;

      if (ISOIndex > MaxISOIndex) {
        ISOIndex = 0;
      }

      updateCurrentSettings();
      renderISOView();
    } else if (MinusButtonState == LOW) {
      if (ISOIndex > 0) {
        ISOIndex--;
      } else {
        ISOIndex = MaxISOIndex;
      }

      updateCurrentSettings();
      renderISOView();
    }
  }
  
  delay(200);
}

void setup() {
  pinMode(MeteringButtonPin, INPUT_PULLUP);
  pinMode(PlusButtonPin, INPUT_PULLUP);
  pinMode(MinusButtonPin, INPUT_PULLUP);
  pinMode(ModeButtonPin, INPUT_PULLUP);
  pinMode(MeteringModeButtonPin, INPUT_PULLUP);
  pinMode(MenuButtonPin, INPUT_PULLUP);
  
  Wire.begin();
  Serial.begin(9600);

  // Initialize display with the CORRECT address (0x3C)
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);       // Normal 1:1 pixel scale
  display.setTextColor(WHITE);  // Draw white text

  ISOIndex = ISOIndex > MaxISOIndex ? DefaultISOIndex : ISOIndex;
  apertureIndex = apertureIndex > MaxApertureIndex ? DefaultApertureIndex : apertureIndex;
  shutterSpeedIndex = shutterSpeedIndex > MaxShutterSpeedIndex ? DefaultShutterSpeedIndex : shutterSpeedIndex;
  modeIndex = (modeIndex < 0 || modeIndex > 1) ? DefaultModeIndex : modeIndex;
  meteringMode = meteringMode > 1 ? 0 : meteringMode;

  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE_2);
  
  vcc = getCurrentVCC();

  renderMainScreen();
}

void loop() {
  if (millis() >= lastBatteryTime + BatteryInterval) {
    lastBatteryTime = millis();
    vcc = getCurrentVCC();
  }

  readButtons();

  render();
}