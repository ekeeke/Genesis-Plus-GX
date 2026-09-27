/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  rewind.c -- holding Backspace steps back through recent gameplay,
 *  picture and sound both playing in reverse.
 *
 *  HISTORY OF THIS FILE'S APPROACH
 *  --------------------------------
 *  The first version stored only a save-state per slot, restoring the
 *  picture by state_load()-ing it and running one "corrective" forward
 *  frame to regenerate the framebuffer. That proved unreliable (confirmed
 *  directly with a hand-built test ROM: VDP/CRAM state was verifiably
 *  correct right after state_load(), yet the corrective frame's own
 *  rendered output stayed black regardless) and the root cause was never
 *  conclusively found, so it was replaced with storing the actual
 *  rendered frame (and its audio) directly per slot -- nothing needed
 *  regenerating, correctly or otherwise.
 *
 *  That fixed correctness but was expensive: a save-state (~1 MB) plus a
 *  full framebuffer (~810 KB) plus audio, uncompressed, per slot, capped
 *  history at a few hundred frames before memory became unreasonable.
 *
 *  This version keeps the same direct-restore approach -- still no
 *  corrective forward frame, still no risk of reviving that bug -- but
 *  compresses what's actually stored. Genesis Plus GX's own state and a
 *  rendered frame both change very little from one frame to the next
 *  (most of a save-state is ROM-mirror/unused-region padding that never
 *  changes at all; most of a picture is background that hasn't moved
 *  since last frame). RetroArch's rewind takes advantage of exactly this
 *  with a reverse delta: a run-length style patch that records only the
 *  words that actually changed between two snapshots, recorded backward
 *  so applying it to the newer one reconstructs the older one. The
 *  algorithm here (find_change/find_same/raw_compress/raw_decompress and
 *  the ring buffer around them) is a close port of RetroArch's
 *  state_manager.c, which is GPL-licensed the same as this project.
 *
 *  The state, the frame, the viewport, and the audio for a given instant
 *  are concatenated into one combined blob and compressed as a single
 *  unit -- simpler than juggling separate compression contexts for each,
 *  and it keeps all four permanently in sync: they're evicted from the
 *  ring together, as one record, never independently.
 ****************************************************************************/

#include <windows.h>
#include <string.h>
#include <stdint.h>

#include "shared.h"
#include "gui.h"
#include "netplay.h"

/* ==========================================================================
 * Combined per-frame blob layout
 * ========================================================================== */

#define REWIND_PIXEL_SIZE   (FRAME_MAX_W * FRAME_MAX_H * FRAME_BPP)

/* One video frame's worth of audio is roughly 735 stereo pairs at
   44100Hz/60fps (NTSC) or 882 at 50fps (PAL) -- 1024 covers either with
   margin. audio_update() is trusted to never hand back more than this in
   a single frame; captured audio beyond it is simply not stored rather
   than overflowing anything. */
#define REWIND_AUDIO_FRAMES 1024

/* Mirrors t_bitmap's anonymous viewport struct field-for-field. Needed
   because get_source_size() reads these to know the source dimensions
   to scale from, and they're not part of what state_save()/state_load()
   restores -- interlaced/im2_flag/odd_frame, the other fields that used
   to need this same treatment, are recomputed from reg[12] instead
   (reg[12] *is* part of the state), which is one less thing to carry
   in the blob. */
typedef struct { int x, y, w, h, ow, oh, changed; } saved_viewport_t;

#define BLOB_STATE_OFF     (0)
#define BLOB_PIXEL_OFF     (BLOB_STATE_OFF + STATE_SIZE)
#define BLOB_VIEWPORT_OFF  (BLOB_PIXEL_OFF + REWIND_PIXEL_SIZE)
#define BLOB_AUDCOUNT_OFF  (BLOB_VIEWPORT_OFF + sizeof(saved_viewport_t))
#define BLOB_AUDIO_OFF     (BLOB_AUDCOUNT_OFF + sizeof(int))
#define BLOB_SIZE          (BLOB_AUDIO_OFF + REWIND_AUDIO_FRAMES * 2 * sizeof(int16))

/* Total ring capacity -- the actual number of frames this holds depends
   entirely on how well a given game's state+picture compress from frame
   to frame, since records are variable-length, but this is sized to be
   comfortably in the same range as RetroArch achieves for comparable
   systems (tens of MB per minute of history, not hundreds). Two working
   blocks of BLOB_SIZE each (a few MB total) come on top of this for the
   compressor's own use. */
#define REWIND_RING_CAPACITY (192u * 1024u * 1024u)

/* Tail padding past each working block's logical end, in bytes. The
   scalar compare loops below read a full size_t/uint32_t at a time and
   don't stop until they hit a byte that differs, so the last word of a
   real comparison can start right at the logical end and still read a
   few bytes past it -- this pad, plus the sentinel word written just
   past the logical end (below), guarantees that read lands in memory
   this file actually owns and is guaranteed to eventually differ. */
#define BLOCK_SCAN_PAD 16

/* ==========================================================================
 * Delta compression -- ported from RetroArch's state_manager.c (GPL,
 * same license as this project). Scalar-only here (no SSE2/AVX2 scan
 * variants): a straight word-at-a-time compare over blobs this size, at
 * 60 times a second, is cheap enough on any x86 this port targets, and
 * skipping the SIMD paths keeps the port simple and avoids adding a CPU-
 * feature-detection dependency for what's a modest speed gain here.
 * ========================================================================== */

static size_t find_change(const uint16_t *a, const uint16_t *b)
{
  const uint16_t *a_org = a;
  const size_t *a_big = (const size_t *)a;
  const size_t *b_big = (const size_t *)b;

  while (*a_big == *b_big)
  {
    a_big++;
    b_big++;
  }
  a = (const uint16_t *)a_big;
  b = (const uint16_t *)b_big;

  while (*a == *b)
  {
    a++;
    b++;
  }
  return a - a_org;
}

static size_t find_same(const uint16_t *a, const uint16_t *b)
{
  const uint16_t *a_org = a;
  const uint32_t *a_big = (const uint32_t *)a;
  const uint32_t *b_big = (const uint32_t *)b;

  while (*a_big != *b_big)
  {
    a_big++;
    b_big++;
  }
  a = (const uint16_t *)a_big;
  b = (const uint16_t *)b_big;

  if (a != a_org && a[-1] == b[-1])
  {
    a--;
    b--;
  }
  return a - a_org;
}

/* Maximum possible compressed size of a blob of 'uncomp' bytes -- very
   likely to compress to far less, but the working patch buffer has to
   be sized for the worst case (a blob that's entirely different from
   the one it's being compared against). */
static size_t raw_maxsize(size_t uncomp)
{
  const size_t maxcblkcover = 65535u * sizeof(uint16_t);
  size_t uncomp16 = (uncomp + sizeof(uint16_t) - 1) & ~(size_t)(sizeof(uint16_t) - 1);
  size_t maxcblks = (uncomp + maxcblkcover - 1) / maxcblkcover;

  return uncomp16 + maxcblks * sizeof(uint16_t) * 2 + sizeof(uint16_t) * 3;
}

/* A reverse delta. Takes two blobs, 'src' the older and 'dst' the newer,
   and writes a patch that, applied to a copy of 'dst', restores 'src':
   the patch stores the old words of every run that differs, so a
   rewind step applies it to the current blob to get the previous one.
   Returns the number of bytes actually written to 'patch', which must
   be at least raw_maxsize(len). */
static size_t raw_compress(const void *src, const void *dst, size_t len, void *patch)
{
  const uint16_t *old16 = (const uint16_t *)src;
  const uint16_t *new16 = (const uint16_t *)dst;
  uint16_t *out = (uint16_t *)patch;
  size_t num16s = (len + sizeof(uint16_t) - 1) / sizeof(uint16_t);

  while (num16s)
  {
    size_t i, changed;
    size_t skip = find_change(old16, new16);

    if (skip >= num16s) break;

    old16 += skip;
    new16 += skip;
    num16s -= skip;

    if (skip > 65535u)
    {
      if (skip > 0xffffffffu) skip = 0xffffffffu;
      *out++ = 0;
      *out++ = (uint16_t)skip;
      *out++ = (uint16_t)(skip >> 16);
      continue;
    }

    changed = find_same(old16, new16);
    if (changed > 65535u) changed = 65535u;
    /* find_same() can run on into the padding after the blob when the very
       last words differ; never count more than is really left. */
    if (changed > num16s) changed = num16s;

    *out++ = (uint16_t)changed;
    *out++ = (uint16_t)skip;

    for (i = 0; i < changed; i++)
      out[i] = old16[i];

    old16 += changed;
    new16 += changed;
    num16s -= changed;
    out += changed;
  }

  out[0] = 0;
  out[1] = 0;
  out[2] = 0;

  return (uint8 *)(out + 3) - (uint8 *)patch;
}

/* Applies a patch from raw_compress() to 'data' (the 'dst' from that
   call), yielding 'src' from that call, in place. Two passes: the first
   walks every record checking it stays inside both the patch and the
   block; the second actually writes. A patch that fails the first pass
   changes nothing and returns false, rather than writing a corrupted
   ring past where it's supposed to stop. */
static int raw_decompress(const void *patch, size_t patch_len, void *data, size_t len)
{
  const uint16_t *patch16 = (const uint16_t *)patch;
  const uint16_t *p = patch16;
  size_t patch_words = patch_len / sizeof(uint16_t);
  size_t words = len / sizeof(uint16_t);
  size_t at = 0;
  uint16_t *out = (uint16_t *)data;

  for (;;)
  {
    uint16_t numchanged;
    if ((size_t)(p - patch16) >= patch_words) return 0;
    numchanged = *p++;
    if (numchanged)
    {
      uint16_t skip;
      if ((size_t)(p - patch16) + 1 + numchanged > patch_words) return 0;
      skip = *p++;
      if (skip > words - at) return 0;
      at += skip;
      if (numchanged > words - at) return 0;
      at += numchanged;
      p += numchanged;
    }
    else
    {
      uint32_t numunchanged;
      if ((size_t)(p - patch16) + 2 > patch_words) return 0;
      numunchanged = p[0] | ((uint32_t)p[1] << 16);
      if (!numunchanged) break;
      p += 2;
      if (numunchanged > words - at) return 0;
      at += numunchanged;
    }
  }

  for (p = patch16;;)
  {
    uint16_t numchanged = *p++;

    if (numchanged)
    {
      uint16_t i;
      out += *p++;
      for (i = 0; i < numchanged; i++) out[i] = p[i];
      p += numchanged;
      out += numchanged;
    }
    else
    {
      uint32_t numunchanged = p[0] | ((uint32_t)p[1] << 16);
      if (!numunchanged) break;
      p += 2;
      out += numunchanged;
    }
  }
  return 1;
}

/* ==========================================================================
 * Ring buffer around the compressor -- also a close port of
 * state_manager.c's approach: a byte ring holding variable-length
 * compressed records, oldest evicted first when a new one won't fit.
 * ========================================================================== */

static uint8 *ring_data;         /* REWIND_RING_CAPACITY bytes */
static uint8 *ring_head;         /* next record gets written here */
static uint8 *ring_tail;         /* oldest record still held starts here */
static size_t ring_maxcompsize;  /* raw_maxsize(BLOB_SIZE) + 2*sizeof(size_t) */
static unsigned ring_entries;

static uint8 *block_alloc;       /* single allocation backing both blocks below */
static uint8 *thisblock;         /* the newest full blob pushed/popped so far */
static uint8 *nextblock;         /* scratch: the blob being built for the next push */
static size_t block_alloc_size;  /* per-block allocation, incl. sentinel + pad */
static int    thisblock_valid;

static int    ring_ok;

static int    suppress_next_capture;

static int16  reversed_audio[REWIND_AUDIO_FRAMES * 2];
static int    reversed_audio_frames;

static void write_size_t(void *ptr, size_t val) { memcpy(ptr, &val, sizeof(val)); }
static size_t read_size_t(const void *ptr)
{
  size_t v;
  memcpy(&v, ptr, sizeof(v));
  return v;
}

static int ring_init(void)
{
  size_t single_block;

  ring_maxcompsize = raw_maxsize(BLOB_SIZE) + sizeof(size_t) * 2;

  /* ring_push_do()'s eviction loop assumes capacity can always
     eventually hold at least one full record once enough is evicted --
     true with room to spare at any realistic capacity, but checked
     explicitly rather than left as an unstated assumption a future
     change to BLOB_SIZE or the capacity constant could silently break
     into an infinite loop. */
  if (REWIND_RING_CAPACITY < ring_maxcompsize * 4) return 0;

  ring_data = (uint8 *)malloc(REWIND_RING_CAPACITY);
  if (!ring_data) return 0;

  /* Each block: BLOB_SIZE rounded to uint16_t alignment, a sentinel
     uint16_t just past that (compare loops terminate here if nothing
     real ever differs first -- thisblock's sentinel is 0, nextblock's
     is 1, so they can never compare equal to each other), then scan
     pad so the scalar readahead never leaves memory this file owns. */
  single_block = ((BLOB_SIZE + 1) & ~(size_t)1) + sizeof(uint16_t) + BLOCK_SCAN_PAD;
  block_alloc_size = single_block;
  block_alloc = (uint8 *)calloc(single_block * 2, 1);
  if (!block_alloc)
  {
    free(ring_data);
    ring_data = NULL;
    return 0;
  }

  thisblock = block_alloc;
  nextblock = block_alloc + single_block;
  *(uint16_t *)(thisblock + (single_block - BLOCK_SCAN_PAD - sizeof(uint16_t))) = 0;
  *(uint16_t *)(nextblock + (single_block - BLOCK_SCAN_PAD - sizeof(uint16_t))) = 1;

  ring_head = ring_data + sizeof(size_t);
  ring_tail = ring_data + sizeof(size_t);
  ring_entries = 0;
  thisblock_valid = 0;

  return 1;
}

/* Returns the oldest still-held blob, decompressing it into thisblock in
   place. Mirrors state_manager_pop(): the very first pop after a push
   just hands back the full blob that push already had on hand (no
   decompression needed yet), same as RetroArch's thisblock_valid path. */
static int ring_pop(uint8 **out)
{
  size_t start;
  const uint8 *compressed;

  *out = thisblock;

  if (thisblock_valid)
  {
    thisblock_valid = 0;
    ring_entries--;
    return 1;
  }

  if (ring_head == ring_tail) return 0;

  start = read_size_t(ring_head - sizeof(size_t));
  if (start + sizeof(size_t) > REWIND_RING_CAPACITY) return 0;
  compressed = ring_data + start + sizeof(size_t);

  if (!raw_decompress(compressed,
        (size_t)(ring_head - sizeof(size_t) - compressed), thisblock, BLOB_SIZE))
    return 0;

  ring_head = ring_data + start;
  ring_entries--;
  return 1;
}

/* Returns the scratch buffer the caller should build the next blob into. */
static uint8 *ring_push_where(void)
{
  if (!thisblock_valid)
  {
    uint8 *ignored;
    if (ring_pop(&ignored))
    {
      thisblock_valid = 1;
      ring_entries++;
    }
  }
  return nextblock;
}

/* Commits the blob ring_push_where()'s caller just built into nextblock:
   compresses it against thisblock (the previous full blob) and appends
   the patch to the ring, evicting the oldest record(s) if needed to fit,
   then swaps thisblock/nextblock so the just-pushed blob becomes the
   reference point for the next push. */
static void ring_push_do(void)
{
  uint8 *swap;

  if (thisblock_valid)
  {
    uint8 *compressed;
    size_t headpos, tailpos, remaining;

    for (;;)
    {
      headpos = ring_head - ring_data;
      tailpos = ring_tail - ring_data;
      remaining = (tailpos + REWIND_RING_CAPACITY - sizeof(size_t) - headpos - 1)
                  % REWIND_RING_CAPACITY + 1;

      if (remaining > ring_maxcompsize) break;

      ring_tail = ring_data + read_size_t(ring_tail);
      ring_entries--;
    }

    compressed = ring_head + sizeof(size_t);
    compressed += raw_compress(thisblock, nextblock, BLOB_SIZE, compressed);

    if ((size_t)(compressed - ring_data) + ring_maxcompsize > REWIND_RING_CAPACITY)
    {
      compressed = ring_data;
      if (ring_tail == ring_data + sizeof(size_t))
      {
        ring_tail = ring_data + read_size_t(ring_tail);
        ring_entries--;
      }
    }

    write_size_t(compressed, ring_head - ring_data);
    compressed += sizeof(size_t);
    write_size_t(ring_head, compressed - ring_data);
    ring_head = compressed;
  }
  else
    thisblock_valid = 1;

  swap = thisblock;
  thisblock = nextblock;
  nextblock = swap;

  ring_entries++;
}

/* ==========================================================================
 * Public API -- unchanged from the previous version, so main.c needs no
 * changes at all.
 * ========================================================================== */

/* The history is only allocated while rewind is switched on: it is about
   200 MB and every frame costs a state save plus a delta pass. */
static void ring_free(void)
{
  free(ring_data);   ring_data = NULL;
  free(block_alloc); block_alloc = NULL;
  thisblock = nextblock = NULL;
  ring_head = ring_tail = NULL;
  ring_entries = 0;
  thisblock_valid = 0;
}

void rewind_init(void)
{
  ring_ok = 0;   /* main.c turns it on with rewind_enable() */
}

void rewind_enable(int on)
{
  if (on && !ring_ok)
  {
    ring_ok = ring_init();
    suppress_next_capture = 0;
  }
  else if (!on && ring_ok)
  {
    ring_ok = 0;
    ring_free();
  }
}

int rewind_ready(void)
{
  return ring_ok;
}

void rewind_reset(void)
{
  if (!ring_ok) return;

  ring_head = ring_data + sizeof(size_t);
  ring_tail = ring_data + sizeof(size_t);
  ring_entries = 0;
  thisblock_valid = 0;
  suppress_next_capture = 0;
}

/* Called once per emulated frame that actually ran forward, whether or not
   it was displayed -- capture cadence tracks game-time, not real time or
   frames-shown, so fast-forwarding through a stretch doesn't leave a gap.
   audio_samples/audio_frames are exactly what this frame's audio_update()
   call just produced, stereo-interleaved, audio_frames stereo pairs. */
void rewind_capture_tick(const int16 *audio_samples, int audio_frames)
{
  uint8 *blob;
  int count;

  if (!ring_ok || !emu_running) return;
  if (netplay_active()) return;      /* rewind is not available during netplay */

  if (suppress_next_capture)
  {
    suppress_next_capture = 0;
    return;
  }

  blob = ring_push_where();

  state_save(blob + BLOB_STATE_OFF);
  memcpy(blob + BLOB_PIXEL_OFF, bitmap.data, REWIND_PIXEL_SIZE);
  memcpy(blob + BLOB_VIEWPORT_OFF, &bitmap.viewport, sizeof(saved_viewport_t));

  count = audio_frames;
  if (count > REWIND_AUDIO_FRAMES) count = REWIND_AUDIO_FRAMES;
  if (count < 0) count = 0;
  memcpy(blob + BLOB_AUDCOUNT_OFF, &count, sizeof(int));
  if (count > 0 && audio_samples)
    memcpy(blob + BLOB_AUDIO_OFF, audio_samples, (size_t)count * 2 * sizeof(int16));

  ring_push_do();
}

int rewind_available(void)
{
  return ring_ok ? (int)ring_entries : 0;
}

/* Pops and loads the most recent snapshot. Returns 0 with nothing changed
   once the buffer runs dry, so the caller can tell the person they've hit
   the end of what's kept rather than silently freezing on the same frame.
   On success, the restored picture is already in bitmap.data (the caller
   just needs to present it) and the reversed audio for this step is
   available via rewind_get_audio(). */
int rewind_step(void)
{
  uint8 *blob;
  int count, i;
  const int16 *src;

  if (!ring_ok || ring_entries == 0) return 0;

  if (!ring_pop(&blob)) return 0;

  if (!state_load(blob + BLOB_STATE_OFF))
  {
    /* Shouldn't happen for a state this same session just wrote, but
       don't present a picture whose matching state didn't actually
       load. */
    return 0;
  }

  /* Recomputes what system_frame_gen() normally derives from reg[12]
     every forward frame -- reg[12] is part of the state state_load()
     just restored, so this reads back exactly what it was at capture
     time. Without it, interlaced (and what depends on it) stays stale
     from whatever it was before rewind started, mismatched against the
     VDP register state that now differs -- get_source_size() doubles
     the source height when interlaced is set, so a mismatch here shows
     up as the picture looking cropped along one edge. */
  interlaced = (reg[12] & 0x02) >> 1;
  im2_flag   = ((reg[12] & 0x06) == 0x06);
  odd_frame  = interlaced;

  memcpy(&bitmap.viewport, blob + BLOB_VIEWPORT_OFF, sizeof(saved_viewport_t));

  /* Directly restores the picture that was actually on screen at this
     instant -- see the file header for why this is still a direct copy
     rather than a corrective forward frame. */
  memcpy(bitmap.data, blob + BLOB_PIXEL_OFF, REWIND_PIXEL_SIZE);

  memcpy(&count, blob + BLOB_AUDCOUNT_OFF, sizeof(int));
  if (count > REWIND_AUDIO_FRAMES) count = REWIND_AUDIO_FRAMES;
  if (count < 0) count = 0;
  src = (const int16 *)(blob + BLOB_AUDIO_OFF);
  for (i = 0; i < count; i++)
  {
    reversed_audio[i * 2]     = src[(count - 1 - i) * 2];
    reversed_audio[i * 2 + 1] = src[(count - 1 - i) * 2 + 1];
  }
  reversed_audio_frames = count;

  waveout_flush();
  cheats_apply();

  /* The caller still calls video_frame() right after this returns, to
     actually present the picture that was just restored directly above
     -- that presentation must not be captured as a new snapshot. */
  suppress_next_capture = 1;

  return 1;
}

/* Valid only immediately after a rewind_step() that returned 1 -- the
   reversed audio for that specific step, ready to hand straight to
   waveout_submit(). */
const int16 *rewind_get_audio(int *out_frames)
{
  *out_frames = reversed_audio_frames;
  return reversed_audio;
}
