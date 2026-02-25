// Assembly function library
// Angelina Abuhilal

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef ASM_H_
#define ASM_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint32_t *getPsp(void);
uint32_t *getMsp(void);
uint32_t  getControl(void);
uint32_t  getIpsr(void);
void setPsp(uint32_t *psp);
void setAspOn(void);
void setAspOff(void);
void setPrivOff(void);
void setPrivOn(void);

#endif