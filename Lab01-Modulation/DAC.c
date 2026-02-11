// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "spi1.h"
#include <math.h>

/* GLOBALS
 * -----------------------------------------------------------------------------
 * delta_phase    how much the ISR steps through the LUT
 * mode_i/q       determines how to write bits into DAC
 * raw_i/q
 */

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint16_t LUTi[256];
uint16_t LUTq[256];

/* Calibration Math
 *      Vi = (R - offset) x 0.5 / gain
 * offset: found when Vout = 0
 * gain:   found when Vout = 0.5
 */
uint32_t offset_i = 2104;
uint32_t gain_i; //105
uint32_t offset_q;
uint32_t gain_q;

// make sin wave (cos is just sin with phase shift)
void makeLUT(uint32_t amp, uint32_t phase)
{
    int i;
    for(i = 0; i < 257; i++)
    {
        LUTi[i] = amp * sin((2 * M_PI * i) / 256) * gain_i + offset_i;
        LUTq[i] = amp * cos((2 * M_PI * i) / 256) * gain_q + offset_q;
    }
}

// i frame 0011...
uint16_t makeFrameI(uint16_t code12)
{
    return (0x3 << 12) | (code12 & 0x0FFF);
}

// q frame 1011...
uint16_t makeFrameQ(uint16_t code12)
{
    return (0xB << 12) | (code12 & 0x0FFF);
}

uint16_t voltsToRAW(float V, uint32_t gain, uint32_t offset)
{
    float R = 0ffset + (V * gain / 0.5);
    if (R > 4095) R = 4095;
    return R;
}

void setFreq(uint32_t freq)
{
    TIMER1_TAILR_R = = 40e6 / freq * (2^32 - 1);
}

// the ISR writes each sample of the signal (depends on sampling frequency)
void writeDACISR()
{
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;

    uint16_t codeI;
    uint16_t codeQ;

    switch(mode_i)
    {
        case OFF:
            codeI = offset_i; // sets i to 0V
            break;
        case RAW:
            codeI = raw_i; // raw value from shell
            break;
        case DC:
            codeI = voltsToRAW(voltage_i, gain_i, offset_i);
            break;
        case SINE:
            codeI = LUT[phase_acci];
            break;
        case TONE:
            codeI = LUT[phase_acci];
            break;
        default break;
    }

    switch(mode_q)
    {
        case OFF:
            codeQ = offset_q;
            break;
        case RAW:
            codeQ = raw_q;
            break;
        case DC:
            codeQ = voltsToRAW(voltage_q, gain_q, offset_q);
            break;
        case SINE:
            codeQ = LUT[phase_accq];
            break;
        case TONE:
            codeQ = LUT[phase_accq];
            break;
    }

    // advance phase once per sample
    if (mode_i== TONE || mode_i == SINE)
    {
        phase_acci += delta_phase;
        if (phase_acci > 256) phase_acci = 0;
    }
    if (mode_q == TONE || mode_q == SINE)
    {
        phase_accq += delta_phase;
        if (phase_accq > 256) phase_accq = 0;
    }

    // write to DAC
    setPinValue(LDAC, 1);
    writeSpi1Data(makeFrameI(codeI));
    writeSpi1Data(makeFrameQ(codeQ));
    setPinValue(LDAC, 0);
}



/*
 * in main()
 *  rate 40MHz/50kHz in GPIO timer for frequency
 *  TAIL_R = 40e6/50e3
 *
 *  NVIC_ST_RELOAD_R = 40000 - 1;  // 1ms tick period
    NVIC_ST_CTRL_R = NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE;

    // TIMER1A counting up (wrap around at 2^32 / 40e6 = 107.37 seconds)
    // Enable timer 1
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;
    while ((SYSCTL_PRTIMER_R & SYSCTL_PRTIMER_R1) == 0);
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;
    // set to 32 bit
    TIMER1_CFG_R = 0x00000000;
    // count up
    TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD | TIMER_TAMR_TACDIR;
    // set reload value (max)
    TIMER1_TAILR_R = 0xFFFFFFFF;
    // clear timeout flag and start timer
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
    TIMER1_CTL_R |= TIMER_CTL_TAEN;
 *
 * in shell()
 *  if phase exists just take it as a ratio and start sampling from there
 */
