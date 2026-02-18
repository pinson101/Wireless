// Faults functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef DAC_H_
#define DAC_H_


#include <stdint.h>
#include "gpio.h"   // for LDAC macro usage

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

// Define LDAC as used by your gpio helpers (same form as you used)
#define LDAC PORTE,1

// Channel modes
typedef enum {
    OFF = 0,
    RAW,
    DC,
    SINE,
    TONE
} MODE_t;

// Shared globals (defined in main.c)
extern volatile MODE_t mode_i, mode_q;
extern volatile uint16_t raw_i, raw_q;        // 0..4095
extern volatile uint32_t freq_i, freq_q;      // Hz
extern volatile uint32_t phase_i, phase_q;    // degrees
extern volatile float voltage_i, voltage_q;   // volts (use float for convenience)
extern volatile uint32_t phase_acci, phase_accq; // LUT indices 0..255
extern volatile uint32_t delta_phase;         // step per sample (0..255)

// LUT size (power of two simplifies wrapping)
#define LUT_SIZE 256

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint16_t makeFrameI(uint16_t code12);
void makeLUT(uint32_t amp);
uint16_t makeFrameI(uint16_t code12);
uint16_t makeFrameQ(uint16_t code12);
uint16_t voltsToRAW(uint32_t V, uint32_t gain, uint32_t offset);
void setFreq(uint32_t freq);
void writeDACISR();
void initTimer1();

#endif
