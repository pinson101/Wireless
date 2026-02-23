/*
 * mod.h  –  Digital modulation (BPSK / QPSK / 8-PSK / 16-QAM)
 *
 * Name: Bryan Gonzalez  |  ID: 1001443032
 * Target: TM4C123GH6PM
 *
 * Responsibilities
 *   - MOD_t enum and constellation symbol mapping
 *   - Bit-stream sourcing from a fixed tx_data array
 *   - Streaming state (mod_type, sym_rate, samp_per_sym, samp_count)
 *   - loadNextSymbol() called by the DAC ISR each symbol period
 *   - modConfigure() / modStart() / modStop() called from the shell
 */

#ifndef MOD_H_
#define MOD_H_

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Modulation types
 *---------------------------------------------------------------------------*/
typedef enum {
    MOD_BPSK  = 0,
    MOD_QPSK,
    MOD_8PSK,
    MOD_16QAM
} MOD_t;

/*---------------------------------------------------------------------------
 * Streaming state  (read by DAC ISR – all volatile)
 *---------------------------------------------------------------------------*/
extern volatile bool     mod_streaming;    /* true while symbol stream runs  */
extern volatile MOD_t    mod_type;         /* current modulation scheme       */
extern volatile uint32_t mod_sym_rate;     /* symbols per second              */
extern volatile uint32_t mod_samp_per_sym; /* DAC samples per symbol          */
extern volatile uint32_t mod_samp_count;   /* sample counter within symbol    */
extern volatile float    mod_cur_i;        /* current symbol I (normalised)   */
extern volatile float    mod_cur_q;        /* current symbol Q (normalised)   */

/*---------------------------------------------------------------------------
 * API
 *---------------------------------------------------------------------------*/

/*
 * modConfigure  –  set modulation type and symbol rate.
 *   Call from the shell (interrupts must be masked by caller if streaming).
 *   Resets the bit-stream and sample counters.
 *   Returns false if rate is out of range (1 … DAC_RATE).
 */
bool modConfigure(MOD_t type, uint32_t rate_sym_s);

/*
 * modStart  –  reset counters and set mod_streaming = true.
 *   Call from the shell with interrupts masked.
 */
void modStart(void);

/*
 * modStop  –  set mod_streaming = false.
 */
void modStop(void);

/*
 * modResetStream  –  rewind bit-stream to byte 0.
 *   Useful when re-starting after a stop.
 */
void modResetStream(void);

/*
 * loadNextSymbol  –  called by the DAC ISR at the start of each symbol period.
 *   Reads the next N bits from the tx_data stream and maps them to
 *   mod_cur_i / mod_cur_q in the range [-1.0, +1.0].
 */
void loadNextSymbol(void);

#endif /* MOD_H_ */
