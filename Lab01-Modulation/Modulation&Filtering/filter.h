/*
 * filter.h  –  Root-Raised-Cosine (RRC) pulse-shaping filter
 *
 * Name: Bryan Gonzalez  |  ID: 1001443032
 * Target: TM4C123GH6PM
 *
 * Responsibilities
 *   - Build RRC coefficients at runtime given samples-per-symbol
 *   - Maintain a circular IQ delay line (one entry per symbol)
 *   - Apply the filter: push one symbol, get one filtered output sample
 *   - Enable / disable flag read by the DAC ISR
 */

#ifndef FILTER_H_
#define FILTER_H_

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Filter parameters  (change here to tune)
 *---------------------------------------------------------------------------*/
#define RRC_ALPHA  0.35f   /* roll-off factor                                */
#define RRC_TAPS   33      /* filter length – must be odd                    */

/*---------------------------------------------------------------------------
 * Enable flag  (volatile – read by DAC ISR)
 *---------------------------------------------------------------------------*/
extern volatile bool rrc_enabled;

/*---------------------------------------------------------------------------
 * API
 *---------------------------------------------------------------------------*/

/*
 * rrcBuild  –  (re)compute RRC coefficients for sps samples-per-symbol.
 *   Call whenever sym_rate changes (with interrupts masked).
 */
void rrcBuild(uint32_t sps);

/*
 * rrcReset  –  zero the IQ delay line.
 *   Call before starting a new stream (with interrupts masked).
 */
void rrcReset(void);

/*
 * rrcApply  –  push a new symbol (in_i, in_q) into the delay line and
 *   return the filtered output via *out_i / *out_q.
 *   Called from the DAC ISR – must be fast.
 *   Both inputs and outputs are in the normalised range [-1.0, +1.0].
 */
void rrcApply(float in_i, float in_q, float *out_i, float *out_q);

#endif /* FILTER_H_ */
