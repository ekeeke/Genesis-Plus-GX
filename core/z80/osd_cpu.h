/*******************************************************************************
*                                                                              *
*  Define size independent data types and operations.                          *
*                                                                              *
*   The following types must be supported by all platforms:                    *
*                                                                              *
*  uint8_t  - Unsigned 8-bit Integer    INT8  - Signed 8-bit integer             *
*  uint16_t - Unsigned 16-bit Integer  INT16 - Signed 16-bit integer             *
*  uint32_t - Unsigned 32-bit Integer  INT32 - Signed 32-bit integer             *
*                                                                              *
*******************************************************************************/

#pragma once

#undef TRUE
#undef FALSE
#define TRUE  1
#define FALSE 0

/******************************************************************************
 * Union of uint8_t, uint16_t and uint32_t in native endianess of the target
 * This is used to access bytes and words in a machine independent manner.
 * The upper bytes h2 and h3 normally contain zero (16 bit CPU cores)
 * thus PAIR.d can be used to pass arguments to the memory system
 * which expects 'int' really.
 ******************************************************************************/

typedef union {
#ifdef LSB_FIRST
  struct { uint8_t l,h,h2,h3; } b;
  struct { uint16_t l,h; } w;
#else
  struct { uint8_t h3,h2,h,l; } b;
  struct { uint16_t h,l; } w;
#endif
  uint32_t d;
}  PAIR;

