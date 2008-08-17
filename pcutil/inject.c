/****************************************************************************
 * Genesis Plus 1.04
 *
 * Developer ROM injector.
 *
 * You should set ROMOFFSET to match ngc.c, as this will change as the 
 * binary expands and/or contracts.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOLHEADERLENGTH          256 // GC DOL Header Length
#define MAXTEXT                  7   // Maximum 7 Text Sections
#define MAXDATA                  11  // Maximum 11 Data Sections

struct DOLHEADER{ 
    unsigned int textOffsets[MAXTEXT];
    unsigned int dataOffsets[MAXDATA];
    
    unsigned int textAddress[MAXTEXT];
    unsigned int dataAddress[MAXDATA];
    
    unsigned int textLength[MAXTEXT];
    unsigned int dataLength[MAXDATA];
    
    unsigned int bssAddress;
    unsigned int bssLength;
    
    unsigned int entryPoint;
    unsigned int unused[MAXTEXT];	
} dolheader;

unsigned int FLIP32(unsigned int b)
{
	unsigned int c;

	c = ( b & 0xff000000 ) >> 24;
	c |= ( b & 0xff0000 ) >> 8;
	c |= ( b & 0xff00 ) << 8;
	c |= ( b & 0xff ) << 24;

	return c;
}

#define ROMOFFSET 0x80700000

int main( int argc, char *argv[] )
{
	FILE *infile, *outfile;
	char *dol;
	char *rom;
	int dollen, romlen, outlen;
	char *sig = "GENPLUSR";
	
	if ( argc != 4 )
	{
		printf("Usage : %s genplus.dol filename_of_your_rom.bin (or .smd) output.dol\n", argv[0]);
		return 1;
	}
	
	/*** Try to open all three handles ***/
	infile = fopen(argv[1], "rb");
	if ( infile == NULL )
	{
		printf("Unable to open %s for reading\n", argv[1]);
		return 1;
	}
	
	/*** Allocate and load ***/
	fseek(infile, 0, SEEK_END);
	dollen=ftell(infile);
	fseek(infile, 0, SEEK_SET);
	
	dol = calloc(sizeof(char), dollen + 32);
	
	if ( fread(dol, 1, dollen, infile ) != dollen )
	{
		free(dol);
		printf("Error reading %s\n", argv[1]);
		fclose(infile);
		return 1;
	}
	
	fclose(infile);
	
	infile = fopen(argv[2], "rb");
	if ( infile == NULL )
	{
		printf("Unable to open %s for reading\n", argv[2]);
		free(dol);
		return 1;
	}
	
	/*** Allocate and load ***/
	fseek( infile, 0, SEEK_END);
	romlen = ftell(infile);
	fseek( infile, 0, SEEK_SET);
	rom = calloc( sizeof(char), romlen + 48 );
	
	if ( fread(rom, 1, romlen, infile) != romlen )
	{
		printf("Error reading %s\n", argv[2]);
		fclose(infile);
		free(rom);
		free(dol);
		return 1;
	}
	
	fclose(infile);
	
	/*** Ok, now have both in memory - so update the dol header and get this file done -;) ***/
	memcpy(&dolheader, dol, DOLHEADERLENGTH);
	
	/*** Align to 32 bytes - no real reason, I just like it -;) ***/
	if ( dollen & 0x1f )
		dollen = ( dollen & ~0x1f ) + 0x20;

	dolheader.dataOffsets[1] = FLIP32(dollen);
	dolheader.dataAddress[1] = FLIP32(ROMOFFSET);
	dolheader.dataLength[1] = FLIP32(romlen + 32);
	
	/*** Move the updated header back ***/
	memcpy(dol, &dolheader, DOLHEADERLENGTH);
	
	outfile = fopen(argv[3], "wb");
	if ( outfile == NULL )
	{
		printf("Unable to open %s for writing!\n", argv[3]);
		free(rom);
		free(dol);
	}
	
	/*** Now simply update the files ***/
	fwrite(dol, 1, dollen, outfile);
	fwrite(sig, 1, 8, outfile);
	
	outlen = FLIP32(romlen);
	fwrite(&outlen, 1, 4, outfile);
	
	char align[32];
	memset(align, 0, 32);
	fwrite(align, 1, 20, outfile);
	fwrite(rom, 1, romlen, outfile);
	fclose(outfile);
	
	free(dol);
	free(rom);
	
	printf("Output file %s created successfully\n", argv[3]);
	
	return 0;
}

