/* SaveRecall.ino - EEPROM stuff
 * 
 * EEPROM memory map (memory size 1k = 0x400 bytes)
 * 0x000 - 0x00F  Identifier block
 * 0x010 - 0x01F  Autosaved system state
 * 0x020 - 0x0FF  Reserved for future use, not initialized
 * 0x100 - 0x1BF  First bank of 12 saved scales. Enough space to save complete persistentState in the future if desired.
 * 0x1C0 - 0x3FF  Reserved, fits 3 more banks of 12 saved states each
 * 
 * Identifier block
 * 0x000           Data version, 1 byte, 0x01 is first and current version (0x00 is empty memory on first startup)
 * 0x001 - 0x00F   Reserved, not initialized
 * 
 * State data block (offset addresses)
 * 0x0 - 0x1  scale
 * 0x2        gatelengthindex
 * 0x3        rotatesemitones 
 * 0x4        transposesemitones
 * 0x5        transposeBsemitones
 * 0x6        offsetsemitones
 * 0x7        offsetBsemitones
 * 0x8        legato
 * 0x9        qmode
 * 0xA        triggerdelay
 * 0xB - 0xC  cvMode
 * 0xD - 0xF  reserved (set to 0)
 */

// Using the arduino EEPROM library
#include <EEPROM.h>

// Address definitions, see memory map above
#define EE_ADDR_ID 0x000
#define EE_ADDR_STATE  0x010
#define EE_ADDR_SAVEDSCALES 0x100
#define EE_SIZE_SAVEDSCALES 0x10

volatile bool eebusy = false;

void loadPersistentState() {
 if (EEPROM.read(EE_ADDR_ID) == 0x01 && !EE_RESET) {
    EEPROM.get(EE_ADDR_STATE, state);
  } else {
    initializePersistentState();
  }
  updateRotation();
  updateGatelength();
}

void initializePersistentState() {
    // Initialize EEPROM with all default values
    EEPROM.put(EE_ADDR_STATE, state);
    for (int i=0; i<12; i++) {
      saveScale(i, 0);    
    }
    EEPROM.write(EE_ADDR_ID, 0x01);
}


void savePersistentState() {
  PersistentState tstate;
  if (!eebusy) {
    eebusy = true;
    //tstate = state;
    EEPROM.put(EE_ADDR_STATE, state);
    eebusy = false;
  }
}

void saveScale(byte index) {  
  unsigned int tscale = rotatedscale;
  saveScale(index, tscale);
}

void saveScale(byte index, unsigned int tscale) {
  if (index < 12) {
    while (eebusy) {}
    eebusy = true;
    EEPROM.put(EE_ADDR_SAVEDSCALES + index*EE_SIZE_SAVEDSCALES, tscale);
    eebusy = false;
  }
}

void loadScale(byte index) {
  unsigned int tscale;
  if (index < 12) {
    EEPROM.get(EE_ADDR_SAVEDSCALES + index*EE_SIZE_SAVEDSCALES, tscale);
    state.scale = tscale;
    state.rotatesemitones = 0;
    updateRotation();
  }
}

// Check which memory locations have something written in them
unsigned int getScaleMemoryStatus() {
  unsigned int tscale = 0;
  unsigned int tdisplay = 0;
  for (byte index = 0; index < 12; index ++) {
    EEPROM.get(EE_ADDR_SAVEDSCALES + index*EE_SIZE_SAVEDSCALES, tscale);
    if (tscale != 0) {
      tdisplay |= (0x8000 >> index);
    }
  }
  return tdisplay;
}
