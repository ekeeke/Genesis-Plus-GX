/******************************************************************************
 *
 * Nintendo Gamecube Zip Support
 *
 * Only partial support is included, in that only the first file within the archive
 * is considered to be a ROM image.
 ***************************************************************************/
#ifndef _UNZIP_H_
#define _UNZIP_H_

extern int IsZipFile (char *buffer);
int UnZipDVD (unsigned char *outbuffer, u64 discoffset, int length);
int UnZipSDCARD (unsigned char *outbuffer, char *filename);

#endif
