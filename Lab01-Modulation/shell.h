// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void getsUart0(USER_DATA *data);
void parseFields(USER_DATA *data);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
bool sameStr(const char str1[], const char str2[]);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
void ps(void);
void ipcs(void);
void kill(uint32_t pidK);
void pkill(char* processName);
void pi(bool on);
void preempt(bool on);
void sched(bool prioOn);
void pidof(char* name);
void run(char* name);
void busFaltTrig(void);
void usageFaltTrig(void);
void hardFaltTrig(void);
void mpuFaltTrig(void);
void pendsvTrig(void);
void test1(void);
void test2(void);
void shell(void);

#endif
