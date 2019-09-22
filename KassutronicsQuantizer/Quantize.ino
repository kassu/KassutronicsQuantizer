/* Quantize.ino - Quantization algorithms
 *  
 *  This file contains several quantization algorithms, corresponding to the 
 *  different quantization modes. Each function takes in a measured voltage in 
 *  ADC units, and returns a quantized note in DAC units.
 *  
 *  The functions differ in how they treat the quantization scale.
 * 
 *  Valid DAC range is 0-127. These functions may return values outside this range,
 *  which will be simply ignored (skip the new note) by the Core code.
 *  Some of these functions explicitly return 255 to indicate "skip".
 */

// Trivial implementation without scale
// This version is used to quantize CV inputs A and B
byte quantizeSemitones(int adcval) {
  return (adcval + quartertone) >> 3;
}

// Quantization mode 0: Nearest
// Returns the nearest note enabled in the scale
byte quantizeNearest(int adcval) {
  byte note, r, scalenote;
  bool current;
  byte next,prev;
  unsigned int localscale;
  const unsigned int bitmask = 1<<15;

  // Quantize to semitone and keep track of the remainder
  note = (adcval + quartertone) >> 3; // round(adcval/8)
  r = adcval & 0b111; // remainder(adcval/8)

  // Convert the quantized note to an index in the scale (0 to 11)
  scalenote = mod12(note + 8);

  // Copy the scale to a local variable
  localscale = rotatedscale;
  
  // Special case: when all notes are off, we return "cannot quantize" flag
  if (localscale == 0) {
    return(255);
  }

  // Loop through the first part of the scale scale to find the lowest and highest enabled note *before* the requested note
  byte ll=255, lh=255;
  byte j;
  // Loop from 0 to the first enabled note
  for (j=0; j<scalenote; j++) {
    if (localscale & bitmask) {
      ll = j;
      break;
    }
    localscale <<= 1;
  }
  // Continue looping until the current note and remember the last enabled note
  for (; j<scalenote; j++) {
    if (localscale & bitmask) {
      lh = j;
    }
    localscale <<= 1;
  }

  // Check if the current note is enabled
  current = (localscale & bitmask) == bitmask;
  localscale <<= 1;

  // Loop further through the scale to find the lowest *after* the requested note  
  byte hl=255, hh=255;
  for (j=scalenote+1; j<12; j++) {
    if (localscale & bitmask) {
      hl = j;
      break;
    }
    localscale <<= 1;
  }
  // Continue looping until the end of the scale and remember the last enabled note.
  for (; j<12; j++) {
    if (localscale & bitmask) {
      hh = j;
    }
    localscale <<= 1;
  }
  
  // Now we can determine the distance, in semitones, to the next and previous enabled tone
  if (lh==255) {
    if (hh==255) {
      prev = 12;
    } else {
      prev = scalenote + 12 - hh;
    }
  } else {
    prev = scalenote - lh;
  }
  
  if (hl==255) {
    if (ll==255) {
      next = 12;
    } else {
      next = 12 + ll - scalenote; // Twelve + LL
    }
  } else {
    next = hl - scalenote;
  }
  
  // Determine closest quantized note, combining all of the above
  if (current) {
    return(note);
  } else if ((next < prev) || ((next == prev) && (r < 4))) {
    return(note + next);
  } else { // next > prev
    return(note - prev);
  }
  
  // Note: (note + next) or (note - prev) may be out of bounds for the DAC range
  // Since we return an unsigned value, both cases will give "large" positive values >= 128,
  // which should be checked for by the calling function. An invalid candidate should just be not accepted.
}

// Quantization mode 1: Skip
// Returns the quantized note if it was enabled, or 255 to indicate skip if it was disabled int he scale
byte quantizeSkip(int adcval) {
  byte note, r, scalenote;
  unsigned int localscale;
  const unsigned int bitmask = 1<<15;
  
  localscale = rotatedscale;
  // Special case: when all notes are off, we return "cannot quantize" flag
  if (localscale == 0) {
    return(255);
  }
  
  note = (adcval + quartertone) >> 3; // round(adcval/8)
  r = adcval & 0b111; // remainder(adcval/8)

  // Look-up table uses about 3us, at the expense of 64 bytes of memory (could be progmem if we need it back)
  scalenote = mod12(note + 8);

  // Shift the requested note to the leftmost position
  localscale <<= scalenote;

  // Is the note enabled?
  if ((localscale & bitmask) == bitmask) {
    return(note);
  } else {
    return(255);
  }
}

// Quantization mode 2: equal
// Returns the nearest quantized note, with all enabled notes equally distributed in the octave
// NOTE: This takes up pretty much as much time as we have available, test timing when making changes!
byte quantizeEqual(int adcval) {
  byte note, r, scalenote;
  unsigned int localscale;
  const unsigned int bitmask = 1<<15;

  // Quantize to semitone and keep track of the remainder
  note = (adcval + quartertone) >> 3; // round(adcval/8)
  r = adcval & 0b111; // remainder(adcval/8)

  // Convert the quantized note to an index in the scale (0 to 11)
  scalenote = mod12(note + 8);
  byte noteoffset = note - scalenote;
  
  // Copy the scale to a local variable
  localscale = rotatedscale;
  
  // Special case: when all notes are off, we return "cannot quantize" flag
  if (localscale == 0) {
    return(255);
  }

  // Loop through the scale to find all enabled notes
  // This takes most of the time, and could in principle be moved to updateRotation() if needed.
  byte j;
  byte numnotes = 0;
  byte notes[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
  for (j=0; j<12; j++) {
    if (localscale & bitmask) {
      notes[numnotes] = j;
      numnotes++;
    }
    localscale <<= 1;
  }

  // A high-resolution version of the scale note in adc units (0 to 95)
  byte scalenotehighres = (scalenote << 3) + r;

  // byte*byte->unsigned int (8*8->16) multiplication is supported directly by the processor
  unsigned int x = (scalenotehighres * numnotes);
  byte index = x/96;
  return(noteoffset + notes[index]);
}

