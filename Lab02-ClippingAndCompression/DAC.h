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

// Define LDAC as used by your gpio helpers
#define LDAC PORTE,1

// Channel modes
typedef enum {
    OFF = 0,
    RAW,
    DC,
    SINE,
    TONE,
    OOK,
    BPSK,
    QPSK,
    PSK8,
    QAM16
} MODE_t;

// Shared globals (defined in main.c)
extern volatile MODE_t mode_i, mode_q;
extern volatile uint16_t raw_i, raw_q;        // 0..4095
extern volatile uint32_t freq_i, freq_q;      // Hz
extern volatile uint32_t phase_i, phase_q;    // degrees
extern volatile uint32_t amplitude_i, amplitude_q;   // millivolts
extern volatile uint32_t phase_acci, phase_accq; // LUT indices 0..255
extern volatile uint32_t delta_phasei, delta_phaseq;         // step per sample (0..255)
extern volatile uint16_t codeI, codeQ;      // final DAC codes to write (0..4095)
extern volatile uint32_t sampling_freq;     // Hz
extern volatile uint8_t filter_enabled;    // 0 = no filter, 1 = filter enabled
extern volatile uint8_t clip_enabled;      // 0 = no clipping, 1 = clipping enabled
extern volatile uint32_t clip_level;      // clipping level in mV


#define LUT_SIZE 4096

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint16_t makeFrameI(uint16_t code12);
void makeLUT(uint32_t amp);
uint16_t makeFrameI(uint16_t code12);
uint16_t makeFrameQ(uint16_t code12);
uint16_t voltsToRAW(uint32_t V, uint32_t gain, uint32_t offset);
void modulate();
void writeDACISR();
void initTimer1();

#endif
