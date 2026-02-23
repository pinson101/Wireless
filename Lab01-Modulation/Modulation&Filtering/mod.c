///*
// * mod.c  –  Digital modulation implementation
// *
// * Name: Bryan Gonzalez  |  ID: 1001443032
// * Target: TM4C123GH6PM
// */
//
//#include <stdint.h>
//#include <stdbool.h>
//#include <math.h>
//
//#include "mod.h"
//#include "DAC.h"   /* DAC_RATE */
//
//#ifndef M_PI
//#define M_PI 3.14159265358979323846
//#endif
//
///*===========================================================================
// * Streaming state  (definitions of the externs declared in mod.h)
// *===========================================================================*/
//volatile bool     mod_streaming    = false;
//volatile MOD_t    mod_type         = MOD_BPSK;
//volatile uint32_t mod_sym_rate     = 1000;
//volatile uint32_t mod_samp_per_sym = DAC_RATE / 1000;   /* = 100 at default */
//volatile uint32_t mod_samp_count   = 0;
//volatile float    mod_cur_i        = 0.0f;
//volatile float    mod_cur_q        = 0.0f;
//
///*===========================================================================
// * Transmit data  –  64-byte repeating pseudo-random payload
// *===========================================================================*/
//#define DATA_BYTES 64
//
//static const uint8_t tx_data[DATA_BYTES] = {
//    0xA5,0x5A,0xFF,0x00,0xDE,0xAD,0xBE,0xEF,
//    0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
//    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
//    0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,
//    0xA5,0x5A,0xFF,0x00,0xDE,0xAD,0xBE,0xEF,
//    0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
//    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
//    0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10
//};
//
//static uint32_t bit_index = 0;   /* current bit position, MSB-first */
//
///*---------------------------------------------------------------------------
// * nextBit  –  pull one bit from tx_data (wraps at end)
// *---------------------------------------------------------------------------*/
//static uint8_t nextBit(void)
//{
//    uint32_t byte_idx = bit_index / 8;
//    uint32_t bit_pos  = 7 - (bit_index % 8);       /* MSB first */
//    bit_index = (bit_index + 1) % (DATA_BYTES * 8);
//    return (tx_data[byte_idx] >> bit_pos) & 0x01;
//}
//
///*---------------------------------------------------------------------------
// * nextNBits  –  pull n bits and pack into a uint8_t (MSB first)
// *---------------------------------------------------------------------------*/
//static uint8_t nextNBits(uint8_t n)
//{
//    uint8_t val = 0, i;
//    for (i = 0; i < n; i++)
//        val = (uint8_t)((val << 1) | nextBit());
//    return val;
//}
//
///*===========================================================================
// * Constellation mappers
// *   All return I/Q as integer steps; the caller normalises to [-1, +1].
// *===========================================================================*/
//
///* BPSK – 1 bit/symbol */
//static void bpskSymbol(uint8_t bits, int8_t *si, int8_t *sq)
//{
//    *si = (bits & 0x1) ? +1 : -1;
//    *sq = 0;
//}
//
///* QPSK – 2 bits/symbol, Grey coded
// *   00 → (+1,+1)   01 → (-1,+1)
// *   10 → (+1,-1)   11 → (-1,-1)
// */
//static void qpskSymbol(uint8_t bits, int8_t *si, int8_t *sq)
//{
//    *si = (bits & 0x2) ? -1 : +1;
//    *sq = (bits & 0x1) ? -1 : +1;
//}
//
///* 8-PSK – 3 bits/symbol, Grey coded, 8 equally-spaced phases
// *   Values are scaled ×100 so we can use int8_t without losing precision.
// *   The caller normalises by dividing by 100.
// */
//static void psk8Symbol(uint8_t bits, int8_t *si, int8_t *sq)
//{
//    static const uint8_t grey_to_bin[8] = {0, 1, 3, 2, 7, 6, 4, 5};
//    uint8_t idx   = grey_to_bin[bits & 0x7];
//    double  angle = (2.0 * M_PI * idx) / 8.0;
//    *si = (int8_t)lround(cos(angle) * 100.0);
//    *sq = (int8_t)lround(sin(angle) * 100.0);
//}
//
///* 16-QAM – 4 bits/symbol, Grey-coded rectangular
// *   2-bit grey map: 00→-3  01→-1  11→+1  10→+3
// */
//static void qam16Symbol(uint8_t bits, int8_t *si, int8_t *sq)
//{
//    static const int8_t map4[4] = {-3, -1, +3, +1};
//    *si = map4[(bits >> 2) & 0x3];
//    *sq = map4[ bits       & 0x3];
//}
//
///*===========================================================================
// * loadNextSymbol  –  ISR-callable; maps bits → mod_cur_i / mod_cur_q
// *===========================================================================*/
//void loadNextSymbol(void)
//{
//    int8_t si = 0, sq = 0;
//    float  scale;
//
//    switch (mod_type)
//    {
//        case MOD_BPSK:
//            bpskSymbol(nextNBits(1), &si, &sq);
//            scale = 1.0f;
//            break;
//
//        case MOD_QPSK:
//            qpskSymbol(nextNBits(2), &si, &sq);
//            scale = 1.0f / 1.4142135f;   /* 1/sqrt(2) – normalise to unit circle */
//            break;
//
//        case MOD_8PSK:
//            psk8Symbol(nextNBits(3), &si, &sq);
//            scale = 1.0f / 100.0f;       /* undo ×100 integer scaling            */
//            break;
//
//        case MOD_16QAM:
//        default:
//            qam16Symbol(nextNBits(4), &si, &sq);
//            scale = 1.0f / (3.0f * 1.4142135f); /* normalise ±3 → ±1             */
//            break;
//    }
//
//    mod_cur_i = (float)si * scale;
//    mod_cur_q = (float)sq * scale;
//}
//
///*===========================================================================
// * Shell-facing API
// *===========================================================================*/
//
//void modResetStream(void)
//{
//    bit_index      = 0;
//    mod_samp_count = 0;
//    mod_cur_i      = 0.0f;
//    mod_cur_q      = 0.0f;
//}
//
//bool modConfigure(MOD_t type, uint32_t rate_sym_s)
//{
//    if (rate_sym_s == 0 || rate_sym_s > DAC_RATE)
//        return false;
//
//    mod_type         = type;
//    mod_sym_rate     = rate_sym_s;
//    mod_samp_per_sym = DAC_RATE / rate_sym_s;
//    modResetStream();
//    return true;
//}
//
//void modStart(void)
//{
//    modResetStream();
//    mod_streaming = true;
//}
//
//void modStop(void)
//{
//    mod_streaming = false;
//}
