/*
 *  Name: Bryan Gonzalez
 *  ID: 1001443032
 *  Common Terminal Interface - header
 */

#include <stdint.h>
#include <stdbool.h>

#ifndef CTI_H_
#define CTI_H_

#define MAX_CHARS 80
#define MAX_FIELDS 5
typedef struct _USER_DATA
{
    char buffer[MAX_CHARS + 1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;

void getsUart0(USER_DATA *data);

void parseFields(USER_DATA *data);
char* getFieldString(USER_DATA *data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA *data, uint8_t fieldNumber);
bool isCommand(USER_DATA *data, const char strCommand[], uint8_t minArguments);
int32_t str_compare(const char *s1, const char *s2);

char* toAsciiDecimal(char *buff, uint32_t Val);
char* toAsciiHex(char *buff, uint32_t Val);

#endif /* DEBUG_H_ */
