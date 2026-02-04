/*
 *  Name: Bryan Gonzalez
 *  ID: 1001443032
 *  Common Terminal Interface - header
 */

#include <stdint.h>
#include <stdbool.h>
#include "uart0.h"
#include "CLI.h"

/*
 * This is a function to receive characters from the user interface, processing special
 * characters such as backspace and writing the resultant string into the buffer
 */
void getsUart0(USER_DATA *data)
{
    int count = 0;
    char c;

    while (1)
    {
        c = getcUart0();

        if (count > 0 && (c == 8 || c == 127)) // Backspace
        {
            count--;
        }
        else if (c == 13) // carriage return
        {
            data->buffer[count] = '\0';
            return;
        }
        else if (c >= 32) // space or character
        {
            data->buffer[count] = c;
            count++;

            if (count == MAX_CHARS) //str is full
            {
                data->buffer[count] = '\0';
                return;
            }
        }
    }
}

/*
 * This is a function that takes the buffer string from the getsUart0() function and
 * processes the string in-place and returns information about the parsed fields in
 * fieldCount, fieldPosition, and fieldType.
 */
void parseFields(USER_DATA *data)
{
    char type = 'd'; //start set as d for delimiter
    uint32_t i = 0;
    data->fieldCount = 0;
    while (data->buffer[i] != '\0')
    {
        char c = data->buffer[i];
        if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z')
        {
            if (type == 'd')
            {
                type = 'a'; //set as 'a' for alphabetical
                data->fieldType[data->fieldCount] = type;
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
                if (data->fieldCount == MAX_FIELDS)
                    return;
            }
        }
        else if (c >= '0' && c <= '9')
        {
            if (type == 'd')
            {
                type = 'n';     //set as 'n' for numeric
                data->fieldType[data->fieldCount] = type;
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
                if (data->fieldCount == MAX_FIELDS)
                    return;
            }
        }
        else
        {
            data->buffer[i] = 0;
            type = 'd';         //reset delimiter
        }

        i++;
    }
}

/*
 * Returns the value of a field requested if the field number is in range or NULL otherwise.
 */
char* getFieldString(USER_DATA *data, uint8_t fieldNumber)
{
    if (fieldNumber < data->fieldCount)
    {
        uint32_t i = data->fieldPosition[fieldNumber];
        return data->buffer + i;
    }
    else
        return 0;

}

/*
 * Returns the integer value of the field if the field number is in range and the field type is
 * numeric or 0 otherwise.
 */
int32_t getFieldInteger(USER_DATA *data, uint8_t fieldNumber)
{
    if (fieldNumber < data->fieldCount && data->fieldType[fieldNumber] == 'n')
    {
        uint32_t val = 0, i = 0;
        char *str = getFieldString(data, fieldNumber);

        if (str[0] == '0' && str[1] == 'x') //Convert from Hex
        {
            i = 2;
            while (str[i] != 0)
            {
                val = val << 4;

                if (str[i] >= '0' && str[i] <= '9')
                    val += (int32_t) str[i] - (int32_t) '0';

                else if (str[i] >= 'a' && str[i] <= 'f')
                    val += (int32_t) str[i] - (int32_t) 'a' + 10;

                else if (str[i] >= 'A' && str[i] <= 'F')
                    val += (int32_t) str[i] - (int32_t) 'A' + 10;

                i++;
            }

        }
        else    //Convert from Decimal
        {
            while (str[i] != 0)
            {
                val *= 10;
                val += ((uint32_t) str[i] - 48);
                i++;
            }
        }

        return val;

    }
    else
        return 0;
}

/*
 * String Compare
 * compare s1 and s2
 * returns 0 if the are the same
 * return a value if the are different
 */
int32_t str_compare(const char *s1, const char *s2)
{
    while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return (*s1 - *s2);
}

int32_t str_lenght(const char *str)
{
    int32_t len = 0;
    while (*str != '\0')
    {
        len++;
        str++;
    }
    return len;
}

/*
 * This function returns true if the command matches the first field and the number of
 // arguments (excluding the command field) is greater than or equal to the requested
 // number of minimum arguments.
 */
bool isCommand(USER_DATA *data, const char strCommand[], uint8_t minArguments)
{
    if (str_compare(strCommand, data->buffer) != 0)
        return false;

    if (data->fieldCount - 1 >= minArguments)
        return true;
    else
        return false;
}

char* toAsciiHex(char *buff, uint32_t Val)
{
    uint32_t hex_digit = 0;
    int i;

    for(i = 0; i < 8; i++)
    {
        //Shifts bits in groups of 4 towards the lsb and isolates them using our 0xF flag
        hex_digit = Val >> (4 * (7-i));
        hex_digit &= 0xF;

        if(hex_digit < 10)  //hex value is 0-9
            buff[i] = 48 + hex_digit;
        else                //hex value is a-b (starts at 97 but we need to shift by 10 starting at a)
            buff[i] = 87 + hex_digit;
    }
    buff[8] = '\0';

    return buff;
}

char* toAsciiDecimal(char *buff, uint32_t Val)
{
    uint32_t digit = 0;
    int i;

    for(i = 14; i >= 0; i--)
    {
        if(Val == 0)
            buff[i] = ' ';
        else
        {
            digit = Val % 10;
            Val /= 10;
            buff[i] = '0' + digit;
        }

    }
    buff[15] = '\0';

    return buff;
}

