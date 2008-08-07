/*
 *  Wii DVD interface API
 *  Copyright (C) 2008 Jeff Epler <jepler@unpythonic.net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef WII_DVD
#include <ogc/ipc.h>
#include <string.h>
#include <ogc/system.h>
#include "wdvd.h"

static int di_fd = -1;
static s32 di_hid = 0;



#include <stdio.h>
#define DEBUG

#ifdef DEBUG
#define debug_printf printf
#define debug_wait printf
#else
#define debug_printf(...) ;
#define debug_wait(...) ;
#endif

bool WDVD_Init() 
{ 
    if(di_fd >= 0) return 1;

    di_fd = IOS_Open("/dev/do", 0);
    if(di_fd < 0) {
        debug_printf("IOS_Open(/dev/di) failed with code %d\n", di_fd);
        return 0;
    }
    
    di_hid = iosCreateHeap(0x10000);
    if(!di_hid) {
        IOS_Close(di_fd);
        di_fd = -1;
        debug_printf("iosCreateHeap(0x20) failed with code %d\n", di_hid);
        return 0;
    }

    debug_printf("fd=%d hid=%d\n", di_fd, di_hid);
    return 1;
}

bool WDVD_Reset() 
{
	bool result = false;
    char *inbuf = (char*)iosAlloc(di_hid, 0x20);
    char *outbuf = (char*)iosAlloc(di_hid, 0x20);
    if(!inbuf || !outbuf) 
		goto out;

    ((u32*)inbuf)[0x00] = 0x8A000000;
    ((u32*)inbuf)[0x01] = 1;

    result = IOS_Ioctl( di_fd, 0x8A, inbuf, 0x20, outbuf, 0x20);
out:
    if(outbuf) iosFree(di_hid, outbuf);
    if(inbuf)  iosFree(di_hid, inbuf);
    return result;
}

#define max(a,b) ((a) > (b) ? (a) : (b))

unsigned char* ios_command = 0;
unsigned char* ios_out = 0;
unsigned int ios_outlen = 0;

void WDVD_AllocCommand()
{
	if (ios_command == 0)
		ios_command = (unsigned char*)iosAlloc(di_hid, 0x20);
}

void WDVD_AllocOutput(unsigned int length)
{
	if (ios_out == 0 || ios_outlen != length)
	{
		if (ios_out)
			iosFree(di_hid, ios_out);

		ios_out = (unsigned char*)iosAlloc(di_hid, max(length, 0x20));
		ios_outlen = length;
	}
}

int WDVD_LowUnencryptedRead(unsigned char **poutbuf, u32 len, u32 offset) 
{
	WDVD_AllocCommand();
	WDVD_AllocOutput(len);

	unsigned char* inbuf = ios_command;
	unsigned char* outbuf = ios_out;

	*poutbuf = ios_out;
    int result = 0;

    if(!inbuf || !outbuf) { result = -1; goto out; }

    ((u32*)inbuf)[0] = 0x8d000000;
    ((u32*)inbuf)[1] = len;
    ((u32*)inbuf)[2] = offset;

//    memset(outbuf, 0x00, len);

    result = IOS_Ioctl(di_fd, 0x8d, inbuf, 0x20, outbuf, len);

	if (result != 1)
		debug_printf("-> %d\n", result);

//    if(result >= 0)  {
//        memcpy(buf, outbuf, len);
//    }

out:
    //if(outbuf) iosFree(di_hid, outbuf);
    //if(inbuf)  iosFree(di_hid, inbuf);

    return result;
}

int WDVD_LowReadDiskId(u64 *id) 
{
    int result;
    char *inbuf = (char*)iosAlloc(di_hid, 0x20);
    char *outbuf = (char*)iosAlloc(di_hid, 0x20);
    if(!inbuf || !outbuf) { result = -1; goto out; }
    ((u32*)inbuf)[0] = 0x70000000; 

	*(u64*)outbuf = 0;
    result = IOS_Ioctl(di_fd, 0x8D, inbuf, 0x20, outbuf, 0x20);

    *id = *(u64*) outbuf;

out:
    if(inbuf) iosFree(di_hid, inbuf);
    if(outbuf) iosFree(di_hid, outbuf);

    return result;
}
#endif
