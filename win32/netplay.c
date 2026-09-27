/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  netplay.c -- two-player LAN netplay (lockstep). See netplay.h.
 *
 *  Wire format (TCP): [u16 length][u8 type][payload], little-endian, length
 *  counting the type byte and payload. Nothing here trusts the peer: lengths
 *  are bounded, unknown or out-of-order packets end the session.
 *
 *  Handshake (guest connects to host):
 *    G -> H  HELLO      protocol version, guest nonce
 *    H -> G  CHALLENGE  host nonce, HMAC(code, "H" | guest nonce | host nonce)
 *    G -> H  AUTH       HMAC(code, "G" | host nonce | guest nonce)
 *    H -> G  INFO       game / build / settings, and the input delay
 *    G -> H  INFO
 *    H -> G  START      both sides hard-reset and begin at frame 0
 *  The session code is only a guard against joining the wrong machine by
 *  accident: input packets are not encrypted or individually authenticated.
 *
 *  Lockstep: on frame F, each side sends its local input for frame F+delay,
 *  then runs frame F using its own input from frame F-delay and the peer's
 *  input for frame F. Frames 0..delay-1 use zero input on both sides.
 ****************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netplay.h"

#define NP_PROTO          1
#define NP_DISC_PORT      55456
#define NP_RING           64
#define NP_MAX_PKT        512
#define NP_TX_MAX         (64 * 1024)
#define NP_HS_TIMEOUT_MS  15000
#define NP_RUN_TIMEOUT_MS 120000    /* no input from the peer for this long. Long enough
                                       for the other player to have a menu or a dialog
                                       open (emulation is stopped meanwhile). */
#define NP_HASH_INTERVAL  60        /* frames between state-hash comparisons */
#define NP_HASH_SLOTS     4

enum
{
  PKT_HELLO = 1, PKT_CHALLENGE, PKT_AUTH, PKT_INFO, PKT_START,
  PKT_INPUT, PKT_HASH, PKT_REJECT, PKT_BYE
};

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

/****************************************************************************
 * SHA-256 / HMAC-SHA-256
 ****************************************************************************/

typedef struct { u32 h[8]; u64 len; u8 buf[64]; u32 n; } sha_ctx;

static const u32 K256[64] =
{
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(sha_ctx *c, const u8 *p)
{
  u32 w[64], a, b, cc, d, e, f, g, h, t1, t2;
  int i;

  for (i = 0; i < 16; i++)
    w[i] = ((u32)p[i*4] << 24) | ((u32)p[i*4+1] << 16) | ((u32)p[i*4+2] << 8) | p[i*4+3];
  for (i = 16; i < 64; i++)
  {
    u32 s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
    u32 s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }

  a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
  e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];

  for (i = 0; i < 64; i++)
  {
    t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
    t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
    h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
  }

  c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
  c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void sha_init(sha_ctx *c)
{
  static const u32 iv[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                             0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
  memcpy(c->h, iv, sizeof(iv));
  c->len = 0;
  c->n = 0;
}

static void sha_update(sha_ctx *c, const void *data, size_t len)
{
  const u8 *p = (const u8 *)data;

  c->len += len;
  while (len)
  {
    u32 take = 64 - c->n;
    if (take > len) take = (u32)len;
    memcpy(c->buf + c->n, p, take);
    c->n += take; p += take; len -= take;
    if (c->n == 64) { sha_block(c, c->buf); c->n = 0; }
  }
}

static void sha_final(sha_ctx *c, u8 out[32])
{
  u64 bits = c->len * 8;
  u8 pad = 0x80, zero = 0, lenbuf[8];
  int i;

  sha_update(c, &pad, 1);
  while (c->n != 56) sha_update(c, &zero, 1);
  for (i = 0; i < 8; i++) lenbuf[i] = (u8)(bits >> (56 - 8 * i));
  sha_update(c, lenbuf, 8);
  for (i = 0; i < 8; i++)
  {
    out[i*4]   = (u8)(c->h[i] >> 24);
    out[i*4+1] = (u8)(c->h[i] >> 16);
    out[i*4+2] = (u8)(c->h[i] >> 8);
    out[i*4+3] = (u8)(c->h[i]);
  }
}

void np_sha256(const void *data, size_t len, u8 out[32])
{
  sha_ctx c;
  sha_init(&c);
  sha_update(&c, data, len);
  sha_final(&c, out);
}

static void hmac_sha256(const u8 *key, size_t klen, const u8 *msg, size_t mlen, u8 out[32])
{
  u8 k[64], ipad[64], opad[64], inner[32];
  sha_ctx c;
  int i;

  memset(k, 0, sizeof(k));
  if (klen > 64) np_sha256(key, klen, k);
  else if (klen) memcpy(k, key, klen);

  for (i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }

  sha_init(&c); sha_update(&c, ipad, 64); sha_update(&c, msg, mlen); sha_final(&c, inner);
  sha_init(&c); sha_update(&c, opad, 64); sha_update(&c, inner, 32); sha_final(&c, out);
}

static int const_time_equal(const u8 *a, const u8 *b, int n)
{
  u8 d = 0;
  int i;
  for (i = 0; i < n; i++) d |= (u8)(a[i] ^ b[i]);
  return d == 0;
}

/****************************************************************************
 * State
 ****************************************************************************/

static struct
{
  const np_host_t *host;
  int      wsa_ready;

  int      state;               /* NP_IDLE .. NP_ACTIVE */
  int      role;
  int      port;
  int      delay;
  char     code[64];

  SOCKET   listen_sock, sock, disc_sock;
  WSAEVENT ev;

  u8       rx[NP_MAX_PKT * 2];
  int      rx_len;
  u8      *tx;
  int      tx_len;

  /* handshake */
  int      hs_stage;
  DWORD    hs_start;
  u8       nonce_g[16], nonce_h[16];
  u8       exe_sha[32];
  int      exe_sha_ready;

  /* lockstep */
  u32      frame;
  int      in_frame, input_done;
  u16      L[NP_RING], R[NP_RING];
  u32      r_next;              /* next peer frame we expect */
  u16      final0, final1;
  DWORD    wait_since;
  int      waiting;

  /* desync detection */
  u32      my_frame[NP_HASH_SLOTS], peer_frame[NP_HASH_SLOTS];
  u64      my_hash[NP_HASH_SLOTS], peer_hash[NP_HASH_SLOTS];
  int      my_have[NP_HASH_SLOTS], peer_have[NP_HASH_SLOTS];

  char     status[160];
  char     local_addrs[128];
} np;

/****************************************************************************
 * Small helpers
 ****************************************************************************/

static void put_u32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void put_u16(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static u32  get_u32(const u8 *p) { return p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u16  get_u16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }

static void say(const char *text, int level)
{
  if (np.host && np.host->message) np.host->message(text, level);
}

static void set_err(char *err, int len, const char *msg)
{
  if (err && len > 0) lstrcpynA(err, msg, len);
}

static int wsa_start(void)
{
  WSADATA d;

  if (np.wsa_ready) return 1;
  if (WSAStartup(MAKEWORD(2, 2), &d) != 0) return 0;
  np.wsa_ready = 1;
  return 1;
}

/* Fills `out` with cryptographically secure random bytes from Windows.
   Returns 0 if Windows could not provide them. There is deliberately no
   weaker fallback: the handshake's nonces are what stop a recorded handshake
   from being replayed, so guessable ones would defeat them. */
static int fill_random(u8 *out, int n)
{
  typedef BOOLEAN (WINAPI *PF_RtlGenRandom)(PVOID, ULONG);
  HMODULE adv = LoadLibraryA("advapi32.dll");
  PF_RtlGenRandom gen = adv ? (PF_RtlGenRandom)GetProcAddress(adv, "SystemFunction036") : NULL;
  int ok = (gen && gen(out, (ULONG)n)) ? 1 : 0;

  if (adv) FreeLibrary(adv);
  if (!ok) memset(out, 0, (size_t)n);   /* never leave anything guessable behind */
  return ok;
}

#define NP_NO_RANDOM_MSG "Windows could not provide secure random numbers, so netplay cannot start."

static void close_sock(SOCKET *s)
{
  if (*s != INVALID_SOCKET) { closesocket(*s); *s = INVALID_SOCKET; }
}

static void refresh_local_addresses(void)
{
  char name[128];
  struct hostent *he;
  int i;

  np.local_addrs[0] = '\0';
  if (gethostname(name, sizeof(name)) != 0) return;
  he = gethostbyname(name);
  if (!he || he->h_addrtype != AF_INET) return;

  for (i = 0; he->h_addr_list[i]; i++)
  {
    struct in_addr a;
    memcpy(&a, he->h_addr_list[i], sizeof(a));
    if (a.S_un.S_un_b.s_b1 == 127) continue;
    if (np.local_addrs[0]) lstrcatA(np.local_addrs, ", ");
    if (lstrlenA(np.local_addrs) + 20 < (int)sizeof(np.local_addrs)) lstrcatA(np.local_addrs, inet_ntoa(a));
  }
}

const char *netplay_local_addresses(void)
{
  if (wsa_start()) refresh_local_addresses();
  return np.local_addrs;
}

/****************************************************************************
 * Sending / receiving
 ****************************************************************************/

static void fail(const char *reason, int is_error);

static int tx_flush(void)
{
  while (np.tx_len > 0 && np.sock != INVALID_SOCKET)
  {
    int n = send(np.sock, (const char *)np.tx, np.tx_len, 0);

    if (n > 0)
    {
      memmove(np.tx, np.tx + n, (size_t)(np.tx_len - n));
      np.tx_len -= n;
    }
    else if (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
    {
      return 1;   /* try again on the next poll */
    }
    else
    {
      return 0;
    }
  }
  return 1;
}

static int send_pkt(int type, const void *payload, int plen)
{
  u8 head[3];
  int len = 1 + plen;

  if (np.sock == INVALID_SOCKET || plen < 0 || len > NP_MAX_PKT) return 0;
  if (np.tx_len + 2 + len > NP_TX_MAX) return 0;

  put_u16(head, (u32)len);
  head[2] = (u8)type;
  memcpy(np.tx + np.tx_len, head, 3);
  np.tx_len += 3;
  if (plen) { memcpy(np.tx + np.tx_len, payload, (size_t)plen); np.tx_len += plen; }

  return tx_flush();
}

/* Sends a final message, then closes. */
static void reject_peer(const char *reason)
{
  send_pkt(PKT_REJECT, reason, lstrlenA(reason) > 200 ? 200 : lstrlenA(reason));
  tx_flush();
}

/****************************************************************************
 * Session start / end
 ****************************************************************************/

static void reset_lockstep(int delay)
{
  memset(np.L, 0, sizeof(np.L));
  memset(np.R, 0, sizeof(np.R));
  np.delay = delay;
  np.frame = 0;
  np.r_next = (u32)delay;
  np.in_frame = np.input_done = 0;
  np.waiting = 0;
  memset(np.my_have, 0, sizeof(np.my_have));
  memset(np.peer_have, 0, sizeof(np.peer_have));
  memset(np.my_frame, 0, sizeof(np.my_frame));
  memset(np.peer_frame, 0, sizeof(np.peer_frame));
}

static void begin_session(int delay)
{
  char msg[128];

  close_sock(&np.listen_sock);
  close_sock(&np.disc_sock);

  reset_lockstep(delay);
  np.state = NP_ACTIVE;
  np.wait_since = GetTickCount();

  if (np.host && np.host->begin) np.host->begin();

  wsprintfA(msg, "Netplay started: you are Player %d (input delay %d frame%s)",
            np.role == NP_ROLE_HOST ? 1 : 2, delay, delay == 1 ? "" : "s");
  say(msg, 0);
}

static void teardown(void)
{
  close_sock(&np.sock);
  close_sock(&np.listen_sock);
  close_sock(&np.disc_sock);
  if (np.ev) { WSACloseEvent(np.ev); np.ev = NULL; }
  np.rx_len = np.tx_len = 0;
  np.in_frame = np.input_done = 0;
  np.waiting = 0;
  np.hs_stage = 0;
  memset(np.code, 0, sizeof(np.code));
}

void netplay_disconnect(const char *reason)
{
  int was_active;

  if (np.state == NP_IDLE) return;

  was_active = (np.state == NP_ACTIVE);
  if (np.sock != INVALID_SOCKET)
  {
    char sink[256];
    DWORD t0 = GetTickCount();

    send_pkt(PKT_BYE, NULL, 0);
    tx_flush();

    /* Half-close, then discard what the peer still has in flight (up to
       ~100 ms). Closing with unread data pending makes Windows send a reset,
       which the other side would report as a lost connection instead of a
       normal disconnect. */
    shutdown(np.sock, SD_SEND);
    while (GetTickCount() - t0 < 100)
    {
      int n = recv(np.sock, sink, sizeof(sink), 0);
      if (n == 0) break;
      if (n < 0 && WSAGetLastError() != WSAEWOULDBLOCK) break;
      if (n < 0) Sleep(5);
    }
  }

  teardown();
  np.state = NP_IDLE;
  np.role = NP_ROLE_NONE;
  np.status[0] = '\0';

  if (was_active && np.host && np.host->end) np.host->end();
  if (reason && reason[0]) say(reason, 0);
}

/* A problem the user needs to read about. */
static void fail(const char *reason, int is_error)
{
  int was_idle = (np.state == NP_IDLE);
  char text[300];

  lstrcpynA(text, reason, sizeof(text));
  if (was_idle) return;

  /* A host that is still setting up with a guest (wrong code, a port scan, a
     dropped connection) goes back to waiting for a player instead of ending
     the whole hosting session. It is reported as a status line, not a dialog,
     so a stray connection cannot keep interrupting the game. */
  if (np.role == NP_ROLE_HOST && np.state == NP_HANDSHAKE && np.listen_sock != INVALID_SOCKET)
  {
    close_sock(&np.sock);
    np.rx_len = np.tx_len = 0;
    np.hs_stage = 0;
    np.state = NP_LISTEN;
    say(text, is_error ? 1 : 0);
    return;
  }

  netplay_disconnect(NULL);
  say(text, is_error ? 1 : 0);
}

/****************************************************************************
 * Compatibility info
 ****************************************************************************/

static void ensure_exe_sha(void)
{
  wchar_t path[MAX_PATH * 2];    /* wide, so a folder with non-ANSI characters works */
  FILE *f;
  unsigned char *buf;
  long size;

  if (np.exe_sha_ready) return;

  memset(np.exe_sha, 0, sizeof(np.exe_sha));
  if (GetModuleFileNameW(NULL, path, MAX_PATH * 2) && (f = _wfopen(path, L"rb")) != NULL)
  {
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (size > 0) ? (unsigned char *)malloc((size_t)size) : NULL;
    if (buf && fread(buf, 1, (size_t)size, f) == (size_t)size) np_sha256(buf, (size_t)size, np.exe_sha);
    free(buf);
    fclose(f);
  }
  np.exe_sha_ready = 1;
}

/* payload: rom[32] exe[32] n[1] values[4*n] delay[1] */
static int build_info(u8 *out, int delay)
{
  np_info info;
  int i, n = 0;

  memset(&info, 0, sizeof(info));
  if (np.host && np.host->collect) np.host->collect(&info);
  ensure_exe_sha();

  memcpy(out + n, info.rom_sha, 32); n += 32;
  memcpy(out + n, np.exe_sha, 32);   n += 32;
  out[n++] = (u8)info.n_settings;
  for (i = 0; i < info.n_settings; i++) { put_u32(out + n, info.value[i]); n += 4; }
  out[n++] = (u8)delay;
  return n;
}

/* Returns 1 if the peer's info matches ours; otherwise fills `reason`. */
static int check_info(const u8 *p, int plen, int *peer_delay, char *reason, int rlen)
{
  np_info mine;
  int n, i, bad = 0;
  char list[200];

  memset(&mine, 0, sizeof(mine));
  if (np.host && np.host->collect) np.host->collect(&mine);
  ensure_exe_sha();

  if (plen < 32 + 32 + 1) { lstrcpynA(reason, "Bad handshake data from the other player.", rlen); return 0; }
  n = p[64];
  if (plen != 65 + 4 * n + 1) { lstrcpynA(reason, "Bad handshake data from the other player.", rlen); return 0; }
  *peer_delay = p[65 + 4 * n];

  if (!const_time_equal(p + 32, np.exe_sha, 32))
  {
    lstrcpynA(reason, "The other player is running a different build of the program.\n"
                      "Both players must use exactly the same gpgx.exe (copy it from one PC to the other).", rlen);
    return 0;
  }
  if (!const_time_equal(p, mine.rom_sha, 32))
  {
    lstrcpynA(reason, "The two players have different game files loaded.\n"
                      "Both must load the same ROM.", rlen);
    return 0;
  }
  if (n != mine.n_settings)
  {
    lstrcpynA(reason, "The emulation settings differ.", rlen);
    return 0;
  }

  list[0] = '\0';
  for (i = 0; i < n; i++)
  {
    if (get_u32(p + 65 + 4 * i) != mine.value[i])
    {
      if (list[0] && lstrlenA(list) < (int)sizeof(list) - 40) lstrcatA(list, ", ");
      if (lstrlenA(list) < (int)sizeof(list) - 40) lstrcatA(list, mine.name[i]);
      bad++;
    }
  }
  if (bad)
  {
    wsprintfA(reason, "These settings differ between the two players: %s.\n"
                      "Set them the same on both PCs and connect again.", list);
    return 0;
  }
  return 1;
}

/****************************************************************************
 * Handshake
 ****************************************************************************/

static void make_proof(char who, const u8 *n1, const u8 *n2, u8 out[32])
{
  u8 msg[33];
  msg[0] = (u8)who;
  memcpy(msg + 1, n1, 16);
  memcpy(msg + 17, n2, 16);
  hmac_sha256((const u8 *)np.code, (size_t)lstrlenA(np.code), msg, sizeof(msg), out);
}

static int preflight_or_reject(void)
{
  char err[200];

  if (np.host && np.host->preflight && !np.host->preflight(err, sizeof(err)))
  {
    reject_peer(err);
    fail(err, 1);
    return 0;
  }
  return 1;
}

static void handle_handshake(int type, const u8 *p, int plen)
{
  char reason[300];
  u8 proof[32], expect[32], info[NP_MAX_SETTINGS * 4 + 80];
  int peer_delay;

  if (type == PKT_REJECT)
  {
    char tmp[220], msg[300];
    int n = (plen < 200) ? plen : 200;

    memcpy(tmp, p, (size_t)n);
    tmp[n] = '\0';
    wsprintfA(msg, "The other player refused the connection:\n\n%s", tmp);
    fail(msg, 1);
    return;
  }
  if (type == PKT_BYE) { fail("The other player disconnected.", 0); return; }

  if (np.role == NP_ROLE_HOST)
  {
    if (np.hs_stage == 0 && type == PKT_HELLO && plen == 17 && p[0] == NP_PROTO)
    {
      memcpy(np.nonce_g, p + 1, 16);
      if (!fill_random(np.nonce_h, 16))
      {
        reject_peer(NP_NO_RANDOM_MSG);
        fail(NP_NO_RANDOM_MSG, 1);
        return;
      }
      make_proof('H', np.nonce_g, np.nonce_h, proof);
      memcpy(info, np.nonce_h, 16);
      memcpy(info + 16, proof, 32);
      send_pkt(PKT_CHALLENGE, info, 48);
      np.hs_stage = 1;
    }
    else if (np.hs_stage == 0 && type == PKT_HELLO)
    {
      reject_peer("The other player has an incompatible version of the program.");
      fail("The other player has an incompatible version of the program.", 1);
    }
    else if (np.hs_stage == 1 && type == PKT_AUTH && plen == 32)
    {
      make_proof('G', np.nonce_h, np.nonce_g, expect);
      if (!const_time_equal(p, expect, 32))
      {
        reject_peer("Wrong session code.");
        fail("A player tried to join with the wrong session code.", 1);
        return;
      }
      if (!preflight_or_reject()) return;
      send_pkt(PKT_INFO, info, build_info(info, np.delay));
      np.hs_stage = 2;
    }
    else if (np.hs_stage == 2 && type == PKT_INFO)
    {
      if (!check_info(p, plen, &peer_delay, reason, sizeof(reason)))
      {
        reject_peer(reason);
        fail(reason, 1);
        return;
      }
      info[0] = (u8)np.delay;
      send_pkt(PKT_START, info, 1);
      tx_flush();
      begin_session(np.delay);
    }
    else
    {
      fail("Unexpected data during connection setup.", 1);
    }
  }
  else /* guest */
  {
    if (np.hs_stage == 0 && type == PKT_CHALLENGE && plen == 48)
    {
      memcpy(np.nonce_h, p, 16);
      make_proof('H', np.nonce_g, np.nonce_h, expect);
      if (!const_time_equal(p + 16, expect, 32))
      {
        reject_peer("Wrong session code.");
        fail("Wrong session code (the host's answer didn't match).", 1);
        return;
      }
      make_proof('G', np.nonce_h, np.nonce_g, proof);
      send_pkt(PKT_AUTH, proof, 32);
      np.hs_stage = 1;
    }
    else if (np.hs_stage == 1 && type == PKT_INFO)
    {
      if (!preflight_or_reject()) return;
      if (!check_info(p, plen, &peer_delay, reason, sizeof(reason)))
      {
        reject_peer(reason);
        fail(reason, 1);
        return;
      }
      if (peer_delay < 0 || peer_delay > 8) { fail("Bad input delay from the host.", 1); return; }
      np.delay = peer_delay;
      send_pkt(PKT_INFO, info, build_info(info, peer_delay));
      np.hs_stage = 2;
    }
    else if (np.hs_stage == 2 && type == PKT_START && plen == 1)
    {
      begin_session(p[0] > 8 ? np.delay : p[0]);
    }
    else
    {
      fail("Unexpected data during connection setup.", 1);
    }
  }
}

/****************************************************************************
 * Active session packets
 ****************************************************************************/

static void compare_hash_slot(int slot)
{
  if (np.my_have[slot] && np.peer_have[slot] && np.my_frame[slot] == np.peer_frame[slot])
  {
    if (np.my_hash[slot] != np.peer_hash[slot])
    {
      char msg[200];
      wsprintfA(msg, "The two games went out of sync (around frame %u), so the session was ended.\n\n"
                     "This can happen if something differs between the two PCs that the program can't see. "
                     "Both players should hard reset and connect again.", np.my_frame[slot]);
      fail(msg, 1);
      return;
    }
    np.my_have[slot] = np.peer_have[slot] = 0;
  }
}

static void handle_active(int type, const u8 *p, int plen)
{
  switch (type)
  {
    case PKT_INPUT:
      if (plen == 6)
      {
        u32 f = get_u32(p);
        if (f != np.r_next || f >= np.frame + (u32)np.delay * 2 + 8)
        {
          fail("Lost sync with the other player (unexpected input frame).", 1);
          return;
        }
        np.R[f % NP_RING] = get_u16(p + 4);
        np.r_next = f + 1;
        np.waiting = 0;
      }
      else fail("Bad data from the other player.", 1);
      break;

    case PKT_HASH:
      if (plen == 12)
      {
        u32 f = get_u32(p);
        int slot = (int)((f / NP_HASH_INTERVAL) % NP_HASH_SLOTS);
        np.peer_frame[slot] = f;
        np.peer_hash[slot] = (u64)get_u32(p + 4) | ((u64)get_u32(p + 8) << 32);
        np.peer_have[slot] = 1;
        compare_hash_slot(slot);
      }
      else fail("Bad data from the other player.", 1);
      break;

    case PKT_BYE:
      fail("The other player disconnected.", 0);
      break;

    case PKT_REJECT:
      fail("The other player ended the session.", 0);
      break;

    default:
      fail("Unexpected data from the other player.", 1);
      break;
  }
}

/****************************************************************************
 * Socket pump
 ****************************************************************************/

static void parse_rx(void)
{
  while (np.state != NP_IDLE && np.rx_len >= 2)
  {
    int len = get_u16(np.rx);
    int type, plen;
    u8 payload[NP_MAX_PKT];

    if (len < 1 || len > NP_MAX_PKT) { fail("Bad data from the other player.", 1); return; }
    if (np.rx_len < 2 + len) break;

    type = np.rx[2];
    plen = len - 1;
    memcpy(payload, np.rx + 3, (size_t)plen);
    memmove(np.rx, np.rx + 2 + len, (size_t)(np.rx_len - 2 - len));
    np.rx_len -= 2 + len;

    if (np.state == NP_HANDSHAKE) handle_handshake(type, payload, plen);
    else if (np.state == NP_ACTIVE) handle_active(type, payload, plen);
  }
}

static void pump_socket(void)
{
  /* The event is manual-reset: without this it stays signalled after the
     first packet and every later wait returns at once (a busy loop). It is
     cleared before the drain below, so data arriving meanwhile re-signals it. */
  if (np.ev) WSAResetEvent(np.ev);

  while (np.state != NP_IDLE && np.sock != INVALID_SOCKET)
  {
    int n = recv(np.sock, (char *)np.rx + np.rx_len, (int)sizeof(np.rx) - np.rx_len, 0);

    if (n > 0)
    {
      np.rx_len += n;
      parse_rx();
      if (np.rx_len >= (int)sizeof(np.rx)) { fail("Bad data from the other player.", 1); return; }
    }
    else if (n == 0)
    {
      fail(np.state == NP_ACTIVE ? "The other player disconnected." : "The other player closed the connection.", 0);
      return;
    }
    else
    {
      int e = WSAGetLastError();
      if (e == WSAEWOULDBLOCK) break;
      if (e == WSAECONNRESET || e == WSAECONNABORTED)
        fail(np.state == NP_ACTIVE ? "The other player disconnected." : "The other player closed the connection.", 0);
      else
        fail("Lost the connection to the other player.", 1);
      return;
    }
  }

  if (np.state != NP_IDLE && !tx_flush()) fail("Lost the connection to the other player.", 1);
}

static void adopt_socket(SOCKET s)
{
  BOOL nodelay = TRUE;

  np.sock = s;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
  if (!np.ev) np.ev = WSACreateEvent();
  WSAEventSelect(s, np.ev, FD_READ | FD_CLOSE);
  np.rx_len = np.tx_len = 0;
  np.hs_stage = 0;
  np.hs_start = GetTickCount();
  np.state = NP_HANDSHAKE;
}

static void answer_discovery(void)
{
  u8 buf[128];
  struct sockaddr_in from;
  int fromlen;

  for (;;)
  {
    int n;
    fromlen = sizeof(from);
    n = recvfrom(np.disc_sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    if (n <= 0) break;

    if (n >= 6 && !memcmp(buf, "GPNP?", 5) && buf[5] == NP_PROTO)
    {
      u8 reply[96];
      char name[32] = "", game[48] = "";
      DWORD nl = sizeof(name);
      const char *title = (np.host && np.host->game_title) ? np.host->game_title() : "";
      int k = 0;

      GetComputerNameA(name, &nl);
      lstrcpynA(game, title ? title : "", sizeof(game));

      memcpy(reply, "GPNP!", 5); k = 5;
      reply[k++] = NP_PROTO;
      put_u16(reply + k, (u32)np.port); k += 2;
      reply[k++] = np.code[0] ? 1 : 0;
      memcpy(reply + k, name, 32); k += 32;
      memcpy(reply + k, game, 48); k += 48;
      sendto(np.disc_sock, (const char *)reply, k, 0, (struct sockaddr *)&from, fromlen);
    }
  }
}

void netplay_poll(void)
{
  DWORD now;

  if (np.state == NP_IDLE) return;
  now = GetTickCount();

  if (np.state == NP_LISTEN)
  {
    SOCKET s;

    if (np.disc_sock != INVALID_SOCKET) answer_discovery();

    s = accept(np.listen_sock, NULL, NULL);
    if (s != INVALID_SOCKET) adopt_socket(s);
    return;
  }

  /* Someone else connecting while a handshake is in progress: turn them away. */
  if (np.role == NP_ROLE_HOST && np.state == NP_HANDSHAKE && np.listen_sock != INVALID_SOCKET)
  {
    SOCKET extra = accept(np.listen_sock, NULL, NULL);
    if (extra != INVALID_SOCKET) closesocket(extra);
  }

  if (np.sock != INVALID_SOCKET) pump_socket();

  if (np.state == NP_HANDSHAKE && now - np.hs_start > NP_HS_TIMEOUT_MS)
    fail("The connection attempt timed out.", 0);
}

/****************************************************************************
 * Host / join
 ****************************************************************************/

void netplay_init(const np_host_t *host)
{
  memset(&np, 0, sizeof(np));
  np.host = host;
  np.listen_sock = np.sock = np.disc_sock = INVALID_SOCKET;
  np.tx = (u8 *)malloc(NP_TX_MAX);
}

void netplay_shutdown(void)
{
  if (np.state != NP_IDLE) netplay_disconnect(NULL);
  free(np.tx);
  np.tx = NULL;
  if (np.wsa_ready) { WSACleanup(); np.wsa_ready = 0; }
}

static int preflight_local(char *err, int err_len)
{
  char msg[200];

  if (np.state != NP_IDLE)
  {
    set_err(err, err_len, "Netplay is already running. Stop it first.");
    return 0;
  }
  if (np.host && np.host->preflight && !np.host->preflight(msg, sizeof(msg)))
  {
    set_err(err, err_len, msg);
    return 0;
  }
  if (!wsa_start())
  {
    set_err(err, err_len, "Windows networking could not be started.");
    return 0;
  }
  return 1;
}

int netplay_host(int port, const char *code, int delay, char *err, int err_len)
{
  struct sockaddr_in a;
  BOOL yes = TRUE;
  u_long nb = 1;

  u8 probe[16];

  if (!preflight_local(err, err_len)) return 0;
  if (!fill_random(probe, sizeof(probe))) { set_err(err, err_len, NP_NO_RANDOM_MSG); return 0; }
  if (port < 1024 || port > 65535) { set_err(err, err_len, "Choose a port between 1024 and 65535."); return 0; }
  if (delay < 1) delay = 1;
  if (delay > 8) delay = 8;

  np.listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (np.listen_sock == INVALID_SOCKET) { set_err(err, err_len, "Could not create a network socket."); return 0; }

  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons((u_short)port);

  if (bind(np.listen_sock, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(np.listen_sock, 1) != 0)
  {
    close_sock(&np.listen_sock);
    set_err(err, err_len, "Could not listen on that port (is another program, or another copy of this one, using it?).");
    return 0;
  }
  ioctlsocket(np.listen_sock, FIONBIO, &nb);

  /* LAN discovery. Failing to bind this isn't fatal: guests can still type the address. */
  np.disc_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (np.disc_sock != INVALID_SOCKET)
  {
    setsockopt(np.disc_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    a.sin_port = htons(NP_DISC_PORT);
    if (bind(np.disc_sock, (struct sockaddr *)&a, sizeof(a)) != 0) close_sock(&np.disc_sock);
    else ioctlsocket(np.disc_sock, FIONBIO, &nb);
  }

  lstrcpynA(np.code, code ? code : "", sizeof(np.code));
  np.port = port;
  np.delay = delay;
  np.role = NP_ROLE_HOST;
  np.state = NP_LISTEN;
  return 1;
}

int netplay_join(const char *address, int port, const char *code, char *err, int err_len)
{
  struct addrinfo hints, *res = NULL;
  char portstr[16];
  SOCKET s;
  u_long nb = 1;
  fd_set wfds, efds;
  struct timeval tv;
  int r, so_err = 0, so_len = sizeof(so_err);
  u8 hello[17];

  if (!preflight_local(err, err_len)) return 0;
  if (!fill_random(hello, 16)) { set_err(err, err_len, NP_NO_RANDOM_MSG); return 0; }   /* before connecting */
  if (!address || !address[0]) { set_err(err, err_len, "Enter the host's address."); return 0; }
  if (port < 1 || port > 65535) { set_err(err, err_len, "Enter a valid port."); return 0; }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  wsprintfA(portstr, "%d", port);
  if (getaddrinfo(address, portstr, &hints, &res) != 0 || !res)
  {
    set_err(err, err_len, "Could not find that address.");
    return 0;
  }

  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) { freeaddrinfo(res); set_err(err, err_len, "Could not create a network socket."); return 0; }

  ioctlsocket(s, FIONBIO, &nb);
  r = connect(s, res->ai_addr, (int)res->ai_addrlen);
  freeaddrinfo(res);

  if (r == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
  {
    FD_ZERO(&wfds); FD_ZERO(&efds);
    FD_SET(s, &wfds); FD_SET(s, &efds);
    tv.tv_sec = 5; tv.tv_usec = 0;
    r = select(0, NULL, &wfds, &efds, &tv);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_err, &so_len);
    if (r <= 0 || so_err != 0 || FD_ISSET(s, &efds))
    {
      closesocket(s);
      set_err(err, err_len, "Could not connect. Check the address and that the host is waiting for a player "
                            "(and that the host's firewall allows this program).");
      return 0;
    }
  }
  else if (r == SOCKET_ERROR)
  {
    closesocket(s);
    set_err(err, err_len, "Could not connect to that address.");
    return 0;
  }

  lstrcpynA(np.code, code ? code : "", sizeof(np.code));
  np.role = NP_ROLE_GUEST;
  np.port = port;
  adopt_socket(s);

  if (!fill_random(np.nonce_g, 16))
  {
    /* Nothing has been sent yet: undo the connection and say why. */
    close_sock(&np.sock);
    close_sock(&np.disc_sock);
    teardown();
    np.state = NP_IDLE;
    np.role = NP_ROLE_NONE;
    set_err(err, err_len, NP_NO_RANDOM_MSG);
    return 0;
  }
  hello[0] = NP_PROTO;
  memcpy(hello + 1, np.nonce_g, 16);
  send_pkt(PKT_HELLO, hello, 17);
  return 1;
}

/****************************************************************************
 * LAN discovery (guest side)
 ****************************************************************************/

static void discovery_send(SOCKET s, const char *ip)
{
  struct sockaddr_in to;
  u8 q[6];

  memcpy(q, "GPNP?", 5);
  q[5] = NP_PROTO;
  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_port = htons(NP_DISC_PORT);
  to.sin_addr.s_addr = inet_addr(ip);
  sendto(s, (const char *)q, 6, 0, (struct sockaddr *)&to, sizeof(to));
}

int netplay_discover(np_found *out, int max)
{
  SOCKET s;
  BOOL yes = TRUE;
  u_long nb = 1;
  DWORD start;
  int count = 0;
  char name[128];
  struct hostent *he;

  if (!wsa_start() || max <= 0) return 0;

  s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return 0;
  setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof(yes));
  ioctlsocket(s, FIONBIO, &nb);

  discovery_send(s, "255.255.255.255");
  discovery_send(s, "127.0.0.1");

  /* Directed broadcasts for each local /24, since the global one only leaves by one adapter. */
  if (gethostname(name, sizeof(name)) == 0 && (he = gethostbyname(name)) != NULL && he->h_addrtype == AF_INET)
  {
    int i;
    for (i = 0; he->h_addr_list[i]; i++)
    {
      struct in_addr a;
      char ip[24];
      memcpy(&a, he->h_addr_list[i], sizeof(a));
      wsprintfA(ip, "%d.%d.%d.255", a.S_un.S_un_b.s_b1, a.S_un.S_un_b.s_b2, a.S_un.S_un_b.s_b3);
      discovery_send(s, ip);
    }
  }

  start = GetTickCount();
  while (GetTickCount() - start < 700 && count < max)
  {
    fd_set rf;
    struct timeval tv;
    u8 buf[128];
    struct sockaddr_in from;
    int fromlen = sizeof(from), n;

    FD_ZERO(&rf); FD_SET(s, &rf);
    tv.tv_sec = 0; tv.tv_usec = 50000;
    if (select(0, &rf, NULL, NULL, &tv) <= 0) continue;

    n = recvfrom(s, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    if (n >= 5 + 1 + 2 + 1 + 32 + 48 && !memcmp(buf, "GPNP!", 5) && buf[5] == NP_PROTO)
    {
      np_found f;
      int i, dup = 0;

      memset(&f, 0, sizeof(f));
      lstrcpynA(f.addr, inet_ntoa(from.sin_addr), sizeof(f.addr));
      f.port = get_u16(buf + 6);
      f.needs_code = buf[8];
      memcpy(f.name, buf + 9, 31);
      memcpy(f.game, buf + 41, 47);

      for (i = 0; i < count; i++)
        if (out[i].port == f.port &&
            (!lstrcmpA(out[i].addr, f.addr) ||
             (!lstrcmpA(out[i].name, f.name) && !lstrcmpA(out[i].game, f.game)))) dup = 1;
      if (!dup) out[count++] = f;
    }
  }

  closesocket(s);
  return count;
}

/****************************************************************************
 * Lockstep
 ****************************************************************************/

int netplay_state(void)   { return np.state; }
int netplay_active(void)  { return np.state == NP_ACTIVE; }
int netplay_role(void)    { return np.role; }
int netplay_delay(void)   { return np.delay; }
unsigned netplay_frame(void) { return np.frame; }

HANDLE netplay_wait_handle(void)
{
  return (np.state != NP_IDLE) ? (HANDLE)np.ev : NULL;
}

int netplay_frame_ready(void)
{
  DWORD now;

  if (np.state != NP_ACTIVE) return 0;

  if (np.frame < np.r_next)
  {
    np.waiting = 0;
    return 1;
  }

  now = GetTickCount();
  if (!np.waiting) { np.waiting = 1; np.wait_since = now; }
  else if (now - np.wait_since > NP_RUN_TIMEOUT_MS)
  {
    fail("The other player stopped responding, so the session was ended.", 1);
  }
  return 0;
}

void netplay_frame_begin(void)
{
  np.in_frame = 1;
  np.input_done = 0;
}

void netplay_apply_input(unsigned short *pad0, unsigned short *pad1)
{
  if (np.state != NP_ACTIVE || !np.in_frame) return;

  if (!np.input_done)
  {
    u8 pkt[6];
    u32 f = np.frame;
    u16 local = *pad0;                          /* this PC's own Player 1 controls */
    u16 mine = np.L[f % NP_RING];               /* what was pressed `delay` frames ago */
    u16 theirs = np.R[f % NP_RING];

    np.L[(f + (u32)np.delay) % NP_RING] = local;
    put_u32(pkt, f + (u32)np.delay);
    put_u16(pkt + 4, local);
    if (!send_pkt(PKT_INPUT, pkt, 6)) { fail("Lost the connection to the other player.", 1); return; }

    np.final0 = (np.role == NP_ROLE_HOST) ? mine : theirs;
    np.final1 = (np.role == NP_ROLE_HOST) ? theirs : mine;
    np.input_done = 1;
  }

  *pad0 = np.final0;
  *pad1 = np.final1;
}

void netplay_frame_end(void)
{
  if (np.state != NP_ACTIVE) return;

  np.in_frame = 0;

  if ((np.frame % NP_HASH_INTERVAL) == NP_HASH_INTERVAL - 1 && np.host && np.host->state_hash)
  {
    u64 h = np.host->state_hash();
    int slot = (int)((np.frame / NP_HASH_INTERVAL) % NP_HASH_SLOTS);
    u8 pkt[12];

    np.my_frame[slot] = np.frame;
    np.my_hash[slot] = h;
    np.my_have[slot] = 1;

    put_u32(pkt, np.frame);
    put_u32(pkt + 4, (u32)h);
    put_u32(pkt + 8, (u32)(h >> 32));
    send_pkt(PKT_HASH, pkt, 12);
    compare_hash_slot(slot);
    if (np.state != NP_ACTIVE) return;
  }

  np.frame++;
}

const char *netplay_status_text(void)
{
  switch (np.state)
  {
    case NP_LISTEN:
      wsprintfA(np.status, "Netplay: waiting for Player 2 on port %d", np.port);
      break;
    case NP_HANDSHAKE:
      lstrcpyA(np.status, "Netplay: connecting...");
      break;
    case NP_ACTIVE:
      wsprintfA(np.status, "Netplay: Player %d, delay %d%s",
                np.role == NP_ROLE_HOST ? 1 : 2, np.delay, np.waiting ? " (waiting for the other player)" : "");
      break;
    default:
      np.status[0] = '\0';
      break;
  }
  return np.status;
}
