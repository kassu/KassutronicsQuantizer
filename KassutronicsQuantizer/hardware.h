/* hardware.h
 * This file contains hardware pinout definitions as well as 
 * some macros to access I/O pins and general purpose macros 
 */

// Macros to set and clear a single bit in any register
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// ***** Pin definitions ******
/* This code defines port names and numbers of the pins by function */

// DIN: Trigger input A
// Elsewhere code assumes Trigger B is on the same port as and one bit above Trigger A
#define DIN_PORT PORTD
#define DIN_PIN PIND
#define DIN_DDR DDRD
#define DIN_BIT 2

// GATE: Gate output A
// Elsewhere code assumes Gate B is on the same port as and one bit above Gate A
#define GATE_PORT PORTC
#define GATE_DDR DDRC
#define GATE_BIT 4

// Front panel shift register pins
// SCL: Serial CLock shared by 595 and 165
#define SCL_PORT PORTB
#define SCL_PIN PINB
#define SCL_DDR DDRB
#define SCL_BIT 0
// SDI: 165 Serial Data Input port
#define SDI_PORT PORTD
#define SDI_PIN PIND
#define SDI_DDR DDRD
#define SDI_BIT 4
// SLI: 165 latch data ouput port
#define SLI_PORT PORTD
#define SLI_PIN PIND
#define SLI_DDR DDRD
#define SLI_BIT 5
// SDO: 595 Serial Data Output port
#define SDO_PORT PORTD
#define SDO_PIN PIND
#define SDO_DDR DDRD
#define SDO_BIT 6
// SLO: 595 Latch Output port
#define SLO_PORT PORTD
#define SLO_PIN PIND
#define SLO_DDR DDRD
#define SLO_BIT 7

// **** Macros for I/O operations ****

// Macros for setting gate output
/* These are basically sbi(GATE_PORT, GATE_BIT+i) and similarly cbi,
 * however by separating the constant GATE_BIT that part of the bit shift will be done at compile time,
 * and only a shift by i (0 or 1) has to be done at runtime (AVR needs one instruction for each single bit shift).
 */
#define GATE_ON(i)  (GATE_PORT |=  ((1 << GATE_BIT) << i))
#define GATE_OFF(i) (GATE_PORT &= ~((1 << GATE_BIT) << i))

// Macro for reading the trigger input
#define DIN_READ(i) (DIN_PIN & ((1 << DIN_BIT) << i))

// DAC output macro setDAC(channel, value)
volatile unsigned int *dac_register[2] = {&OCR1A, &OCR1B};
__attribute__((always_inline)) static inline void setDAC(byte i, unsigned int value) {
  *dac_register[i] = value;
}

// Debug output pin functions
#ifdef DEBUG_PINS
#define DEBUG_ON(i) sbi(PORTB, 3+i)
#define DEBUG_OFF(i) cbi(PORTB, 3+i)
#else
#define DEBUG_ON(i) {}
#define DEBUG_OFF(i) {}
#endif

// **** Miscellaneous macros and functions ****

/* The single most processor intensive calculation we need to do is calculating modulo 12. This function does 
 * modulo 12 on byte data quickly using a single table lookup (implementation detail: it actually does modulo 3
 * after dividing by 4 using bit shifting).
 * Beware that the argument is used twice, so don't feed it complex calculations. For example don't do:
 *    byte result = mod12(a*b+c);  // Slow! a*b+c is evaluated twice.
 * but do:
 *    byte input = a*b+c;     
 *    byte result = mod12(input);  // Faster!
 */
byte mod12table[64] = {0,0,0,12,12,12,24,24,24,36,36,36,48,48,48,60,60,60,72,72,72,84,84,84,96,96,96,108,108,108,120,120,120,132,132,132,144,144,144,156,156,156,168,168,168,180,180,180,192,192,192,204,204,204,216,216,216,228,228,228,240,240,240,252};
#define mod12(value) ((value) - mod12table[(value) >> 2])


// Similar to Arduino's map function but with int datatypes, making it much faster in our application
int intmap(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
