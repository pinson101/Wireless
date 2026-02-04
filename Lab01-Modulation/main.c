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
    SIN,
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
        putcUart0('>');

        getsUart0(&data);

        parseFields(&data);

        putsUart0("\r");

        if(isCommand(&data, "raw", 2))
        {
            char* iq = getFieldString(&data, 1);
            int32_t R = getFieldInteger(&data, 2);
            buff = toAsciiDecimal(buff, R);
            putcUart0('\n');
            putsUart0("RAW ");
            putsUart0(iq);
            putsUart0(buff);
        }
        else if(isCommand(&data, "dc", 2))
        {
            char* iq = getFieldString(&data, 1);
            int32_t V = getFieldInteger(&data, 2);
        }
        else
            putsUart0("INVALID");
        putsUart0("\r\n\n");
    }
}
