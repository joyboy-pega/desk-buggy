#ifndef CHIBI_BITMAPS_H
#define CHIBI_BITMAPS_H

#include <Arduino.h>

// ----------------------------------------------------------------------------
// Chibi Expressive Graphics & Particles
// ----------------------------------------------------------------------------

// Cute Floating Heart (8x8)
const unsigned char chibi_heart[] PROGMEM = {
  0b01100110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00011000,
  0b00000000
};

// Cute Mini Sparkle / Star (7x7)
const unsigned char chibi_sparkle[] PROGMEM = {
  0b00010000,
  0b00010000,
  0b01111100,
  0b11111110,
  0b01111100,
  0b00010000,
  0b00010000
};

// Tiny 4-pointed Star (5x5)
const unsigned char chibi_tiny_star[] PROGMEM = {
  0b00100000,
  0b01110000,
  0b11111000,
  0b01110000,
  0b00100000
};

// Music Note (7x8)
const unsigned char chibi_music_note[] PROGMEM = {
  0b00001100,
  0b00001010,
  0b00001001,
  0b00001001,
  0b01101000,
  0b11111000,
  0b11110000,
  0b01100000
};

// Sleeping "z" (5x6)
const unsigned char chibi_zzz[] PROGMEM = {
  0b11111000,
  0b00011000,
  0b00110000,
  0b01100000,
  0b11000000,
  0b11111000
};

#endif // CHIBI_BITMAPS_H

