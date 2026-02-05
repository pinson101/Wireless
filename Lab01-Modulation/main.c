#include <clock.h>
#include <stdint.h>
#include <stdbool.h>

#include "clock.h"
#include "gpio.h"
#include "nvic.h"
#include "spi1.h"
#include "uart0.h"
#include "wait.h"
#include "CLI.h"

// Global Variables for I & Q
static volatile uint32_t mode_i, mode_q;
static volatile uint32_t freq_i, freq_q;
static volatile uint32_t phase_i, phase_q;

enum MODE
{
    OFF,
    DC,
    SINE,
    TONE,
    RAW
};

int main(void)
{
    // Init
    initSystemClockTo40Mhz();
    initUart0();
    setUart0BaudRate(115200, 40e6);

    USER_DATA data;
    char* buff;

    //shell
    while(1)
    {
        // command prompt
        putsUart0("\r\e[1;32m> \e[0m");

        getsUart0(&data);

        parseFields(&data);

        //Command to clear screen
        if(isCommand(&data, "clear", 0))
        {
            putsUart0("\033[H\033[J");
        }

        // Command to send raw value to DAC
        else if(isCommand(&data, "raw", 2))
        {
            char* iq = getFieldString(&data, 1);
            uint32_t R = getFieldInteger(&data, 2);
            buff = toAsciiDecimal(buff, R);
            putsUart0("RAW ");
            putsUart0(iq);
            putsUart0(buff);
            putcUart0('\n');
        }

        // Command to send DC voltage to DAC
        else if(isCommand(&data, "dc", 2))
        {
            char* iq = getFieldString(&data, 1);
            uint32_t V = getFieldInteger(&data, 2);
        }
        else
        {
            putsUart0("\r\e[0;91mInvalid command/args: ");
            putsUart0(data.buffer);
            putsUart0("\e[0m\n");
        }
    }
}
