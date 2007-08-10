
#include "shared.h"

static uint8 block[0x4000];

void deinterleave_block(uint8 *src)
{
    int i;
    memcpy(block, src, 0x4000);
    for(i = 0; i < 0x2000; i += 1)
    {
        src[i*2+0] = block[0x2000 + (i)];
        src[i*2+1] = block[0x0000 + (i)];
    }
}

int load_rom(char *filename)
{
    int size, offset = 0;
    uint8 header[0x200];
    uint8 *ptr;

    ptr = load_archive(filename, &size);
    if(!ptr) return (0);

    if((size / 512) & 1)
    {
        int i;

        size -= 512;
        offset += 512;

        memcpy(header, ptr, 512);

        for(i = 0; i < (size / 0x4000); i += 1)
        {
            deinterleave_block(ptr + offset + (i * 0x4000));
        }
    }

    memset(cart_rom, 0, 0x400000);
    if(size > 0x400000) size = 0x400000;
    memcpy(cart_rom, ptr + offset, size);

    /* Free allocated file data */
    free(ptr);

    return (1);
}



