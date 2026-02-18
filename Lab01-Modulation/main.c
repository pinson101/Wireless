//#include <clock.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "tm4c123gh6pm.h"
#include "clock80.h"
#include "gpio.h"
#include "nvic.h"
#include "spi1.h"
#include "uart0.h"
#include "wait.h"
#include "CLI.h"
#include "DAC.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global Variables for I & Q
// Unsure if all of these will be needed, but defining them all for now
volatile MODE_t mode_i = OFF, mode_q = OFF;
volatile uint16_t raw_i = 2048, raw_q = 2048;
volatile uint32_t freq_i = 10000, freq_q = 10000;
volatile uint32_t phase_i = 0, phase_q = 0;
volatile float voltage_i = 0.0f, voltage_q = 0.0f;
volatile uint32_t phase_acci = 0, phase_accq = 0;
volatile uint32_t delta_phase = 1; // default step

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

int main(void)
{
    // Init
    void initSystemClockTo80Mhz();
    initUart0();
    setUart0BaudRate(115200, 80e6); // 115200 bps
    initSpi1(0x0000000F);           // SCK, MOSI, MISO, CS as output
    setSpi1BaudRate(1e6, 80e6);     // 1 MHz
    setSpi1Mode(0,0);               // CPOL = 0, CPHA = 0
                                    // ^probably not necessary, should be mode 0 by default
    enablePort(PORTE);
    selectPinPushPullOutput(LDAC);
    setPinValue(LDAC, 1);
    void initTimer1();              // Initialize Periodic Timer 1 to trigger ever 20us

    //-----------------------------------------------------------------------------
    // Manual Sine Generation
    makeLUT(500); // arg: Volts in mV


    //-----------------------------------------------------------------------------

    USER_DATA data;

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

        // Command to turn off I or Q channel
        else if (isCommand(&data, "off", 1))
        {
            char* iq = getFieldString(&data, 1);

            if (str_compare(iq, "i") == 0 || str_compare(iq, "I") == 0)
            {
                mode_i = OFF;
                putsUart0("\e[0;36mI channel turned off\r\n");
            }
            else if (str_compare(iq, "q") == 0 || str_compare(iq, "Q") == 0)
            {
                mode_q = OFF;
                putsUart0("\e[0;36mQ channel turned off\r\n");
            }
            else
            {
                putsUart0("\r\e[0;91mInvalid I/Q specifier: ");
                putsUart0(iq);
                putsUart0("\e[0m\n");
            }
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
                writeSpi1Data(0x3 << 12 | (R & 0xFFF));
                waitMicrosecond(30);
                setPinValue(LDAC, 0);
                waitMicrosecond(1);
                setPinValue(LDAC, 1);
            }
            else if(str_compare(iq, "q") == 0 || str_compare(iq, "Q") == 0)
            {
                raw_q = R;
                mode_q = RAW;
                putsUart0("\e[0;36mQ RAW mode set\r\n");
                // Send R to Q channel DAC via SPI1
                // This should go in the ISR, but doing it here for now to test SPI
                // DAC expects 16-bit data with control bits in upper 4 bits
                writeSpi1Data(0xB << 12 | (R & 0xFFF));
                waitMicrosecond(30);
                setPinValue(LDAC, 0);
                waitMicrosecond(1);
                setPinValue(LDAC, 1);
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

        // Command to show help message
        else if (isCommand(&data, "help", 0))
        {
            putsUart0("\e[0;36mAvailable commands:\r\n");
            putsUart0("  \e[0;36mclear\r\n"
                      "    \e[0m- Clear the terminal screen\r\n");
            putsUart0("  \e[0;36moff [i/q] \r\n"
                      "    \e[0m- Turn off I or Q channel\r\n");
            putsUart0("  \e[0;36mraw [i/q] [R] \r\n"
                      "    \e[0m- Set I or Q channel to raw value R (0 - 4095)\r\n");
            putsUart0("  \e[0;36mdc [i/q] [V] \r\n"
                      "    \e[0m- Set I or Q channel to DC voltage V (-0.5 - 0.5)\r\n");
            putsUart0("  \e[0;36msine [i/q] [A] [f] [p] \r\n"
                      "    \e[0m- Set I or Q channel to sine wave with amplitude A (0 - 0.5), frequency f (Hz), and optional phase p (degrees)\r\n");
            putsUart0("  \e[0;36mtone [A] [f] \r\n"
                      "    \e[0m- Set both channels to tone with amplitude A (0-4095) and frequency f (Hz)\r\n");
            putsUart0("  \e[0;36mhelp \r\n"
                      "    \e[0m- Show this help message\r\n");
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
