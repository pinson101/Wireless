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

/* calibration parameters for DAC output voltage (macros) */
#define OFFSET_I 2104  /* measured DAC code that maps to 0V after op-amp */
#define OFFSET_Q 2104
#define GAIN_I   1991  /* counts corresponding to +0.5V from offset (calibrate) */
#define GAIN_Q   1991

char encoding_patterns[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const uint32_t encoding_patterns_len = sizeof(encoding_patterns) / sizeof(encoding_patterns[0]);
const uint32_t encoding_total_bits = encoding_patterns_len * 8;
uint32_t pattern_index = 0;
uint32_t mod_patterni[416];
uint32_t mod_patternq[416];

int32_t ookI[2] = { 0,
                    GAIN_I};
int32_t ookQ[2] = { 0,
                    0};

int32_t bpskI[2] = { GAIN_I,
                    -GAIN_I};
int32_t bpskQ[2] = { 0,
                     0};

int32_t qpskI[2] = { GAIN_I,
                    -GAIN_I};
int32_t qpskQ[2] = { GAIN_Q,
                    -GAIN_Q};

int32_t psk8I[8] = {  GAIN_I * 1.00,   // 000 -> 0�
                      GAIN_I * 0.71,   // 001 -> 45�
                     -GAIN_I * 0.71,   // 010 -> 135�
                      GAIN_I * 0.00,   // 011 -> 90�
                      GAIN_I * 0.71,   // 100 -> 315�
                     -GAIN_I * 0.00,   // 101 -> 270�
                     -GAIN_I * 1.00,   // 110 -> 180�
                     -GAIN_I * 0.71 }; // 111 -> 225�

int32_t psk8Q[8] = {  GAIN_Q * 0.00,   // 000 -> 0�
                      GAIN_Q * 0.71,   // 001 -> 45�
                      GAIN_Q * 0.71,   // 010 -> 135�
                      GAIN_Q * 1.00,   // 011 -> 90�
                     -GAIN_Q * 0.71,   // 100 -> 315�
                     -GAIN_Q * 1.00,   // 101 -> 270�
                     -GAIN_Q * 0.00,   // 110 -> 180�
                     -GAIN_Q * 0.71 }; // 111 -> 225�

int32_t qam16I[4] = { -GAIN_I,
                      -GAIN_I/3,
                       GAIN_I/3,
                       GAIN_I };

int32_t qam16Q[4] = { -GAIN_Q,
                      -GAIN_Q/3,
                       GAIN_Q/3,
                       GAIN_Q };

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

        int32_t v_i = (int32_t)lround((double)OFFSET_I + a * s * (double)GAIN_I);
        int32_t v_q = (int32_t)lround((double)OFFSET_Q + a * c * (double)GAIN_Q);

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

void modulate()
{
    switch(mode_i)
    {
        case OOK:
        {
            uint32_t byte_index = 0;
            uint32_t bit_index = 0;
            uint32_t b = 0;
            for (byte_index = 0; byte_index < encoding_patterns_len; ++byte_index)
            {
                for (b = 0; b < 8; ++b)
                {
                    if (bit_index >= encoding_total_bits) break;
                    uint8_t bit = (encoding_patterns[byte_index] >> b) & 0x1;
                    int32_t v = ookI[bit];
                    int32_t r = v + OFFSET_I;
                    if (r < 0) r = 0; else if (r > 4095) r = 4095;
                    mod_patterni[bit_index++] = (uint32_t)r;
                }
            }
        }
            break;
        case BPSK:
        {
            uint32_t byte_index = 0;
            uint32_t bit_index = 0;
            uint32_t b = 0;
            for (byte_index = 0; byte_index < encoding_patterns_len; ++byte_index)
            {
                for (b = 0; b < 8; ++b)
                {
                    if (bit_index >= encoding_total_bits) break;
                    uint8_t bit = (encoding_patterns[byte_index] >> b) & 0x1;
                    int32_t v = bpskI[bit];
                    int32_t r = v + OFFSET_I;
                    if (r < 0) r = 0; else if (r > 4095) r = 4095;
                    mod_patterni[bit_index++] = (uint32_t)r;
                }
            }
        }
            break;

        case QPSK:
        {
            uint32_t symbol_index = 0;
            uint32_t bit_pos = 0;

            uint32_t byte_index = 0;
            uint32_t b = 0;
            /* iterate through bits two at a time (LSB-first within each byte) */
            for (byte_index = 0; byte_index < encoding_patterns_len; ++byte_index)
            {
                for (b = 0; b < 8; b += 2)
                {
                    /* check availability of first bit */
                    uint8_t bit0 = (encoding_patterns[byte_index] >> b) & 0x1;
                    /* second bit may be in same byte or next byte if b==7 (handled by next loop iterations)
                       but since we iterate in steps of 2 within each byte, we only need to check bounds */
                    uint8_t bit1 = 0;
                    if (b + 1 < 8)
                    {
                        bit1 = (encoding_patterns[byte_index] >> (b + 1)) & 0x1;
                    }

                    /* map bits to I/Q levels: bit 0 -> I index, bit1 -> Q index */
                    int32_t vi = qpskI[bit0];
                    int32_t vq = qpskQ[bit1];

                    int32_t ri = vi + OFFSET_I;
                    int32_t rq = vq + OFFSET_Q;
                    if (ri < 0) ri = 0; else if (ri > 4095) ri = 4095;
                    if (rq < 0) rq = 0; else if (rq > 4095) rq = 4095;

                    mod_patterni[symbol_index] = (uint32_t)ri;
                    mod_patternq[symbol_index] = (uint32_t)rq;
                    symbol_index++;

                    bit_pos += 2;
                    if (bit_pos >= encoding_total_bits) break;
                }
                if (bit_pos >= encoding_total_bits) break;
            }
        }
            break;

        case PSK8:
        {
            uint32_t symbol_index = 0;
            uint32_t bit_pos = 0;

            /* consume 3 bits per PSK8 symbol (LSB-first across bytes) */
            while ((bit_pos + 3) <= encoding_total_bits)
            {
                uint32_t val = 0;
                uint32_t k = 0;
                for (k = 0; k < 3; ++k)
                {
                    uint32_t pos = bit_pos + k;
                    uint8_t byte = (uint8_t)encoding_patterns[pos >> 3];
                    uint32_t bitin = pos & 7;
                    val |= (((byte >> bitin) & 0x1) << k);
                }

                int32_t vi = psk8I[val];
                int32_t vq = psk8Q[val];

                int32_t ri = vi + OFFSET_I;
                int32_t rq = vq + OFFSET_Q;
                if (ri < 0) ri = 0; else if (ri > 4095) ri = 4095;
                if (rq < 0) rq = 0; else if (rq > 4095) rq = 4095;

                mod_patterni[symbol_index] = (uint32_t)ri;
                mod_patternq[symbol_index] = (uint32_t)rq;
                symbol_index++;

                bit_pos += 3;
            }
        }
            break;

        case QAM16:
        {
            uint32_t symbol_index = 0;
            uint32_t bit_pos = 0;

            /* consume 4 bits per 16-QAM symbol (LSB-first across bytes)
               lower 2 bits -> I index, upper 2 bits -> Q index */
            while ((bit_pos + 4) <= encoding_total_bits)
            {
                uint32_t val = 0;
                uint32_t k = 0;
                for (k = 0; k < 4; ++k)
                {
                    uint32_t pos = bit_pos + k;
                    uint8_t byte = (uint8_t)encoding_patterns[pos >> 3];
                    uint32_t bitin = pos & 7;
                    val |= (((byte >> bitin) & 0x1) << k);
                }

                uint32_t i_idx = val & 0x3;
                uint32_t q_idx = (val >> 2) & 0x3;

                int32_t vi = qam16I[i_idx];
                int32_t vq = qam16Q[q_idx];

                int32_t ri = vi + OFFSET_I;
                int32_t rq = vq + OFFSET_Q;
                if (ri < 0) ri = 0; else if (ri > 4095) ri = 4095;
                if (rq < 0) rq = 0; else if (rq > 4095) rq = 4095;

                mod_patterni[symbol_index] = (uint32_t)ri;
                mod_patternq[symbol_index] = (uint32_t)rq;
                symbol_index++;

                bit_pos += 4;
            }
        }
            break;

        default:

            break;
    }
}

// the ISR writes each sample of the signal (depends on sampling frequency)
void writeDACISR()
{
    setPinValue(LDAC, 0);
    setPinValue(LDAC, 1);

    switch(mode_i)
    {
        case OFF:
            codeI = OFFSET_I;
            break;
        case RAW:
            codeI = raw_i; // raw value from shell
            break;
        case DC:
            codeI = voltsToRAW(amplitude_i, GAIN_I, OFFSET_I);
            break;
        case SINE:
            codeI = LUTi[phase_acci >> 20];
            break;
        case TONE:
            codeI = LUTi[phase_acci >> 20];
            break;
        case OOK:
            codeI = mod_patterni[pattern_index];
            break;
        case BPSK:
            codeI = mod_patterni[pattern_index];
            break;
        case QPSK:
            codeI = mod_patterni[pattern_index];
            break;
        case PSK8:
            codeI = mod_patterni[pattern_index];
            break;
        case QAM16:
            codeI = mod_patterni[pattern_index];
            break;
        default: break;
    }

    switch(mode_q)
    {
        case OFF:
            codeQ = OFFSET_Q;
            break;
        case RAW:
            codeQ = raw_q;
            break;
        case DC:
            codeQ = voltsToRAW(amplitude_q, GAIN_Q, OFFSET_Q);
            break;
        case SINE:
            codeQ = LUTq[phase_accq >> 20];
            break;
        case TONE:
            codeQ = LUTq[phase_accq >> 20];
            break;
        case QPSK:
            codeQ = mod_patternq[pattern_index];
            break;
        case PSK8:
            codeQ = mod_patternq[pattern_index];
            break;
        case QAM16:
            codeQ = mod_patternq[pattern_index];
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
    if (mode_i == OOK || mode_i == BPSK || mode_i == QPSK || mode_i == PSK8 || mode_i == QAM16)
    {
        pattern_index++;
    }

    /* wrap pattern_index according to the active modulation symbol count */
    uint32_t wrap_limit;
    switch (mode_i)
    {
        case OOK:
        case BPSK:
            wrap_limit = encoding_total_bits; break; /* 1 bit per symbol */
        case QPSK:
            wrap_limit = encoding_total_bits / 2; break; /* 2 bits per symbol */
        case PSK8:
            wrap_limit = encoding_total_bits / 3; break; /* 3 bits per symbol */
        case QAM16:
            wrap_limit = encoding_total_bits / 4; break; /* 4 bits per symbol */
        default:
            wrap_limit = encoding_total_bits; break;
    }

    if (pattern_index >= wrap_limit) pattern_index = 0;

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
    TIMER1_TAILR_R = 800 - 1;                  // periodic reload value

    // 6) Clear any pending timeout
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;

    // 7) Enable Timer0A timeout interrupt
    TIMER1_IMR_R |= TIMER_IMR_TATOIM;

    // 8) Enable Timer0A
    TIMER1_CTL_R |= TIMER_CTL_TAEN;

    // 9) Enable IRQ in NVIC turn-on interrupt 37 (TIMER1A) in NVIC
    NVIC_EN0_R = 1 << (INT_TIMER1A-16);
}

