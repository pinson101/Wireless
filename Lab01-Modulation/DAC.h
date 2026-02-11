// Faults functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef DAC_H_
#define DAC_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint16_t makeFrameI(uint16_t code12);
void makeLUT(uint32_t amp, uint32_t phase);
uint16_t makeFrameI(uint16_t code12);
uint16_t makeFrameQ(uint16_t code12);
uint16_t voltsToRAW(float V);
void setFreq(uint32_t freq);
void writeDACISR();

#endif
