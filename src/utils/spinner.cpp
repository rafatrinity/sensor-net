//spinner.cpp
#include "spinner.hpp"
#include "config.hpp"
#include <Arduino.h>

void spinner() {
  static int8_t counter = 0;
  static int8_t lastCounter = -1;
  const char* glyphs = "\xa1\xa5\xdb";

  if (counter != lastCounter) {
      LCD.setCursor(15, 1);
      LCD.print(glyphs[counter]);
      lastCounter = counter;
  }
  counter = (counter + 1) % strlen(glyphs);
}