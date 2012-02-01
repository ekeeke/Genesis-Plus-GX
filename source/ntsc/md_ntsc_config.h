/* Configure library by modifying this file */

#ifndef MD_NTSC_CONFIG_H
#define MD_NTSC_CONFIG_H

/* Format of source pixels */
#ifdef USE_15BPP_RENDERING
#define MD_NTSC_IN_FORMAT MD_NTSC_RGB15
#else
#define MD_NTSC_IN_FORMAT MD_NTSC_RGB16
#endif
#define MD_NTSC_IN_FORMAT MD_NTSC_RGB16

/* Original CRAM format */
/* #define MD_NTSC_IN_FORMAT MD_NTSC_BGR9 */

/* Type of input pixel values */
#define MD_NTSC_IN_T unsigned short

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, etc. */
#define MD_NTSC_ADJ_IN( in ) in

/* For each pixel, this is the basic operation:
output_color = MD_NTSC_ADJ_IN( MD_NTSC_IN_T ) */

#endif
