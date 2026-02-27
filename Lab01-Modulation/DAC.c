// Target uC:       TM4C123GH6PM
// System Clock:    80 MHz

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
#define GAIN_I   1991  /* counts corresponding to +0.5V from offset */
#define GAIN_Q   1991
#define H_GAIN   65536 /* gain of 1.0 in Q16 fixed-point format (for filtering) */
#define NTAPS    31    /* number of filter taps for pulse shaping */
#define SPS      4     /* samples per symbol */

char encoding_patterns[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const uint32_t encoding_patterns_len = sizeof(encoding_patterns) - 1;
const uint32_t encoding_total_bits   = encoding_patterns_len * 8;
uint32_t pattern_index = 0;
int32_t mod_patterni[424];
int32_t mod_patternq[424];


// filtering stuff
int32_t filter_bufferi[31];
int32_t filter_bufferq[31];
static uint8_t pattern_phase = 0;
volatile uint32_t num_symbols = 0;


// modulation patterns
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

int32_t psk8I[8] = {  GAIN_I * 1.00,   // 000 -> 0
                      GAIN_I * 0.71,   // 001 -> 45
                     -GAIN_I * 0.71,   // 010 -> 135
                      GAIN_I * 0.00,   // 011 -> 90
                      GAIN_I * 0.71,   // 100 -> 315
                     -GAIN_I * 0.00,   // 101 -> 270
                     -GAIN_I * 1.00,   // 110 -> 180
                     -GAIN_I * 0.71 }; // 111 -> 225

int32_t psk8Q[8] = {  GAIN_Q * 0.00,   // 000 -> 0
                      GAIN_Q * 0.71,   // 001 -> 45
                      GAIN_Q * 0.71,   // 010 -> 135
                      GAIN_Q * 1.00,   // 011 -> 90
                     -GAIN_Q * 0.71,   // 100 -> 315
                     -GAIN_Q * 1.00,   // 101 -> 270
                     -GAIN_Q * 0.00,   // 110 -> 180
                     -GAIN_Q * 0.71 }; // 111 -> 225

int32_t qam16I[4] = { -GAIN_I,
                      -GAIN_I/3,
                       GAIN_I/3,
                       GAIN_I };

int32_t qam16Q[4] = { -GAIN_Q,
                      -GAIN_Q/3,
                       GAIN_Q/3,
                       GAIN_Q };

int32_t hrrc[] = {
                    0.0023 * H_GAIN,
                   -0.0043 * H_GAIN,
                   -0.0102 * H_GAIN,
                   -0.0090 * H_GAIN,
                    0.0015 * H_GAIN,
                    0.0159 * H_GAIN,
                    0.0230 * H_GAIN,
                    0.0130 * H_GAIN,
                   -0.0136 * H_GAIN,
                   -0.0422 * H_GAIN,
                   -0.0493 * H_GAIN,
                   -0.0160 * H_GAIN,
                    0.0593 * H_GAIN,
                    0.1553 * H_GAIN,
                    0.2357 * H_GAIN,
                    0.2671 * H_GAIN,
                    0.2357 * H_GAIN,
                    0.1553 * H_GAIN,
                    0.0593 * H_GAIN,
                   -0.0160 * H_GAIN,
                   -0.0493 * H_GAIN,
                   -0.0422 * H_GAIN,
                   -0.0136 * H_GAIN,
                    0.0130 * H_GAIN,
                    0.0230 * H_GAIN,
                    0.0159 * H_GAIN,
                    0.0015 * H_GAIN,
                   -0.0090 * H_GAIN,
                   -0.0102 * H_GAIN,
                   -0.0043 * H_GAIN,
                    0.0023 * H_GAIN
                 };


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

static inline uint8_t getBit(uint32_t bit_pos)
{
    // LSB-first across bytes
    uint8_t byte = (uint8_t)encoding_patterns[bit_pos >> 3];
    return (byte >> (bit_pos & 7)) & 1U;
}

static inline uint16_t sat12_from_baseband(int32_t bb, int32_t offset)
{
    int32_t r = bb + offset;
    if (r < 0) r = 0;
    else if (r > 4095) r = 4095;
    return (uint16_t)r;
}

static inline int isModMode(uint32_t m)
{
    return (m == OOK || m == BPSK || m == QPSK || m == PSK8 || m == QAM16);
}

void modulate(void)
{
    uint32_t bit_pos = 0;
    uint32_t sym = 0;

    // reset indices / filter state when (re)building pattern
    pattern_index  = 0;
    pattern_phase  = 0;
    int i;
    for (i = 0; i < NTAPS; i++)
    {
        filter_bufferi[i] = 0;
        filter_bufferq[i] = 0;
    }

    switch (mode_i)
    {
        case OOK:
        {
            // 1 bit per symbol, Q=0
            for (bit_pos = 0; bit_pos < encoding_total_bits && sym < 424; bit_pos++, sym++)
            {
                uint8_t b = getBit(bit_pos);
                mod_patterni[sym] = ookI[b];
                mod_patternq[sym] = 0;
            }
            num_symbols = sym;
        }
        break;

        case BPSK:
        {
            // 1 bit per symbol, Q=0
            for (bit_pos = 0; bit_pos < encoding_total_bits && sym < 424; bit_pos++, sym++)
            {
                uint8_t b = getBit(bit_pos);
                mod_patterni[sym] = bpskI[b];
                mod_patternq[sym] = 0;
            }
            num_symbols = sym;
        }
        break;

        case QPSK:
        {
            // 2 bits per symbol: bit0->I sign, bit1->Q sign (LSB-first stream)
            while ((bit_pos + 1) < encoding_total_bits && sym < 424)
            {
                uint8_t b0 = getBit(bit_pos + 0);
                uint8_t b1 = getBit(bit_pos + 1);

                mod_patterni[sym] = qpskI[b0];
                mod_patternq[sym] = qpskQ[b1];

                sym++;
                bit_pos += 2;
            }
            num_symbols = sym;
        }
        break;

        case PSK8:
        {
            // 3 bits per symbol -> 0..7
            while ((bit_pos + 2) < encoding_total_bits && sym < 424)
            {
                uint32_t v =
                    ((uint32_t)getBit(bit_pos + 0) << 0) |
                    ((uint32_t)getBit(bit_pos + 1) << 1) |
                    ((uint32_t)getBit(bit_pos + 2) << 2);

                mod_patterni[sym] = psk8I[v];
                mod_patternq[sym] = psk8Q[v];

                sym++;
                bit_pos += 3;
            }
            num_symbols = sym;
        }
        break;

        case QAM16:
        {
            // 4 bits per symbol -> lower2=I index, upper2=Q index
            while ((bit_pos + 3) < encoding_total_bits && sym < 424)
            {
                uint32_t v =
                    ((uint32_t)getBit(bit_pos + 0) << 0) |
                    ((uint32_t)getBit(bit_pos + 1) << 1) |
                    ((uint32_t)getBit(bit_pos + 2) << 2) |
                    ((uint32_t)getBit(bit_pos + 3) << 3);

                uint32_t i_idx = (v >> 0) & 0x3;
                uint32_t q_idx = (v >> 2) & 0x3;

                mod_patterni[sym] = qam16I[i_idx];
                mod_patternq[sym] = qam16Q[q_idx];

                sym++;
                bit_pos += 4;
            }
            num_symbols = sym;
        }
        break;

        default:
            num_symbols = 0;
        break;
    }
}

int32_t clamp12(int32_t v)
{
    if (v < OFFSET_I - GAIN_I) return OFFSET_I - GAIN_I;
    else if (v > OFFSET_I + GAIN_I) return OFFSET_I + GAIN_I;
    return v;
}

// the ISR writes each sample of the signal (depends on sampling frequency)
void writeDACISR(void)
{
    setPinValue(LDAC, 0);
    setPinValue(LDAC, 1);

    // defaults
    uint16_t outI = OFFSET_I;
    uint16_t outQ = OFFSET_Q;

    // -------------------------
    // I channel (non-mod modes)
    // -------------------------
    switch (mode_i)
    {
        case OFF:  outI = OFFSET_I; break;
        case RAW:  outI = (uint16_t)raw_i; break;
        case DC:   outI = voltsToRAW(amplitude_i, GAIN_I, OFFSET_I); break;
        case SINE: outI = LUTi[phase_acci >> 20]; break;
        case TONE: outI = LUTi[phase_acci >> 20]; break;
        default:   break; // mod modes handled below
    }

    // -------------------------
    // Q channel (non-mod modes)
    // -------------------------
    switch (mode_q)
    {
        case OFF:  outQ = OFFSET_Q; break;
        case RAW:  outQ = (uint16_t)raw_q; break;
        case DC:   outQ = voltsToRAW(amplitude_q, GAIN_Q, OFFSET_Q); break;
        case SINE: outQ = LUTq[phase_accq >> 20]; break;
        case TONE: outQ = LUTq[phase_accq >> 20]; break;
        default:   break; // mod modes handled below
    }

    // -------------------------
    // MODULATION PATH (uses shared pattern_index)
    // -------------------------
    const int i_mod = isModMode(mode_i);
    const int q_mod = isModMode(mode_q);

    if ((i_mod || q_mod) && (num_symbols > 0))
    {
        if (filter_enabled)
        {
            // RRC filtering on I/Q symbol streams
            int32_t xinI = 0;
            int32_t xinQ = 0;

            // zero-stuffing: symbol, 0, 0, 0, symbol, 0, 0, 0...
            if (pattern_phase == 0)
            {
                xinI = mod_patterni[pattern_index];
                xinQ = mod_patternq[pattern_index];

                // advance symbol once, wrap around to beginning of pattern when reaching end
                pattern_index++;
                if (pattern_index >= num_symbols) pattern_index = 0;
            }

            // advance phase of zero-stuffing pattern (0,0,0,1,0,0,0,1,...)
            pattern_phase++;
            if (pattern_phase >= SPS) pattern_phase = 0;

            // shift buffer and insert new sample at index 0 - oldest sample is dropped off
            int j;
            for (j = NTAPS - 1; j > 0; j--)
            {
                filter_bufferi[j] = filter_bufferi[j - 1];
                filter_bufferq[j] = filter_bufferq[j - 1];
            }
            filter_bufferi[0] = xinI;
            filter_bufferq[0] = xinQ;

            // convolution
            int64_t sumI = 0;
            int64_t sumQ = 0;
            int k;
            for (k = 0; k < NTAPS; k++)
            {
                sumI += (int64_t)filter_bufferi[k] * (int64_t)hrrc[k];
                sumQ += (int64_t)filter_bufferq[k] * (int64_t)hrrc[k];
            }

            // divide by H_GAIN
            // int32_t yI = (int32_t)((sumI + (H_GAIN / 2)) / H_GAIN);
            // int32_t yQ = (int32_t)((sumQ + (H_GAIN / 2)) / H_GAIN);

            // divide by H_GAIN with bit shift (H_GAIN=65536=2^16)
            int32_t yI = (int32_t)(sumI >> 16);
            int32_t yQ = (int32_t)(sumQ >> 16);

            // compensate for zero-stuffing amplitude loss
            yI *= 4;
            yQ *= 4;

            // convert to DAC codes
            outI = sat12_from_baseband(yI, OFFSET_I);
            outQ = sat12_from_baseband(yQ, OFFSET_Q);
        }
        else
        {
            // no filter: output one symbol per ISR tick
            int32_t bbI = 0;
            int32_t bbQ = 0;

            if (i_mod) bbI = mod_patterni[pattern_index];
            if (q_mod) bbQ = mod_patternq[pattern_index];

            outI = i_mod ? sat12_from_baseband(bbI, OFFSET_I) : outI;
            outQ = q_mod ? sat12_from_baseband(bbQ, OFFSET_Q) : outQ;

            // advance ONCE per sample tick (shared for I/Q)
            pattern_index++;
            if (pattern_index >= num_symbols) pattern_index = 0;
        }
    }

    // -------------------------
    // phase accumulators for tone/sine
    // -------------------------
    if (mode_i == TONE || mode_i == SINE) phase_acci += delta_phasei;
    if (mode_q == TONE || mode_q == SINE) phase_accq += delta_phaseq;

    // write to DAC
    writeSpi1Data(makeFrameI(outI));
    writeSpi1Data(makeFrameQ(outQ));

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

    // 5) Set reload value for 30KHz freq at 80 MHz:
    //    ticks = clock * period = 80e6 / 30e6 = 2666.67 -> TAILR = 2666 - 1 = 2665
    TIMER1_TAILR_R = 2665;                  // periodic reload value

    // 6) Clear any pending timeout
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;

    // 7) Enable Timer0A timeout interrupt
    TIMER1_IMR_R |= TIMER_IMR_TATOIM;

    // 8) Enable Timer0A
    TIMER1_CTL_R |= TIMER_CTL_TAEN;

    // 9) Enable IRQ in NVIC turn-on interrupt 37 (TIMER1A) in NVIC
    NVIC_EN0_R = 1 << (INT_TIMER1A-16);
}

