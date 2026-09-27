/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  netplay.h -- two-player LAN netplay (lockstep).
 *
 *  Both machines run the same game frame by frame. Every frame each side
 *  applies its own controller input from `delay` frames ago together with
 *  the other side's input for the same frame, so the emulators see identical
 *  inputs and stay identical. There is no rollback: if the other side's input
 *  for a frame hasn't arrived yet, that frame simply waits. On a LAN, with a
 *  delay of 1-2 frames, that's imperceptible.
 *
 *  The host plays Player 1 (Genesis port 1) and the guest Player 2. Each
 *  player uses their own "Player 1" controls locally.
 *
 *  Transport is TCP over the local network; there is no relay, NAT
 *  traversal or internet matchmaking. Hosts announce themselves to
 *  broadcast queries on the LAN so a guest can pick them from a list.
 *
 *  This file knows nothing about the emulator core: everything it needs comes
 *  through the np_host_t callbacks below.
 ****************************************************************************/

#ifndef _NETPLAY_H_
#define _NETPLAY_H_

#include <windows.h>

#define NP_DEFAULT_PORT   55455
#define NP_MAX_SETTINGS   24

enum { NP_IDLE = 0, NP_LISTEN, NP_HANDSHAKE, NP_ACTIVE };
enum { NP_ROLE_NONE = 0, NP_ROLE_HOST = 1, NP_ROLE_GUEST = 2 };

/* What has to be identical on both machines for the games to stay in step. */
typedef struct
{
  unsigned char rom_sha[32];        /* SHA-256 of the game data */
  int           n_settings;
  const char   *name[NP_MAX_SETTINGS];
  unsigned int  value[NP_MAX_SETTINGS];
} np_info;

typedef struct
{
  /* 1 = ok. Otherwise fills `err` with a reason the user can act on. */
  int  (*preflight)(char *err, int err_len);
  /* Fills the ROM hash and the emulation-affecting settings. */
  void (*collect)(np_info *info);
  /* Hard-resets the game so both sides start from the same state. */
  void (*begin)(void);
  /* The session is over (the game keeps running on its own). */
  void (*end)(void);
  /* Hash of the whole machine state, for out-of-sync detection. */
  unsigned long long (*state_hash)(void);
  /* Name of the loaded game, shown to guests browsing for hosts. */
  const char *(*game_title)(void);
  /* level 0: status message. level 1: error the user has to read. */
  void (*message)(const char *text, int level);
} np_host_t;

typedef struct
{
  char           addr[48];
  unsigned short port;
  char           name[32];          /* computer name */
  char           game[48];
  int            needs_code;
} np_found;

extern void netplay_init(const np_host_t *host);
extern void netplay_shutdown(void);

/* Both return 1 on success; on failure 0 with a reason in `err`. */
extern int  netplay_host(int port, const char *code, int delay, char *err, int err_len);
extern int  netplay_join(const char *address, int port, const char *code, char *err, int err_len);
extern void netplay_disconnect(const char *reason);

/* Asks the LAN who is hosting. Blocks for about 0.7 s. Returns the count. */
extern int  netplay_discover(np_found *out, int max);

/* Every emulation tick, before anything else. Accepts connections, reads
   the socket and runs the handshake. */
extern void netplay_poll(void);

extern int  netplay_state(void);       /* NP_IDLE .. NP_ACTIVE */
extern int  netplay_active(void);      /* a lockstep session is running */
extern int  netplay_role(void);
extern int  netplay_delay(void);
extern unsigned netplay_frame(void);

/* Lockstep: call frame_ready() first; only run a frame when it returns 1,
   bracketing the frame with begin() and end(). While waiting, sleep on
   netplay_wait_handle() (signalled when data arrives). */
extern int  netplay_frame_ready(void);
extern void netplay_frame_begin(void);
extern void netplay_frame_end(void);
extern HANDLE netplay_wait_handle(void);

/* Called from the emulator's per-frame input update: replaces both pads with
   the agreed inputs for this frame. */
extern void netplay_apply_input(unsigned short *pad0, unsigned short *pad1);

/* One-line description for the status bar; "" when idle. */
extern const char *netplay_status_text(void);
extern const char *netplay_local_addresses(void);

/* Exposed for the host side (ROM hashing) and for tests. */
extern void np_sha256(const void *data, size_t len, unsigned char out[32]);

#endif
