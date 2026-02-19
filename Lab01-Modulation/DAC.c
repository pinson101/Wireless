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
void makeLUT(uint32_t amp)
{
    int i;
    for(i = 0; i < LUT_SIZE; i++)
    {
        LUTi[i] = (amp/1000) * sin((2 * M_PI * i) / 256) * gain_i + offset_i;
        LUTq[i] = (amp/1000) * cos((2 * M_PI * i) / 256) * gain_q + offset_q;
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
    uint16_t R = offset + ((V / 1000) * gain); // volts given in mV
    if (R > 4095) R = 4095;
    return R;
}

void setFreq(uint32_t freq)
{
    TIMER1_TAILR_R = 40e6 / freq * (2^32 - 1);
}

char *buffer;
// the ISR writes each sample of the signal (depends on sampling frequency)
void writeDACISR()
{
    setPinValue(LDAC, 0);
    setPinValue(LDAC, 1);

    uint16_t codeI;
    uint16_t codeQ;

    switch(mode_i)
    {
        case OFF:
            break;
        case RAW:
            codeI = raw_i; // raw value from shell
            break;
        case DC:
            codeI = voltsToRAW(voltage_i, gain_i, offset_i);
            break;
        case SINE:
            codeI = LUTi[phase_acci >> 24];
            break;
        case TONE:
            codeI = LUTi[phase_acci >> 24];
            break;
        default: break;
    }

    switch(mode_q)
    {
        case OFF:
            break;
        case RAW:
            codeQ = raw_q;
            break;
        case DC:
            codeQ = voltsToRAW(voltage_q, gain_q, offset_q);
            break;
        case SINE:
            codeQ = LUTq[phase_accq];
            break;
        case TONE:
            codeQ = LUTq[phase_accq];
            break;
    }

    // advance phase
    if (mode_i== TONE || mode_i == SINE)
    {
        phase_acci += delta_phasei;
        if (phase_acci > 256) phase_acci = 0;
    }
    if (mode_q == TONE || mode_q == SINE)
    {
        phase_accq += delta_phaseq;
        if (phase_accq > 256) phase_accq = 0;
    }

    // write to DAC
    putsUart0(toAsciiHex(buffer, codeI));
    putsUart0("\n\r");
    if(mode_i != OFF) writeSpi1Data(makeFrameI(codeI));
    if(mode_q != OFF) writeSpi1Data(makeFrameQ(codeQ));

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

    // 5) Set reload value for 20 us period at 80 MHz:
    //    ticks = clock * period = 80e6 * 20e-6 = 1600 -> TAILR = 1600 - 1 = 1599
    TIMER1_TAILR_R = 1600 - 1;                  // periodic reload value

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
