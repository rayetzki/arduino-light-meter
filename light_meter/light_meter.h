#include <BH1750.h>

#define HighResCorrection 2.4  // Correction value for CONTINUOUS_HIGH_RES_MODE_2 mode
#define DomeMultiplier    2.17 // Dome correction factor

BH1750 lightMeter; // Address is 0x23

unsigned long lastInterruptTime = 0;

float getMinDistance(float x, float v1, float v2) {
  if (x - v1 > v2 - x) {
    return v2;
  }

  return v1;
}

float luxToEV(float lux) {
  if (lux <= 0) return 0;
  return log(lux / 2.5) / log(2);
}

float getCurrentLuxValue() {
  unsigned long interruptTime = millis();
  
  if (interruptTime - lastInterruptTime < 120) {
    return 0;
  }

  delay(180);

  uint16_t lux = lightMeter.readLightLevel();

  lastInterruptTime = interruptTime;
  lux /= HighResCorrection;
  return lux;
}

// return aperture value (1.4, 1.8, 2.0) by index in sequence (0, 1, 2, 3, ...).
float getApertureByIndex(uint8_t indx, uint8_t MaxApertureIndex) {
  float roundIndx = 10.0;

  if (indx > 39) {
    roundIndx = 1;
  }

  float f = round(pow(2, indx / 3.0 * 0.5) * roundIndx) / roundIndx;

  // the formula returns exact value, but photographers uses more memorable values.
  // convert it.

  if (f >= 1.1 && f < 1.2) {
    f = 1.1;
  } else if (f >= 1.2 && f < 1.4) {
    f = 1.2;
  } else if (f > 3.2 && f < 4) {
    f = 3.5;
  } else if (f > 5 && f < 6.3) {
    f = 5.6;
  } else if (f > 10 && f < 11) {
    f = 10;
  } else if (f >= 11 && f < 12) {
    f = 11;
  } else if (f >= 12 && f < 14) {
    f = 13;
  } else if (f >= 14 && f < 16) {
    f = 14;
  } else if (f >= 20 && f < 22) {
    f = 20;
  } else if (f >= 22 && f < 25) {
    f = 22;
  } else if (f >= 24 && f < 28) {
    f = 25;
  } else if (f >= 28 && f < 40) {
    f = 36;
  } else if (f >= 40 && f < 45) {
    f = 40;
  } else if (f >= 45 && f < 50) {
    f = 45;
  } else if (f >= 50 && f < 57) {
    f = 51;
  } else if (f >= 71 && f < 80) {
    f = 72;
  } else if (f >= 80 && f < 90) {
    f = 80;
  } else if (f >= 90 && f < 101) {
    f = 90;
  }

  return f;
}

// Return ISO value (100, 200, 400, ...) by index in sequence (0, 1, 2, 3, ...).
long getISOByIndex(uint8_t indx, uint8_t MaxISOIndex) {
  if (indx < 0 || indx > MaxISOIndex) {
    indx = 0;
  }

  indx += 10;

  long int factor = 1;
  float iso = 0;

  if (indx > 60) {
    indx -= 50;
    factor = 100000;
  } else if (indx > 50) {
    indx -= 40;
    factor = 10000;
  } else if (indx > 40) {
    indx -= 30;
    factor = 1000;
  } else if (indx > 30) {
    indx -= 20;
    factor = 100;
  } else if (indx > 20) {
    indx -= 10;
    factor = 10;
  }

  if (indx == 10) {
    iso = 8;
  } else if (indx == 11) {
    iso = 10;
  } else if (indx == 12) {
    iso = 12.5;
  } else if (indx == 13) {
    iso = 16;
  } else if (indx == 14) {
    iso = 20;
  } else if (indx == 15) {
    iso = 25;
  } else if (indx == 16) {
    iso = 32;
  } else if (indx == 17) {
    iso = 40;
  } else if (indx == 18) {
    iso = 50;
  } else if (indx == 19) {
    iso = 64;
  } else if (indx == 20) {
    iso = 80;
  }

  iso = (long)floor(iso * factor);
  return iso;
}

float getShutterSpeedByIndex(uint8_t indx, uint8_t MaxShutterSpeedIndex) {
  if (indx < 0 || indx >= MaxShutterSpeedIndex) {
    indx = 0;
  }

  float factor = 0;
  float t = 0;

  if (indx < 10) {
    factor = 100.0;
  } else if (indx < 20) {
    indx -= 10;
    factor = 10.0;
  } else if (indx < 30) {
    indx -= 20;
    factor = 1.0;
  } else if (indx < 40) {
    indx -= 30;
    factor = 0.1;
  } else if (indx < 50) {
    indx -= 40;
    factor = 0.01;
  } else if (indx < 60) {
    indx -= 50;
    factor = 0.001;
  } else if (indx < 70) {
    indx -= 60;
    factor = 0.0001;
  } else if (indx < 80) {
    indx -= 70;
    factor = 0.00001;
  }

  if (indx == 0) {
    t = 100;
  } else if (indx == 1) {
    t = 80;
  } else if (indx == 2) {
    t = 64;
  } else if (indx == 3) {
    t = 50;
  } else if (indx == 4) {
    t = 40;
  } else if (indx == 5) {
    t = 32;
  } else if (indx == 6) {
    t = 25;
  } else if (indx == 7) {
    t = 20;
  } else if (indx == 8) {
    t = 16;
  } else if (indx == 9) {
    t = 12.5;
  }

  t = 1 / (t * factor);
  return t;
}

// Convert calculated time (in seconds) to photograpy style shutter speed. 
double formatShutterSpeed(double time, uint8_t MaxShutterSpeedIndex) {
  double divider = 1;

  float maxTime = getShutterSpeedByIndex(time, MaxShutterSpeedIndex);

  if (time < maxTime) {
    return maxTime;
  }

  time = 1 / time;

  if (time > 99999) {
    divider = 10000;
  } else if (time > 9999) {
    divider = 1000;
  } else if (time > 999) {
    divider = 100;
  } else if (time > 99) {
    divider = 10;
  }

  time /= divider;

  if (time >= 10 && time <= 12.5) {
    time = getMinDistance(time, 10, 12.5);
  } else if (time >= 12.5 && time <= 16) {
    time = getMinDistance(time, 12.5, 16);
  } else if (time >= 16 && time <= 20) {
    time = getMinDistance(time, 16, 20);
  } else if (time >= 20 && time <= 25) {
    time = getMinDistance(time, 20, 25);
  } else if (time >= 25 && time <= 32) {
    time = getMinDistance(time, 25, 32);
  } else if (time >= 32 && time <= 40) {
    time = getMinDistance(time, 32, 40);
  } else if (time >= 40 && time <= 50) {
    time = getMinDistance(time, 40, 50);
  } else if (time >= 50 && time <= 64) {
    time = getMinDistance(time, 50, 64);
  } else if (time >= 64 && time <= 80) {
    time = getMinDistance(time, 64, 80);
  } else if (time >= 80 && time <= 100) {
    time = getMinDistance(time, 80, 100);
  }

  time *= divider;

  if (time == 32) {
    time = 30;
  }

  if (time == 16) {
    time = 15;
  }

  time = 1 / time;

  return time;
}

// Convert calculated aperture value to photograpy style aperture value. 
float formatAperture(float a, uint8_t MaxApertureIndex) {
  for (int i = 0; i < MaxApertureIndex; i++) {
    float a1 = getApertureByIndex(i, MaxApertureIndex);
    float a2 = getApertureByIndex(i + 1, MaxApertureIndex);

    if (a1 < a && a2 >= a) {
      return getMinDistance(a, a1, a2);
    }
  }

  return 0;
}
