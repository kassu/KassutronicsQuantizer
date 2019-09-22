// **** Debug functionality ****
/* SLOW slows down the ADC by a factor 2, giving more time for core processing. */
//#define SLOW

/* DEBUG_PINS sets up the MOSI, MISO and SCK pins debug outputs 0, 1 and 2 respectively
   They can be set in code with the DEBUG_ON(i) and DEBUG_OFF(i) macros, where i = 0-2.
   Typically used for checking all kinds of timing issues  */
//#define DEBUG_PINS

/* DEBUGPRINT enables the serial port for debugging. This kind of works if you occasionally
  print something, but breaks if you try to send any significant amount of data. */
//#define DEBUGPRINT

// Set to true to reset the EEPROM to default values on startup
#define EE_RESET false

/* By default arduino sets the compiler to optimize for size. Using this pragma we can optimize
  for speed in stead. This results in a much larger binary (but still fits), but unfortunately
  no significant speed increase at all. */
//#pragma GCC optimize ("-O3")

#include "hardware.h"

volatile unsigned int autosavecounter = 0;

// **** Setup routine. All actual setup is done in Hardware.ino ****
void setup() {
  loadPersistentState();
  
  setupPWM();
  setupADC();
  setupGPIO();
  
  // Bit of delay after setup to allow the ADC to be properly set (probably not needed)
  delay(10);

  setupTimer();
  
  // Trigger the ADC once, should get it started
  startADC();
     
  #ifdef DEBUGPRINT
  Serial.begin(19200);
  #endif
}

// **** Main program loop ****
void loop() {
  // All important stuff runs from the ADC and Timer0 interrupts.
  // Here we have only the lowest priority process, which is initiating an autosave when needed
  if (autosavecounter == 1) {
    savePersistentState();
    autosavecounter = 0;
  }
}
