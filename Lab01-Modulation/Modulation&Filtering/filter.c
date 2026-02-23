/*
 * filter.c  –  Root-Raised-Cosine (RRC) pulse-shaping filter implementation
 *
 * Name: Bryan Gonzalez  |  ID: 1001443032
 * Target: TM4C123GH6PM
 *
 * Design notes
 *   α  = RRC_ALPHA (0.35)
 *   N  = RRC_TAPS  (33, odd)
 *   T  = sps       (DAC samples per symbol, e.g. 100 at 1 ksym/s, 100 kHz DAC)
 *
 *   Coefficients are normalised to unit energy so the peak output stays
 *   within [-1, +1] for any unit-amplitude input symbol.
 *
 *   The delay line holds one floating-point IQ value per past symbol
 *   (not per DAC sample) – the filter is evaluated at the symbol rate
 *   and the result is held constant for sps DAC samples (NRZ upsampling).
 *   This keeps the ISR lightweight: one FIR evaluation per symbol period,
 *   not per DAC sample.
 */

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "filter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*===========================================================================
 * State
 *===========================================================================*/
volatile bool rrc_enabled = false;

static float rrc_coeff[RRC_TAPS];

/* Circular delay line – one slot per past symbol (length = RRC_TAPS) */
#define DELAY_LEN  RRC_TAPS

static float delay_i[DELAY_LEN];
static float delay_q[DELAY_LEN];
static uint32_t delay_ptr = 0;   /* points to the oldest sample (write head) */

/*===========================================================================
 * rrcBuild  –  compute RRC coefficients for sps samples-per-symbol
 *===========================================================================*/
void rrcBuild(uint32_t sps)
{
    int   half   = RRC_TAPS / 2;
    float T      = (float)sps;
    float a      = RRC_ALPHA;
    float energy = 0.0f;
    int   n;

    for (n = -half; n <= half; n++)
    {
        float t = (float)n;
        float h;

        if (n == 0)
        {
            /* Centre tap */
            h = (1.0f - a + 4.0f * a / (float)M_PI);
        }
        else if (fabsf(fabsf(t) * 4.0f * a - T) < 1e-5f)
        {
            /* Special case: t = ±T / (4α) – avoid divide-by-zero */
            float s = sinf((float)M_PI / (4.0f * a));
            float c = cosf((float)M_PI / (4.0f * a));
            h = (a / (float)(M_PI * sqrtf(2.0f))) *
                ((1.0f + 2.0f / (float)M_PI) * s +
                 (1.0f - 2.0f / (float)M_PI) * c);
        }
        else
        {
            float nt  = t / T;
            float num = sinf((float)M_PI * nt * (1.0f - a)) +
                        4.0f * a * nt * cosf((float)M_PI * nt * (1.0f + a));
            float den = (float)M_PI * nt *
                        (1.0f - (4.0f * a * nt) * (4.0f * a * nt));
            h = num / den;
        }

        rrc_coeff[n + half] = h;
        energy += h * h;
    }

    /* Normalise to unit energy */
    float norm = sqrtf(energy);
    for (n = 0; n < RRC_TAPS; n++)
        rrc_coeff[n] /= norm;
}

/*===========================================================================
 * rrcReset  –  zero the delay line
 *===========================================================================*/
void rrcReset(void)
{
    memset(delay_i, 0, sizeof(delay_i));
    memset(delay_q, 0, sizeof(delay_q));
    delay_ptr = 0;
}

/*===========================================================================
 * rrcApply  –  push symbol (in_i, in_q), return filtered (out_i, out_q)
 *
 * The newest symbol is written to delay_ptr, then we convolve across the
 * entire circular buffer from newest to oldest against rrc_coeff[0..N-1].
 *===========================================================================*/
void rrcApply(float in_i, float in_q, float *out_i, float *out_q)
{
    /* Write newest symbol into the delay line */
    delay_i[delay_ptr] = in_i;
    delay_q[delay_ptr] = in_q;

    /* Convolve: tap 0 = newest symbol */
    float acc_i = 0.0f;
    float acc_q = 0.0f;
    uint32_t k;

    for (k = 0; k < RRC_TAPS; k++)
    {
        /* Walk backwards through the circular buffer */
        uint32_t idx = (delay_ptr + DELAY_LEN - k) % DELAY_LEN;
        acc_i += rrc_coeff[k] * delay_i[idx];
        acc_q += rrc_coeff[k] * delay_q[idx];
    }

    /* Clamp to [-1, +1] to protect the DAC */
    if (acc_i >  1.0f) acc_i =  1.0f;
    if (acc_i < -1.0f) acc_i = -1.0f;
    if (acc_q >  1.0f) acc_q =  1.0f;
    if (acc_q < -1.0f) acc_q = -1.0f;

    *out_i = acc_i;
    *out_q = acc_q;

    /* Advance write pointer */
    delay_ptr = (delay_ptr + 1) % DELAY_LEN;
}
