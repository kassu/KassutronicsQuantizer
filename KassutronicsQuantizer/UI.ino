/* UI.ino - Interface/menu structure
 *  
 * This file handles the user interface at high level. 
 * In particular, it dynamically calculates what LEDs 
 * should be on, and handles button press events.
 */
 
// Front panel key functions
#define KEY_GATELENGTH 0
#define KEY_ROTATE 1
#define KEY_TRANSPOSE 2
#define KEY_TRANSPOSE_ONE 3
#define KEY_OFFSET 4
#define KEY_OFFSET_ONE 5
#define KEY_KEYBOARD 6
#define KEY_LEGATO 7
#define KEY_QM 8
#define KEY_TRIGGERDELAY 9
#define KEY_CVA 10
#define KEY_CVB 11
#define KEY_LOAD 12
#define KEY_SAVE 12
#define KEY_UP 13
#define KEY_DOWN 14
#define KEY_HOME 15
#define KEY_SHIFT 15

// Macro for making an LED blink to show what menu we are in.
// The blink pattern changes if data == key.
#define BLINKDISPLAY(data,key) { \
  (data == key) ? \
    (antiblink) ?  0 : (0x8000 >> data)    : \
    (blink)     ?  (0x8000 >> data) | (0x8000 >> key) : (0x8000 >> data)    }

// Same but without data (always just blink key)
#define BLINKKEY(key) ((blink) ? (0x8000 >> key) : 0)

// Macro for making the octave buttons blink differently depending on what octave is selected.
#define OCTAVEDISPLAY(octaves)  \
  (octaves > 0 && octaves <= 5) ? \
    ((blinkoctave[octaves]) ? (0x8000 >> KEY_UP) : 0 )  : \
    (octaves >= -5) ? \
      ((blinkoctave[-octaves]) ? (0x8000 >> KEY_DOWN) : 0) : \
      0
  
/* getDisplayState is executed at ~1kHz, but not time-critical (if the call takes more than 1ms it will skip the
 * next call, but that is acceptable for IO operations).
 * IMPORTANT: this code may be interrupted at any time by the Core code (processChannel and processCV).
 * Hence, when acessing any global variables that are used in those functions, 
 * interrupts must be disabled with cli() and re-enabled with sei()
 */
unsigned int getDisplayState(boolean shift) {
  // A counter to keep track of blink states
  static unsigned int counter = 0;
  counter--;

  // Keep track of autosavecounter
  if (autosavecounter > 1) {
    autosavecounter --;
  }
  
  // Precalculate all blink states (even if we might not use them)
  boolean blink = (counter & 0x100) == 0x100;
  boolean antiblink = (counter &0x1C0) == 0x000;
  boolean blinkoctave[6] = {false, (counter & 0x100) == 0x100, (counter & 0x80) == 0x80, (counter & 0x40) == 0x40, (counter & 0x20) == 0x20, (counter & 0x10) == 0x10};

  unsigned int display = 0;
  byte data;

  byte x;
  signed char oct = 0;
  byte semi = 0;

  switch (mode) {
    case normal:
      if (shift) {
        display = (0x8000 >> KEY_SHIFT);
        if (state.gatelegato) display |= (0x8000 >> KEY_LEGATO);
        return display;
      } else {
        cli();
        display = rotatedscale;
        sei();
        return display | 0b0000;
      }
    case rotate:
      return BLINKDISPLAY(state.rotatesemitones, KEY_ROTATE);
    case transpose:
      x = state.transposesemitones + 120;
      oct = (x/12) - 10;
      semi = mod12(x);

      display = BLINKDISPLAY(semi, KEY_TRANSPOSE);
      display |= OCTAVEDISPLAY(oct);
      return display;
    case transposeB:
      x = state.transposeBsemitones + 120;
      oct = (x/12) - 10;
      semi = mod12(x);

      display = BLINKDISPLAY(semi, KEY_TRANSPOSE_ONE);
      display |= OCTAVEDISPLAY(oct);
      return display;
    case offset:
      x = state.offsetsemitones + 120;
      oct = (x/12) - 10;
      semi = mod12(x);

      display = BLINKDISPLAY(semi, KEY_OFFSET);
      display |= OCTAVEDISPLAY(oct);
      return display;
    case offsetB:
      x = state.offsetBsemitones + 120;
      oct = (x/12) - 10;
      semi = mod12(x);

      display = BLINKDISPLAY(semi, KEY_OFFSET_ONE);
      display |= OCTAVEDISPLAY(oct);
      return display;
    case keyboard:
      display = BLINKDISPLAY(keyboardsemitones, KEY_KEYBOARD);
      display |= OCTAVEDISPLAY(keyboardoctaves);
      return display;
    case gatelengthmenu:
      return BLINKDISPLAY(state.gatelengthindex, KEY_GATELENGTH);
    case qmodemenu:
      return BLINKDISPLAY(state.qmode, KEY_QM);
    case cvA:
      data = (state.cvmode[0]==cvoff) ? KEY_CVA : state.cvmode[0];
      return BLINKDISPLAY(data, KEY_CVA);
    case cvB:
      data = (state.cvmode[1]==cvoff) ? KEY_CVB : state.cvmode[1];
      return BLINKDISPLAY(data, KEY_CVB);
    case load:
      return (getScaleMemoryStatus()) | (0x8000 >> KEY_LOAD);
    case save:
      return (getScaleMemoryStatus()) | BLINKKEY(KEY_SAVE);
    case triggerdelaymenu:
      return BLINKDISPLAY(state.triggerdelay, KEY_TRIGGERDELAY);
  }

  // Something is wrong if we get here!
  return 0;
}

// Process key presses depending on mode  
/* IMPORTANT: this code may be interrupted at any time by the Core code (processChannel and processCV).
 * Hence, when acessing any global variables that are used in those functions, 
 * interrupts must be disabled with cli() and re-enabled with sei()
 */
void keyDownEvent(byte key, boolean shift) {
  byte x;
  signed char oct = 0;
  byte semi = 0;
  
  switch (mode) {
    case normal:
      if (shift == false) {
        if (key<12) {
          // Toggle one bit in the scale
          byte rotation = state.rotatesemitones + rotatesemitonesCV;
          byte unrotatedkey = mod12(key + 24 - rotation); // rotation can be at most 24
          cli();
          state.scale ^= (0x8000 >> unrotatedkey);
          updateRotation();
          sei();
          autosavecounter = autosavetime;
        } else if (key==KEY_LOAD) {
          mode = load;
        } else if (key==KEY_UP) {
          state.rotatesemitones++;
          if (state.rotatesemitones == 12) state.rotatesemitones = 0;
          cli();
          updateRotation();
          sei();
          autosavecounter = autosavetime;
        } else if (key==KEY_DOWN) {
          if (state.rotatesemitones == 0) state.rotatesemitones = 12;
          state.rotatesemitones--;
          cli();
          updateRotation();
          sei();
          autosavecounter = autosavetime;
        }
      } else { // shift == true
        if (key==KEY_GATELENGTH) {
          mode = gatelengthmenu;
        } else if (key==KEY_ROTATE) {
          mode = rotate;
        } else if (key==KEY_TRANSPOSE) {
          mode = transpose;
        } else if (key==KEY_TRANSPOSE_ONE) {
          mode = transposeB;
        } else if (key==KEY_OFFSET) {
          mode = offset;
        } else if (key==KEY_OFFSET_ONE) {
          mode = offsetB;
        } else if (key==KEY_KEYBOARD) {
          mode = keyboard;
        } else if (key==KEY_LEGATO) {
          state.gatelegato = !state.gatelegato;
        } else if (key==KEY_QM) {
          mode = qmodemenu;
        } else if (key==KEY_TRIGGERDELAY) {
          mode = triggerdelaymenu;
        } else if (key==KEY_CVA) {
          mode = cvA;
        } else if (key==KEY_CVB) {
          mode = cvB;
        } else if (key==KEY_SAVE) {
          mode = save;
        }
      }
      break;
    case rotate:
      if (key<12) {
        state.rotatesemitones = key;
        cli();
        updateRotation();
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case keyboard:
      if (key<12) {
        cli();
        keyboardsemitones = key;
        keyboardtriggered = 2;
        sei();
      } else if (key==KEY_HOME) {
        mode = normal;
      } else if (key==KEY_DOWN && keyboardoctaves > -5) {
        cli();
        keyboardoctaves--;
        keyboardtriggered = 2;
        sei();
      } else if (key==KEY_UP && keyboardoctaves < 5) {
        cli();
        keyboardoctaves++;
        keyboardtriggered = 2;
        sei();
      }
      break;
    case transpose:
      if (key<12) {
        x = state.transposesemitones + 120;
        oct = (x/12) - 10;
        semi = mod12(x);
        cli();
        state.transposesemitones = 12*oct + key;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      } else if (key==KEY_DOWN && state.transposesemitones > -49) {
        cli();
        state.transposesemitones -= 12;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_UP && state.transposesemitones < 60) {
        cli();
        state.transposesemitones += 12;
        sei();
        autosavecounter = autosavetime;
      }
      break;
    case transposeB:
      if (key<12) {
        x = state.transposeBsemitones + 120;
        oct = (x/12) - 10;
        semi = mod12(x);
        cli();
        state.transposeBsemitones = 12*oct + key;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      } else if (key==KEY_DOWN && state.transposeBsemitones > -49) {
        cli();
        state.transposeBsemitones -= 12;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_UP && state.transposeBsemitones < 60) {
        cli();
        state.transposeBsemitones += 12;
        sei();
        autosavecounter = autosavetime;
      }
      break;
    case offset:
      if (key<12) {
        x = state.offsetsemitones + 120;
        oct = (x/12) - 10;
        semi = mod12(x);
        cli();
        state.offsetsemitones = 12*oct + key;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      } else if (key==KEY_DOWN && state.offsetsemitones > -49) {
        cli();
        state.offsetsemitones -= 12;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_UP && state.offsetsemitones < 60) {
        cli();
        state.offsetsemitones += 12;
        sei();
        autosavecounter = autosavetime;
      }
      break;
    case offsetB:
      if (key<12) {
        x = state.offsetBsemitones + 120;
        oct = (x/12) - 10;
        semi = mod12(x);
        cli();
        state.offsetBsemitones = 12*oct + key;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      } else if (key==KEY_DOWN && state.offsetBsemitones > -49) {
        cli();
        state.offsetBsemitones -= 12;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_UP && state.offsetBsemitones < 60) {
        cli();
        state.offsetBsemitones += 12;
        sei();
        autosavecounter = autosavetime;
      }
      break;
    case gatelengthmenu:
      if (key<12) {
        cli();
        state.gatelengthindex = key;
        updateGatelength();
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case qmodemenu:
      if (key<=MAX_Q_MODE) {
        cli();
        state.qmode = (QMode)key;
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case cvA:
      if (key<=MAX_CV_MODE || key==KEY_LOAD) {
        cli();
        state.cvmode[0] = (CVMode)key;
        resetCVState(0);
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_CVA) {
        cli();
        state.cvmode[0] = cvoff;
        resetCVState(0);
        sei();        
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case cvB:
      if (key<=MAX_CV_MODE || key==KEY_LOAD) {
        cli();
        state.cvmode[1] = (CVMode)key;
        resetCVState(1);
        sei();
        autosavecounter = autosavetime;
      } else if (key==KEY_CVB) {
        cli();
        state.cvmode[1] = cvoff;
        resetCVState(1);
        sei();  
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case load:
      if (key<12) {
        loadScale(key);
        mode = normal;
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case save:
      if (key<12) {
        saveScale(key);
        mode = normal;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
    case triggerdelaymenu:
      if (key<12) {
        state.triggerdelay = key;
        autosavecounter = autosavetime;
      } else if (key==KEY_HOME) {
        mode = normal;
      }
      break;
  }
}
