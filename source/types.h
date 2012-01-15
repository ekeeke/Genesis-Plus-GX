#undef uint8
#undef uint16
#undef uint32
#undef int8
#undef int16
#undef int32

#define uint8  unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define int8  signed char
#define int16 signed short
#define int32 signed int

/* C89 compatibility */
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif
