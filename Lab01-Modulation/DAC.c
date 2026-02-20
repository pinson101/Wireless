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
#include <math.h>

#include "tm4c123gh6pm.h"
#include "spi1.h"
#include "gpio.h"
#include "DAC.h"
#include "uart0.h"
#include "CLI.h"

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

// LUTs
static uint16_t LUTi[LUT_SIZE];
static uint16_t LUTq[LUT_SIZE];

// calibration - set to your measured values after bench calibration
static const uint32_t offset_i = 2104; // measured DAC code that maps to 0V after op-amp
static const uint32_t offset_q = 2104;
static const uint32_t gain_i   = 1991;  // Measured to be 1995, but gain + offset must be <= 4095. counts corresponding to +0.5V from offset (calibrate)
static const uint32_t gain_q   = 1991;  // Measured to be 2002, but gain + offset must be <= 4095.

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// make sin wave (cos is just sin with phase shift)
void makeLUT(uint32_t amplitude)
{
    double a = (double)amplitude / 1000.0; /* amplitude in V (from mV) */
    int k;
    for(k = 0; k < LUT_SIZE; k++)
    {
        double angle = (2.0 * M_PI * (double)k) / (double)LUT_SIZE;
        double s = sin(angle);
        double c = cos(angle);

        int32_t v_i = (int32_t)lround((double)offset_i + a * s * (double)gain_i);
        int32_t v_q = (int32_t)lround((double)offset_q + a * c * (double)gain_q);

        if (v_i < 0) v_i = 0; else if (v_i > 4095) v_i = 4095;
        if (v_q < 0) v_q = 0; else if (v_q > 4095) v_q = 4095;

        LUTi[k] = (uint16_t)v_i;
        LUTq[k] = (uint16_t)v_q;
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

uint16_t voltsToRAW(uint32_t V, uint32_t gain, uint32_t offset)
{
    // V in mV; compute offset + (V * gain / 1000) with full precision
    uint32_t R = offset + (uint32_t)(((uint64_t)V * gain) / 1000ULL);
    if (R > 4095) R = 4095;
    return (uint16_t)R;
}

//void setFreq(uint32_t freq)
//{
//    TIMER1_TAILR_R = 40e6 / freq * ((1UL << 32) - 1);
//}

// the ISR writes each sample of the signal (depends on sampling frequency)
void writeDACISR()
{
    setPinValue(LDAC, 0);
    setPinValue(LDAC, 1);

    switch(mode_i)
    {
        case OFF:
            codeI = offset_i;
            break;
        case RAW:
            codeI = raw_i; // raw value from shell
            break;
        case DC:
            codeI = voltsToRAW(amplitude_i, gain_i, offset_i);
            break;
        case SINE:
            codeI = LUTi[phase_acci >> 20];
            break;
        case TONE:
            codeI = LUTi[phase_acci >> 20];
            break;
        default: break;
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
            codeQ = voltsToRAW(amplitude_q, gain_q, offset_q);
            break;
        case SINE:
            codeQ = LUTq[phase_accq >> 20];
            break;
        case TONE:
            codeQ = LUTq[phase_accq >> 20];
            break;
    }

    // advance phase once per sample
    if (mode_i == TONE || mode_i == SINE)
    {
        phase_acci += delta_phasei;
    }
    if (mode_q == TONE || mode_q == SINE)
    {
        phase_accq += delta_phaseq; 
    }

    // write to DAC
    writeSpi1Data(makeFrameI(codeI));
    writeSpi1Data(makeFrameQ(codeQ));

    // clear the timer interrupt
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
}

void initTimer1()
{
    // 1) Enable clock
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;
//    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    // 3) Disable Timer0A during configuration
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;

    // 4) Configure as 32-bit timer, periodic, count down
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;      // 32-bit timer mode
    TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;     // periodic timer (count-down)

    // 5) Set reload value for 10 us period at 80 MHz:
    //    ticks = clock * period = 80e6 * 10e-6 = 800 -> TAILR = 800 - 1 = 799
    TIMER1_TAILR_R = 400 - 1;                  // periodic reload value

    // 6) Clear any pending timeout
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;

    // 7) Enable Timer0A timeout interrupt
    TIMER1_IMR_R |= TIMER_IMR_TATOIM;

    // 8) Enable Timer0A
    TIMER1_CTL_R |= TIMER_CTL_TAEN;

    // 9) Enable IRQ in NVIC turn-on interrupt 37 (TIMER1A) in NVIC
    NVIC_EN0_R = 1 << (INT_TIMER1A-16);
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
