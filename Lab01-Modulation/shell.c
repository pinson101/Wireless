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
#include "tm4c123gh6pm.h"
#include "shell.h"
#include "uart0.h"
#include "mm.h"
#include "faults.h"
#include "tasks.h"
#include "kernel.h"

// REQUIRED: Add header files here for your strings functions, ...

// Info that can be accepted
#define MAX_CHARS 80
#define MAX_FIELDS 5
#define longestCommand 7

// UI info structure
typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Function that stores the inputed characters
void getsUart0(USER_DATA *data)
{
    int count = 0;
    while (count < MAX_CHARS)
    {
        while (!kbhitUart0())
        {
            yield();
        }

        // get the character input and store in variable
        char c = getcUart0();
        putcUart0(c);

        // backspace and not first char
        if ((c == 8 || c == 127) && count > 0)
        {
            count --;
        }

        // carriage return
        else if (c == 13)
        {
            break;
        }

        // space bar or printable characters
        else if (c >= 32)
        {
            data->buffer[count] = c;
            count ++;
        }

    }
    data->buffer[count] = 0;
    return;
}

void parseFields(USER_DATA *data)
{
    data->fieldCount = 0;

    char current;
    char prev = '\0';

    int i = 0;
    for (i = 0; i < MAX_CHARS && data->buffer[i] != '\0' && data->fieldCount < MAX_FIELDS; i ++)
    {
        bool alpha = (data->buffer[i] > 64 && data->buffer[i] < 91) || (data->buffer[i] > 96 && data->buffer[i] < 123);
        bool numeric = data->buffer[i] > 47 && data->buffer[i] < 58;

        current = data->buffer[i];

        if(prev == '\0' && current != '\0')
                {
                    if(alpha)
                    {
                        data->fieldPosition[data->fieldCount] = i;
                        data->fieldType[data->fieldCount] = 'a';
                        data->fieldCount++;
                    }
                    else if(numeric)
                    {
                        data->fieldPosition[data->fieldCount] = i;
                        data->fieldType[data->fieldCount] = 'n';
                        data->fieldCount++;
                    }
                }
                if(!(alpha || numeric))
                {
                    data->buffer[i] = '\0';
                    current = '\0';
                }
                prev = current;
    }
    return;
}

// function to return the value of a field requested if the field number is in range or NULL otherwise.
char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
    if (fieldNumber <= data->fieldCount)
    {
        return &data->buffer[data->fieldPosition[fieldNumber]];
    }
    else
    {
        return NULL;
    }

}

// function to return the integer value of the field if the field number is in range and the field type is numeric or 0 otherwise.
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{
    // Check if fieldNumber is within range and if the field type is numeric
        if (fieldNumber > data->fieldCount || data->fieldType[fieldNumber] != 'n')
        {
            return 0; // Return NULL if fieldNumber is out of range or non numeric
        }
        else
        {
            //char* str = &data->buffer[data->fieldPosition[fieldNumber]]; //get the field addy
            return atoi(&data->buffer[data->fieldPosition[fieldNumber]]);
        }
}

// check if strings are equal (not case sensitive)
bool sameStr(const char *str1, const char *str2)
{
    while (*str1 && *str2)
    {
        char str1Lower, str2Lower;
        if (*str1 > 64 && *str1 < 91) str1Lower = *str1 + 32;  // if upper case turn lower
        else str1Lower = *str1;
        if (*str2 > 64 && *str2 < 91) str2Lower = *str2 + 32;  // if upper case turn lower
        else str2Lower = *str2;
        if (str1Lower != str2Lower) return false;
        str1++;
        str2++;
    }
    return (*str1 == *str2); // loop ended cause one reached null, if both ended at null and didn't fail in the loop then they are equal
}

// function which returns true if the command matches the first field and the number of arguments (excluding the command field) is greater than or equal to the requested number of minimum arguments.
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
    // check if command matches the first field
    char* firstField = getFieldString(data, 0);

    if (!sameStr(strCommand, firstField))
    {
        return false;
    }

    // check if number of arguments (minus command) is >= requested min arguments
    if ((data->fieldCount - 1) < minArguments)
    {
        return false;
    }

    return true; // if it hasn't returned false then it has a matching command and enough args

}


//------------------------------------------------------------------------------------------------------------------------------------------------------
// OS Functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

void ps(void)
{
    putsUart0("ps called");
}

void ipcs(void)
{
    putsUart0("ipcs called");
}

void kill(uint32_t pidK)
{
    putsUart0("pid ");
    putsUart0(uitoa(pidK));
    putsUart0(" killed");
}
void pkill(char* processName)
{
    putsUart0(processName);
    putsUart0(" killed");
}
void pi(bool on)
{
    if (on)
    {
        putsUart0("pi on");
    }
    if (!on)
    {
        putsUart0("pi off");
    }
}
void preempt(bool on)
{
    if (on)
    {
        putsUart0("preempt on");
    }
    if (!on)
    {
        putsUart0("preempt off");
    }
}
void sched(bool prioOn)  // true = priority scheduling, false = round robin scheduling
{
    if (prioOn)
    {
        putsUart0("sched prio");
    }
    if (!prioOn)
    {
        putsUart0("sched rr");
    }
}
void pidof(char *name)
{
    putsUart0(name);
    putsUart0(" launched");
}
void run(char *name)
{
    if (sameStr(name, "blue"))
        setPinValue(BLUE_LED, 1); // test function turning red led on
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Fault Trigger Functions (bus, usage, hard, mpu, pendsv)
//------------------------------------------------------------------------------------------------------------------------------------------------------

void busFaltTrig() // works
{
    // try accessing an invalid peripheral address
    uint32_t* p = malloc_heap(4000);      //fill r1
    if (!p) putsUart0("malloc failed\n");

    uint32_t* s = malloc_heap(8000);      //fill r2
    if (!s) putsUart0("malloc failed\n");

    uint32_t* q = malloc_heap(8000);      // fill r3
    if (!q) putsUart0("malloc failed\n");

    uint32_t* k = malloc_heap(7000);      // r4
    if (!k) putsUart0("malloc failed\n");

    uint32_t* a = malloc_heap(1000);      // r4
    if (!1) putsUart0("malloc failed\n");

    free_heap(p);
    free_heap(s);
    free_heap(q);
    free_heap(k);

    uint32_t* ptr = (uint32_t *) 0xFFFFFFFC;
    uint32_t val = *ptr;
}

void usageFaltTrig()
{
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;      // disable the divide by 0 trap
    volatile int zero = 0;               // volatile so the compiler can’t fold it
    volatile int val = 23 / zero;        // UsageFault now
    (void)val;
}

void hardFaltTrig()
{
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_USAGE;    // disable usage fault handler
    usageFaltTrig();
}

void mpuFaltTrig()
{
    setPrivOff();
    // read from heap without malloc
    uint32_t* p = (uint32_t *)0x20005000;
}

void pendsvTrig()
{
    NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
// MPU Tests
//------------------------------------------------------------------------------------------------------------------------------------------------------

//unpriv r/w pass
void test1()
{
    // in priv, malloc stuff
    // witch to unpriv, test to see if you can write to those areas in memory

    uint32_t* p = malloc_heap(4000);      //fill r1
    if (!p) putsUart0("malloc failed\n");

    uint32_t* s = malloc_heap(8000);      //fill r2
    if (!s) putsUart0("malloc failed\n");

    uint32_t* q = malloc_heap(8000);      // fill r3
    if (!q) putsUart0("malloc failed\n");

    uint32_t* k = malloc_heap(7000);      // r4
    if (!k) putsUart0("malloc failed\n");

    uint32_t* a = malloc_heap(1000);      // very last block
    if (!a) putsUart0("malloc failed\n");

    free_heap(p);
    free_heap(s);
    free_heap(q);
    free_heap(k);

    dumpHeap(); // prints the block table

    // Dereference the pointer and write to the address (*p = value)
    // While in privileged mode, verify you can still access ram in the allocated range of SRAM.
    setPrivOff();

    *a = 0xB00B;
    uint32_t val = *a;
    putsUart0("Success!!");
}

//unpriv r/w fail
void test2()
{
    // in priv, malloc stuff
    // free them
    // switch to unpriv and try to write to them (should fail)

    //In unprivileged mode, dereference the pointer and write to the address (*p = value) and verify there is now a fault.

    uint32_t* p = malloc_heap(4000);      //fill r1
    if (!p) putsUart0("malloc failed\n");

    uint32_t* s = malloc_heap(8000);      //fill r2
    if (!s) putsUart0("malloc failed\n");

    uint32_t* q = malloc_heap(8000);      // fill r3
    if (!q) putsUart0("malloc failed\n");

    uint32_t* k = malloc_heap(7000);      // r4
    if (!k) putsUart0("malloc failed\n");

    uint32_t* a = malloc_heap(1000);      // very last block
    if (!a) putsUart0("malloc failed\n");

    putsUart0("malloced heap: \n");
    dumpHeap(); // prints the block table

    free_heap(p);
    free_heap(s);
    free_heap(q);
    free_heap(k);
//    free_heap(a);

    putsUart0("freed heap: \n");
    dumpHeap(); // prints the block table

    setPrivOff();

    *k = 0xB00B;
    uint32_t val = *k;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Shell Function (mother? mom? mamacita)
//------------------------------------------------------------------------------------------------------------------------------------------------------

void shell(void)
{
    USER_DATA data;
    while(true)
    {
        putsUart0("> ");
        //get string from userCall allowFlashAccess(),
        getsUart0(&data);
        //parse fields
        parseFields(&data);
        //malloc testing
        uint32_t* p;

        if(isCommand(&data,"reboot",0))
        {
            // reboot later
        }
        else if(isCommand(&data,"ps",0))
        {
            ps();
        }
        else if(isCommand(&data,"ipcs",0))
        {
            ipcs();
        }
        else if(isCommand(&data,"kill",1))
        {
            int32_t pidK = getFieldInteger(&data, 1);
            kill(pidK);
        }
        else if(isCommand(&data,"pkill",1))
        {
            char* processName = getFieldString(&data, 1);
            pkill(processName);
        }
        else if (isCommand(&data, "pi", 1))
        {
            // Turns priority inheritance on or off
            char* OnOff = getFieldString(&data, 1);

            if (sameStr(OnOff, "on"))
            {
                pi(true);
            }
            else if (sameStr(OnOff, "off"))
            {
                pi(false);
            }
            else
            {
                putsUart0("invalid on|off field");
            }
        }
        else if (isCommand(&data, "preempt", 1))
        {
            // Turns preemption on or off
            char* OnOff = getFieldString(&data, 1);

            if (sameStr(OnOff, "on"))
            {
                preempt(true);
            }
            else if (sameStr(OnOff, "off"))
            {
                preempt(false);
            }
            else
            {
                putsUart0("invalid on|off field");
            }
        }
        else if (isCommand(&data, "sched", 1))
        {
            // either priority or round robin scheduling
            char* prioRR = getFieldString(&data, 1);
            bool prioOn;

            if (sameStr(prioRR, "prio"))
            {
                prioOn = true;
                sched(prioOn);
            }
            else if (sameStr(prioRR, "rr"))
            {
                prioOn = false;
                sched(prioOn);
            }
            else
                putsUart0("invalid prio|rr field");
        }
        else if (isCommand(&data, "pidof", 1))
        {
            char* name = getFieldString(&data, 1);
            pidof(name);
        }
        else if (isCommand(&data, "run", 1))
        {
            char* name = getFieldString(&data, 1);
            run(name);
        }
        else if (isCommand(&data, "trig", 1)) // trigger fault ISRs
        {
            char* fault = getFieldString(&data, 1);
            if      (sameStr(fault, "bus"))    busFaltTrig();
            else if (sameStr(fault, "usage"))  usageFaltTrig();
            else if (sameStr(fault, "hard"))   hardFaltTrig();
            else if (sameStr(fault, "mpu"))    mpuFaltTrig();
            else if (sameStr(fault, "pendsv")) pendsvTrig();
            else
                putsUart0("Invalid. Trigger options: bus, usage, hard, mpu, pendsv");
        }
        else if (isCommand(&data, "malloc", 1)) // malloc size
        {
            uint32_t size = atoi(getFieldString(&data, 1));
            p = malloc_heap(size);
            if (!p) putsUart0("invalid\n");
            else putsUart0("success!\n");
        }
        else if (isCommand(&data, "dumpHeap", 0))
        {
            //dumpHeap();
        }
        else if (isCommand(&data, "free", 0))
        {
            free_heap(p);
        }
        else if (isCommand(&data, "test1", 0))
        {
            test1();
        }
        else if (isCommand(&data, "test2", 0))
        {
            test2();
        }
        else if (isCommand(&data, "debugR", 1))
        {
            uint32_t region = atoi(getFieldString(&data, 1));
            NVIC_MPU_NUMBER_R = region;
            volatile uint32_t regionReg = NVIC_MPU_ATTR_R;
            putsUart0(inttohex(regionReg));
        }
        else
        {
            putsUart0("invalid command");
        }
        putcUart0('\n');
    }
}
