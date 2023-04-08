

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;

typedef struct
{
    int64_t __val[2];
} int128_t;

typedef struct
{
    uint64_t __val[2];
} uint128_t;

void __android_log_print(int32_t prio, const char *tag, const char *fmt, ...);

void x(int32_t i, char i2, int32_t sequence, int32_t safe_key, int32_t forward, char *data, int32_t data_len, char *result, char *keyptr)
{
    uint64_t x24_1 = ((uint64_t) ((0x7f & ((0x8f & i2) | ((int8_t) ((7 & i) << 4)))) | ((int8_t) ((0x1ffffff & forward) << 7))));
    *(int32_t *) result = x24_1;
    memcpy(&result[1], data, data_len);

    uint64_t something;
    if (data_len + 4 < 1)
    {
        something = 0;
    }
    else
    {
        uint64_t x8_2 = ((uint64_t) data_len + 4);
        int64_t x9_1;
        if (data_len + 4 != 1)
        {
            int64_t x12_1 = 0;
            int32_t x10_1 = 0;
            int32_t x11_1 = 0;
            x9_1 = (x8_2 & 0xfffffffe);
            do
            {
                uint32_t x13;
                uint32_t x14_1 = ((uint32_t) * (int8_t *) ((char *) result + x12_1));
                if (x12_1 != 2)
                {
                    x13 = ((uint32_t) * (int8_t *) (((char *) result + x12_1) + 1));
                }
                bool cond_2_1 = x12_1 != 2;
                x12_1 = (x12_1 + 2);
                if ((!cond_2_1))
                {
                    x13 = 0;
                }
                x10_1 = (x14_1 + x10_1);
                x11_1 = (x11_1 + x13);
            } while (x9_1 != x12_1);
            something = ((uint64_t) (x11_1 + x10_1));
            if (x9_1 != x8_2)
            {
                goto label_15c24;
            }
        }
        else
        {
            x9_1 = 0;
            something = 0;
            do
            {
                something = ((uint64_t) (((uint32_t) * (int8_t *) ((char *) result + x9_1)) + something));
                do
                {
                    x9_1 = (x9_1 + 1);
                    if (x8_2 == x9_1)
                    {
                        break;
                    }
                label_15c24:;
                } while (x9_1 == 3);
            } while (x8_2 != x9_1);
        }
    }

    result[0] = x24_1 ^ 0x5e;
    result[1] = sequence ^ 0x36;
    result[2] = safe_key ^ 0x7b;
    result[3] = something ^ 0xc4;

    extern char HEADER_XOR_KEY;

    char *key = keyptr;
    if (key == 0)
        key = &HEADER_XOR_KEY;

    if (data_len > 0)
    {
        int64_t counter = 0;
        do
        {
            data[counter] = key[counter & 3] ^ data[counter];
            counter = (counter + 1);
        } while (data_len != counter);
    }

    memcpy(&result[4], data, data_len);
}
