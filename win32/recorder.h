/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  recorder.h -- audio and video+audio recording.
 *
 *  Audio only      -> 16-bit stereo PCM .wav
 *  Video + audio   -> .mp4 (H.264 + AAC) through the encoders built into
 *                     Windows (Media Foundation). Media Foundation is loaded
 *                     at run time, so the program still starts on systems
 *                     that don't have it (Windows "N" editions without the
 *                     Media Feature Pack); recording video just reports an
 *                     error there.
 *
 *  Encoding runs on its own thread, fed through a small queue, so the
 *  emulation doesn't stall on the encoder. Timestamps come from the number of
 *  emulated frames and audio samples, not the clock, so video and audio stay
 *  in step whatever the emulation speed.
 ****************************************************************************/

#ifndef _RECORDER_H_
#define _RECORDER_H_

#define REC_NONE   0
#define REC_AUDIO  1   /* .wav */
#define REC_VIDEO  2   /* .mp4, video + audio */

/* Starts recording to `path`. `sample_rate` is the emulator's audio rate
   (44100 or 48000), `fps` the emulated frame rate. Returns 1 on success;
   on failure returns 0 and recorder_error() says why. */
extern int  recorder_start(int mode, const char *path, int sample_rate, double fps);

/* Finishes the file (blocks until it is complete). Safe to call when idle. */
extern void recorder_stop(void);

/* Call once per emulated frame with that frame's audio (interleaved 16-bit
   stereo, `frames` sample pairs). In video mode this also takes the current
   picture. */
extern void recorder_capture(const short *samples, int frames);

extern int         recorder_active(void);      /* REC_NONE / REC_AUDIO / REC_VIDEO */
extern int         recorder_failed(void);      /* the encoder hit an error mid-recording */
extern const char *recorder_error(void);
extern const char *recorder_path(void);
extern unsigned    recorder_seconds(void);     /* length recorded so far */

/* Provided by video.c: the emulator's current, unprocessed frame (RGB565). */
extern int video_capture_frame(const unsigned short **pixels, int *pitch, int *w, int *h);

#endif
