#include <clock.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "clock.h"
#include "gpio.h"
#include "nvic.h"
#include "spi1.h"
#include "uart0.h"
#include "wait.h"
#include "CLI.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LUT_SIZE   4096u                    // Size of lookup tables
#define DAC_BITS   12u
#define DAC_MAX    ((1u << DAC_BITS) - 1u)  // 4095
#define DAC_MID    (DAC_MAX / 2u + 1u)      // 2048

// Global Variables for I & Q
// Unsure if all of these will be needed, but defining them all for now
static volatile uint32_t mode_i, mode_q;
static volatile uint32_t raw_i, raw_q;
static volatile uint32_t freq_i, freq_q;
static volatile uint32_t phase_i, phase_q;
static volatile uint32_t voltage_i, voltage_q;
static volatile uint32_t phase_acc;
static volatile uint32_t delta_phase;

enum MODE
{
    OFF,
    RAW,
    DC,
    SINE,
    TONE
};

//static uint16_t sin_lut[LUT_SIZE];
//static uint16_t cos_lut[LUT_SIZE];
//
//// Function to initialize sine and cosine lookup tables
//void initLUTs(void)
//{
//    for (uint32_t i = 0; i < LUT_SIZE; i++)
//    {
//        sin_lut[i] = (uint16_t)( ( (sin(2.0 * M_PI * i / LUT_SIZE) + 1.0) / 2.0 ) * DAC_MAX );
//        cos_lut[i] = (uint16_t)( ( (cos(2.0 * M_PI * i / LUT_SIZE) + 1.0) / 2.0 ) * DAC_MAX );
//    }
//}

int main(void)
{
    // Init
    initSystemClockTo40Mhz();
    initUart0();
    setUart0BaudRate(115200, 40e6); // 115200 bps
    initSpi1(0x0000000E);           // SCK, MOSI, CS as output
    setSpi1BaudRate(1e6, 40e6);     // 1 MHz
    setSpi1Mode(0,0);               // CPOL = 0, CPHA = 0
                                    // ^probably not necessary, should be mode 0 by default

    USER_DATA data;
    // char* buff;

    // shell
    while(1)
    {
        // command prompt (green arrow)
        putsUart0("\r\e[1;32m> \e[0m");

        getsUart0(&data);

        parseFields(&data);

        // Command to clear screen
        if(isCommand(&data, "clear", 0))
        {
            putsUart0("\033[H\033[J");
        }

        // Command to send raw value to DAC
        else if(isCommand(&data, "raw", 2))
        {
            char* iq = getFieldString(&data, 1);
            uint32_t R = getFieldInteger(&data, 2);

            if(str_compare(iq, "i") == 0 || str_compare(iq, "I") == 0)
            {
                raw_i = R;
                mode_i = RAW;
                putsUart0("\e[0;36mI RAW mode set\r\n");
                // Send R to I channel DAC via SPI1
                // This should go in the ISR, but doing it here for now to test SPI
                // DAC expects 16-bit data with control bits in upper 4 bits
                writeSpi1Data(0x1 << 12 | (R & 0xFFF));
            }
            else if(str_compare(iq, "q") == 0 || str_compare(iq, "Q") == 0)
            {
                raw_q = R;
                mode_q = RAW; 
                putsUart0("\e[0;36mQ RAW mode set\r\n");
                // Send R to Q channel DAC via SPI1
                // This should go in the ISR, but doing it here for now to test SPI
                // DAC expects 16-bit data with control bits in upper 4 bits
                writeSpi1Data(0x9 << 12 | (R & 0xFFF));
            }
            else
            {
                putsUart0("\r\e[0;91mInvalid I/Q specifier: ");
                putsUart0(iq);
                putsUart0("\e[0m\n");
            }
        }

        // Command to send DC voltage to DAC
        else if(isCommand(&data, "dc", 2))
        {
            char* iq = getFieldString(&data, 1);
            uint32_t V = getFieldInteger(&data, 2);

            if (str_compare(iq, "i") == 0 || str_compare(iq, "I") == 0)
            {
                voltage_i = V;
                mode_i = DC;
                putsUart0("\e[0;36mI DC voltage set\r\n");
            }
            else if (str_compare(iq, "q") == 0 || str_compare(iq, "Q") == 0)
            {
                voltage_q = V;
                mode_q = DC;
                putsUart0("\e[0;36mQ DC voltage set\r\n");
            }
            else
            {
                putsUart0("\r\e[0;91mInvalid I/Q specifier: ");
                putsUart0(iq);
                putsUart0("\e[0m\n");
            }
            
        }

        // Command to set sine wave parameters
        else if (isCommand(&data, "sine", 3) || isCommand(&data, "sine", 4))
        {
            char* iq = getFieldString(&data, 1);
            uint32_t amplitude = getFieldInteger(&data, 2);
            uint32_t frequency = getFieldInteger(&data, 3);
            uint32_t phase = (data.fieldCount == 5) ? getFieldInteger(&data, 4) : 0;

            if (str_compare(iq, "i") == 0 || str_compare(iq, "I") == 0)
            {
                voltage_i = amplitude;
                freq_i = frequency;
                phase_i = phase;
                mode_i = SINE;
                putsUart0("\e[0;36mI SINE wave set\r\n");
            }
            else if (str_compare(iq, "q") == 0 || str_compare(iq, "Q") == 0)
            {
                voltage_q = amplitude;
                freq_q = frequency;
                phase_q = phase;
                mode_q = SINE;
                putsUart0("\e[0;36mQ SINE wave set\r\n");
            }
            else
            {
                putsUart0("\r\e[0;91mInvalid I/Q specifier: ");
                putsUart0(iq);
                putsUart0("\e[0m\n");
            }
        }

        // Commmand to set tone parameters 
        else if (isCommand(&data, "tone", 2))
        {
            uint32_t amplitude = getFieldInteger(&data, 1);
            uint32_t frequency = getFieldInteger(&data, 2);

            voltage_i = amplitude;
            voltage_q = amplitude; 
            freq_i = frequency;
            freq_q = frequency;
            mode_i = TONE;
            mode_q = TONE;
            putsUart0("\e[0;36mTONE set\r\n");
        }


        // Command not recognized
        else
        {
            putsUart0("\r\e[0;91mInvalid command/args: ");
            putsUart0(data.buffer);
            putsUart0("\e[0m\n");
        }
    }
}
