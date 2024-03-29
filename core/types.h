#pragma once

#include <stdint.h>

typedef union
{
    uint16_t w;
    struct
    {
#ifdef LSB_FIRST
        uint8_t l;
        uint8_t h;
#else
        uint8_t h;
        uint8_t l;
#endif
    } byte;

} reg16_t;

