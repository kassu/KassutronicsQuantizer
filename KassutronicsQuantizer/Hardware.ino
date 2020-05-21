/* Hardware.ino
 *  This code does all the lower level hardware-related stuff, such as
 *  - Setting up registers at start up
 *  - Interrupt routines for the ADC and the UI timer
 *  - Read/write the front panel shift registers
 *  - Switch debounce and edge detection
 *  
 *  This code is heavily tied in with the hardware, and changing it requires
 *  some understanding of how the hardware is used.
 *  
 *  Higher level stuff is done in Core.ino (the actual quantiziation) and 
 *  UI.ino (defining the menu structure etc).
 */

// system constants
#define SWITCH_DEBOUNCE 10 // Switch debounce time, in IO loop counts (around 1ms)


void setupADC() {  
  // Disable ADC (just to be sure)
  cbi(ADCSRA,ADEN);

  // Choose voltage reference. 00=Aref external, 01=AVCC (5V), 11=internal 1.1V
  cbi(ADMUX,REFS1);
  sbi(ADMUX,REFS0);

  // Disable left adjust result
  cbi(ADMUX,ADLAR);
  
  // Define input channels
  // 0000 - 0111: ADC0 - ADC7
  // 1000: Temp sensor
  // 1110: 1.1V reference
  // 1111: GND
  cbi(ADMUX,MUX3);
  cbi(ADMUX,MUX2);
  cbi(ADMUX,MUX1);
  sbi(ADMUX,MUX0);

  // Enable auto trigger
  sbi(ADCSRA,ADATE); 

  // Enable interrupt
  sbi(ADCSRA,ADIE);
  
  // set prescale to 16 --> 13us per conversion
  // Note: prescale of 2 to 8 gives somehow lower resolution (not all values occur)
  // Datasheet specifies ADC clock must be max 200kHz for full resolution, but faster can be used if
  // 10-bit accuracy is not required. Prescaler = 64 would give 250kHz, probably close enough
  // Prescaler = 2^z, where z is these three bits (0 is lsb). max 128, min 2 (000 and 001 both give 2)
  // Set prescale to 64 = 52us per conversion (ADC clock 250 kHz)
  sbi(ADCSRA,ADPS2) ; //1
  sbi(ADCSRA,ADPS1) ; //1
  cbi(ADCSRA,ADPS0) ; //0

  #ifdef SLOW
  sbi(ADCSRA,ADPS2) ; //1
  sbi(ADCSRA,ADPS1) ; //1
  sbi(ADCSRA,ADPS0) ; //1
  #endif

  // Set Auto Trigger Source to free running
  // Other options: timer or external interrupt
  cbi(ADCSRB,ADTS2);
  cbi(ADCSRB,ADTS1);
  cbi(ADCSRB,ADTS0);

  // Disable digital input buffer of ADC0 and ADC1 pins (since we use them as analog)
  //cbi(DIDR0,ADC0D);
  //cbi(DIDR0,ADC1D);

  // Enable ADC 
  sbi(ADCSRA,ADEN);
}
 
void startADC() {
  sbi(ADCSRA,ADSC);
}

void setupPWM() {
  // Setup timer1 for PWM on pin 9 and 10
  
  // The pins should be outputs
  pinMode(9, OUTPUT); 
  pinMode(10, OUTPUT); 

  // --- Clock prescaler ---   
  // The PWM clock runs at the CPU rate (16 MHz) divided by a prescaler.
  // For settings 0x01 -- 0x05, the prescaler is 1, 8, 64, 256, 1024, respectively.
  // We use the fastest option:
  TCCR1B = TCCR1B & 0b11111000 | 0x01;   
  
  // --- Waveform Generation Mode (WGM)
  /* WGM13:0 sets the PWM mode. Of the 16 modes, these are most interesting:
     5 - 7: Fast PWM 8-10 bit, respectively
     14: Fast PWM, frequency set by ICR1 register
     15: Fast PWM, frequency set by OCR1A register
 
     In mode 14 and 15, the PWM frequency can be chosen very flexibly: 
  
         fPWM = fCPU / (N * (TOP+1))
  
     where fIO is the CPU clock, N is the prescaler set above and TOP is the value in
     either ICR1 or OCR1A (mode 14 or 15, resp.).
     
     We use mode 14, because then we can freely choose the frequency, but have the OCR1A register
     available to control the PWM on pin 9.
    */
  // The mode setting is spread over the two timer registers,so we set it one bit at the time (14 = 0b1110 = set set set clear)
  sbi(TCCR1B, WGM13);
  sbi(TCCR1B, WGM12);
  sbi(TCCR1A, WGM11);
  cbi(TCCR1A, WGM10);
  
  // Set ICR1 register define frequency (see formula above) and resolution (valid output values are 0 to ICR1)
  //ICR1 = 0x01FF; // 9 bit
  ICR1 = 0x007F; // 7 bit
  
  // Compare Output Mode - COM1A1, COM1A0, COM1B1, COM1B0 (first 4 bits of TCCR1A, respectively)
  // These set the mode for output A (=pin 9) and output B (=pin 10), respectively. In fast PWM mode, the modes are:
  //   0b00 - Normal port (disable PWM)
  //   0b01 - Only in WGM mode 14 or 15 (see above): Toggle output A on Compare Match. Output B is normal port (PWM disabled)
  //          I'm not sure what the point of this is. It seems you get always 50% duty cycle and half the normal fast PWM frequency,
  //          but then you can set the phase with register OCR1A (or analogWrite(9,phase) )
  //   0b10 - non-inverting PWM
  //   0b11 - inverting PWM
  // Arduinos analogWrite sets the first bit for the respective port each time you call analogWrite (so you can switch to 
  // digitalWrite and back whenever you feel like it). Let's set them all explicitly in the beginning, so we can write the
  // registers directly.
  sbi(TCCR1A, COM1A1);
  cbi(TCCR1A, COM1A0);
  sbi(TCCR1A, COM1B1);
  cbi(TCCR1A, COM1B0);
 
  // Write 0 to both ports as initial value
  OCR1A = 0;  
  OCR1B = 0;
}

void setupGPIO() {
  // Set up digital GPIO
  cbi(DIN_DDR, DIN_BIT); sbi(DIN_PORT, DIN_BIT); // PD2 (DINA) = input, pullup
  cbi(DIN_DDR, DIN_BIT+1); sbi(DIN_PORT, DIN_BIT+1); // PD3 (DINB) = input, pullup
  sbi(GATE_DDR, GATE_BIT); cbi(GATE_PORT, GATE_BIT); // PC4 (GATEA) = output, low
  sbi(GATE_DDR, GATE_BIT+1); cbi(GATE_PORT, GATE_BIT+1); // PC5 (GATEB) = output, low  

  // Shift register front panel interface
  sbi(SCL_DDR, SCL_BIT); sbi(SCL_PORT, SCL_BIT); // SCL output, initialize high
  cbi(SDI_DDR, SDI_BIT); cbi(SDI_PORT, SDI_BIT); // SDI input, no pullup
  sbi(SLI_DDR, SLI_BIT); sbi(SLI_PORT, SLI_BIT); // SLI output, initialize high
  sbi(SDO_DDR, SDO_BIT); cbi(SDO_PORT, SDO_BIT); // SDO output, initialize low (don't care)
  sbi(SLO_DDR, SLO_BIT); sbi(SLO_PORT, SLO_BIT); // SLO output, initialize high
  
  // Debug pins
#ifdef DEBUG_PINS
  sbi(DDRB, 3); cbi(PORTB, 3); // PC3 = MOSI (Debug signal 0) = output, low   
  sbi(DDRB, 4); cbi(PORTB, 4); // PC4 = MISO (Debug signal 1) = output, low  
  sbi(DDRB, 5); cbi(PORTB, 5); // PC5 = SCK  (Debug signal 2) = output, low  
#endif

  // We use the external interrupt hardware to detect edges on the digital inputs (INT0 = DINA, INT1 = DINB)
  // We do not enable the actual interrupt, but manually check the interrupt flag that will be set when an edge is detected
  // Set to falling edge (10 = falling edge, 11 = rising edge)
  sbi(EICRA,ISC01); // INT0 (DINA)
  cbi(EICRA,ISC00);
  sbi(EICRA,ISC11); // INT1 (DINB)
  cbi(EICRA,ISC10);
}

void setupTimer(){
  /* Timer0 runs at about 1kHz (default arduino settings), and is normally used for the millis()
     and micros() functions.
     Here we disable the millis() and micros() operation, and in stead define our own interrupt 
     where we will handle the front panel IO at 1kHz rate.
   */
  
  // Disable TIMER0 overflow interrupt
  // THIS WILL KILL millis() AND micros()!
  cbi(TIMSK0, TOIE0);
  
  // Enable output compare match A interrupt  
  sbi(TIMSK0, OCIE0A);
}

/* shiftIO writes data to the LEDs and reads the buttons in one go */
byte shiftIO(byte data) {
  // Shifts input and output with shared clock, MSB first 
  // data variable is re-used for input data to reduce number of bitshift operations
  for (int i=7; i>=0; i--)  {
    cbi(SCL_PORT,SCL_BIT); // Clock low
  
    // Write output data
    //if ( bitRead(data,7) == 1) {
    if ( data & (1<<7) ) {
      sbi(SDO_PORT,SDO_BIT); //dataOn();
    } else {      
      cbi(SDO_PORT,SDO_BIT); // dataOff();
    }

    // Shift buffer by one bit
    data <<= 1;
    
    // Read input data
    if ( SDI_PIN & (1<<SDI_BIT) ) {
      // Update LSB
      sbi(data,0);
    }

    // Both 595 and 165 registers shift on rising edge of clock pin  
    sbi(SCL_PORT,SCL_BIT); // clock high
  }
  return data;
}

/* processIO runs at ~1kHz. It handles switch debounce and turns
 *  raw button data into events that are sent off to UI.ino.
 */
void processIO() {
  // Debounce state memory (ring buffer)
  static byte debounceindex = 0;
  static byte switchdebounce0[SWITCH_DEBOUNCE];
  static byte switchdebounce1[SWITCH_DEBOUNCE];

  // Debounced result
  static byte switchstate0 = 0;
  static byte switchstate1 = 0;

  // remember the state of the shift button
  static boolean shift = false;

  byte ledstate0, ledstate1;
  unsigned int displayscale = 0;

  // Get display state from UI code
  displayscale = getDisplayState(shift);
  
  // Separate display state into bytes
  ledstate0 = ~((byte)(displayscale >> 8));
  ledstate1 = ~((byte)(displayscale));

  /* Do actual shift register I/O */
  // Sample input registers
  cbi(SLI_PORT,SLI_BIT); sbi(SLI_PORT,SLI_BIT);

  // Transfer serial data
  switchdebounce0[debounceindex] = shiftIO(ledstate1);
  switchdebounce1[debounceindex] = shiftIO(ledstate0);

  // Latch output registers
  cbi(SLO_PORT,SLO_BIT); sbi(SLO_PORT,SLO_BIT);
  /* End of shift register I/O */

  // Debounce (active low):
  // Check to see if we have SWITCH_DEBOUNCE consecutive low readings
  byte newswitchstate0 = 0;
  byte newswitchstate1 = 0; 
  for (byte i=0; i<SWITCH_DEBOUNCE; i++) {
    newswitchstate0 |= switchdebounce0[i];
    newswitchstate1 |= switchdebounce1[i];
  }

  // Increment ring buffer counter
  debounceindex++;
  if (debounceindex >= SWITCH_DEBOUNCE) debounceindex = 0;

  // Falling edge detection
  byte edge0 = (switchstate0 & ~newswitchstate0);
  byte edge1 = (switchstate1 & ~newswitchstate1);
  unsigned int edge = (edge0 << 8) | edge1;

  // Save switch state
  switchstate0 = newswitchstate0;
  switchstate1 = newswitchstate1;

  // Is the shift key pressed right now?
  shift = (newswitchstate1 | 0b11111110) == 0b11111110;
  
  // If any edge was detected, raise event
  if (edge != 0) {
    unsigned int e = edge;
    for (byte i = 0; i<16; i++) {
      if (e & 0x8000) {
        keyDownEvent(i, shift);
        // break;
      }
      e <<= 1;
    }
  }
}

/* This timer runs at ~1 kHz and is used for all front panel IO (buttons, LEDs, menus)
 */
ISR(TIMER0_COMPA_vect) {
  /* Using nested interrupts: we allow the ADC interrupt to fire while processing this one using the sei() instruction
   * However, to make sure we don't start nesting inside ourselves we disable the COMPA interrupt (in case our processing takes
   * longer than one timer cycle).
   * NOTE: could use the ISR_NOBLOCK flag to insert the sei() instruction earlier, but its safer to first disable COMPA interrupt.
   */
  // Disable COMPA interrupt
  cbi(TIMSK0, OCIE0A); 
  sei();

  DEBUG_ON(1);
  
  // Actual processing
  processIO();

  DEBUG_OFF(1);
  
  // Re-enable the COMPA interrupt
  sbi(TIMSK0, OCIE0A);
}

/* The ADC interrupt runs at ~19 kHz and cycles through the 4 ADC channels. All "critical" processing of 
 * ADC readings, trigger inputs and gate outputs is done in this interrupt (in the processADC function)
 * TODO: Check/think about what happens if we run out of time (52 us including all overhead). Interrupts are blocking,
 * so they will slow down, but the ADC will hapilly continue and I suspect that, if we miss a reading, we mess up the channel counting.
 */
ISR(ADC_vect){
  static byte readchannel;
  static byte nextchannel = 0;
  int val = 0;
  byte low, high;
  
  DEBUG_ON(0);
  
  // Increment channel modulo 4
  nextchannel++; 
  nextchannel &= 0b11;
  
  // NOTE: The channel will be updated in hardware only for the next conversion.
  // In this ISR, we are processing the previous conversion.
  // The "current" conversion is happening right now in the background.
  ADMUX = (ADMUX & 0b11110000) | nextchannel;

  // We now read out the _previous_ channel, which is nextchannel - 2 (modulo 4)
  // This can be calculated quickly by flipping the 2nd bit
  readchannel = nextchannel ^ 0b10; 

  // Read ADC register. Must read low byte first to ensure atomic operation
  low  = ADCL;
  high = ADCH;

  // Combine the two bytes
  val = (high << 8) | low;
  
  // Call processing function in Core.ino depending on channel (input or CV)
  if ((readchannel & 0b1) == 0b1) {
    processCV(readchannel >> 1, val);
  } else {
    processChannel(readchannel >> 1, val);
  }

  DEBUG_OFF(0);
}
