/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  cheats.c -- Game Genie / Action Replay / raw patch support.
 *
 *  There is no cheat engine in the core: decoding and patching are frontend
 *  work, duplicated in the libretro and Wii ports. The decoder below is
 *  ported from libretro/libretro.c so the accepted code formats stay
 *  identical across ports.
 *
 *  Codes are kept as text and re-decoded on every apply, because what a code
 *  means depends on system_hw -- the same string decodes differently for a
 *  Mega Drive and a Master System game. Storing the decoded address would
 *  silently mis-patch after a console switch.
 *
 *  Patches come in two kinds and are maintained differently:
 *
 *    ROM patches are written once into cart.rom (16-bit systems) or into the
 *    currently banked page (8-bit systems), and the original bytes are kept
 *    so they can be put back. The 8-bit case has to be redone whenever the
 *    mapper switches banks, which is what the core's CHEATS_UPDATE hook is
 *    for.
 *
 *    RAM patches cannot be written once, because the game overwrites them.
 *    They are reapplied every frame.
 ****************************************************************************/

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>

#include "shared.h"
#include "gui.h"
#include "theme.h"
#include "resource.h"

#define CHEAT_CODE_LEN  24
#define CHEAT_DESC_LEN  128

typedef struct
{
  char   code[CHEAT_CODE_LEN];
  char   desc[CHEAT_DESC_LEN];
  int    enabled;
  int    valid;        /* the code decoded against the current system */

  uint32 address;
  uint16 data;
  uint16 old;          /* 16-bit: original word. 8-bit: reference byte (0 = no check) */
  uint8  orig;         /* 8-bit: the byte that was at *prev before it was patched */
  uint8 *prev;         /* patched location in the banked 8-bit ROM window */
} t_cheat;

static t_cheat cheats[CHEAT_MAX];
static int     cheat_count;

/* Indexes of the active patches: RAM grows from the front, ROM from the back. */
static uint8 cheat_index[CHEAT_MAX];
static int   ram_patches;
static int   rom_patches;

static int   patches_live;   /* patches are currently written into memory */

/*
 * Set while the list view is being repopulated. Ticking a checkbox in code
 * raises the same LVN_ITEMCHANGED notification a user click does, and that
 * handler refills the list -- which would tick more boxes, and so on.
 */
static int   list_refilling;

/* While a whole file is being loaded every entry would re-decode and re-patch
   the whole list; that is done once at the end instead. */
static int   defer_apply;

static const char ggvalidchars[] = "ABCDEFGHJKLMNPRSTVWXYZ0123456789";
static const char arvalidchars[] = "0123456789ABCDEF";

/****************************************************************************
 * Decoding
 *
 * Ported from libretro/libretro.c. Returns the code length, or 0 when the
 * string is not a valid code for the system currently running.
 ****************************************************************************/

/* strchr() treats the terminating NUL as part of the string, so a short code
   would have decoded its missing digits as valid ones. */
static const char *valid_char(const char *set, int c)
{
  return c ? strchr(set, c) : NULL;
}

static uint32 decode_cheat(const char *input, int index)
{
  const char *string = input;
  const char *p;
  int i, n;
  uint32 len = 0;
  uint32 address = 0;
  uint16 data = 0;
  uint8  ref = 0;

  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    /* ---- 16-bit systems ---- */

    /* Game Genie: ABCD-EFGH */
    if ((strlen(string) >= 9) && (string[4] == '-'))
    {
      for (i = 0; i < 8; i++)
      {
        if (i == 4) string++;
        p = valid_char(ggvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - ggvalidchars);

        switch (i)
        {
          case 0: data |= n << 3; break;
          case 1: data |= n >> 2; address |= (n & 3) << 14; break;
          case 2: address |= n << 9; break;
          case 3: address |= (n & 0xF) << 20 | (n >> 4) << 8; break;
          case 4: data |= (n & 1) << 12; address |= (n >> 1) << 16; break;
          case 5: data |= (n & 1) << 15 | (n >> 1) << 8; break;
          case 6: data |= (n >> 3) << 13; address |= (n & 7) << 5; break;
          case 7: address |= n; break;
        }
      }
      len = 9;
    }
    /* Action Replay / raw patch: aaaaaa:dddd */
    else if ((strlen(string) >= 11) && (string[6] == ':'))
    {
      for (i = 0; i < 6; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        address |= (uint32)n << ((5 - i) * 4);
      }

      string++;
      for (i = 0; i < 4; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        data |= (uint16)(n << ((3 - i) * 4));
      }
      len = 11;
    }
  }
  else
  {
    /* ---- 8-bit systems ---- */

    /* Game Genie: ABC-DEF or ABC-DEF-GHI */
    if ((strlen(string) >= 7) && (string[3] == '-'))
    {
      for (i = 0; i < 2; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        data |= (uint16)(n << ((1 - i) * 4));
      }

      for (i = 0; i < 3; i++)
      {
        if (i == 1) string++;   /* separator */
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        address |= (uint32)n << ((2 - i) * 4);
      }

      p = valid_char(arvalidchars, *string++);
      if (!p) return 0;
      n = (int)(p - arvalidchars) & 0xF;
      n ^= 0xF;                 /* high nibble is inverted */
      address |= (uint32)n << 12;

      if (*string == '-')
      {
        /* optional reference byte */
        for (i = 0; i < 2; i++)
        {
          if (!*string) return 0;
          string++;
          p = valid_char(arvalidchars, *string++);
          if (!p) return 0;
          n = (int)(p - arvalidchars) & 0xF;
          ref |= (uint8)(n << ((1 - i) * 4));
        }
        ref = (uint8)((ref >> 2) | ((ref & 0x03) << 6));
        ref ^= 0xBA;
        len = 11;
      }
      else
      {
        len = 7;
      }
    }
    /* Action Replay: 00XX-YYYY */
    else if ((strlen(string) >= 9) && (string[4] == '-'))
    {
      string += 2;

      for (i = 0; i < 4; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        address |= (uint32)n << ((3 - i) * 4);
        if (i == 1) string++;
      }

      for (i = 0; i < 2; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        data |= (uint16)(n << ((1 - i) * 4));
      }
      len = 9;
    }
    /* Fusion RAM: aaaa:dd */
    else if ((strlen(string) >= 7) && (string[4] == ':'))
    {
      for (i = 0; i < 4; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        address |= (uint32)n << ((3 - i) * 4);
      }

      string++;
      for (i = 0; i < 2; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        data |= (uint16)(n << ((1 - i) * 4));
      }
      len = 7;
    }
    /* Fusion ROM: rraaaa:dd */
    else if ((strlen(string) >= 9) && (string[6] == ':'))
    {
      for (i = 0; i < 2; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        ref |= (uint8)(n << ((1 - i) * 4));
      }

      for (i = 0; i < 4; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        address |= (uint32)n << ((3 - i) * 4);
      }

      string++;
      for (i = 0; i < 2; i++)
      {
        p = valid_char(arvalidchars, *string++);
        if (!p) return 0;
        n = (int)(p - arvalidchars) & 0xF;
        data |= (uint16)(n << ((1 - i) * 4));
      }
      len = 9;
    }

    /* Map the Z80 Work RAM window onto a 24-bit address. */
    if (address >= 0xC000)
    {
      address = 0xFF0000 | (address & 0x1FFF);
    }
  }

  if (len)
  {
    cheats[index].address = address;
    cheats[index].data    = data;
    cheats[index].old     = ref;
  }

  return len;
}

/****************************************************************************
 * Applying and removing patches
 ****************************************************************************/

static void write_patches(void)
{
  uint8 *ptr;
  int i;

  ram_patches = 0;
  rom_patches = 0;

  if (!emu_running || !gui.cheats_enabled) return;

  for (i = 0; i < cheat_count; i++)
  {
    if (!cheats[i].enabled || !cheats[i].valid) continue;

    /* Work RAM */
    if (cheats[i].address >= 0xFF0000)
    {
      cheat_index[ram_patches++] = (uint8)i;
    }
    /* Mega CD RAM areas */
    else if ((system_hw == SYSTEM_MCD) && !scd.cartridge.boot)
    {
      if (cheats[i].address < 0x80000)
      {
        cheat_index[ram_patches++] = (uint8)i;
      }
      else if ((cheats[i].address >= 0x200000) && (cheats[i].address < 0x240000))
      {
        cheat_index[ram_patches++] = (uint8)i;
      }
    }
    /* Cartridge ROM */
    else if (cheats[i].address < cart.romsize)
    {
      if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
      {
        /* Keep the original word so the patch can be undone. */
        cheats[i].old = *(uint16 *)(cart.rom + (cheats[i].address & 0xFFFFFE));
        *(uint16 *)(cart.rom + (cheats[i].address & 0xFFFFFE)) = cheats[i].data;
      }
      else
      {
        rom_patches++;
        cheat_index[CHEAT_MAX - rom_patches] = (uint8)i;

        ptr = &z80_readmap[(cheats[i].address) >> 10][cheats[i].address & 0x03FF];

        /*
         * A basic Game Genie code carries no reference byte, so .old is 0.
         * Treated as "no check" rather than "only patch a byte that is
         * already 0" -- the latter would silently fail to apply most
         * ordinary codes until the next bank switch. Must match the same
         * condition in ROMCheatUpdate() below, or a patch applied here would
         * be second-guessed the moment banking changes.
         */
        if (!cheats[i].old || ((uint8)cheats[i].old) == *ptr)
        {
          cheats[i].orig = *ptr;      /* what to put back later */
          *ptr = (uint8)cheats[i].data;
          cheats[i].prev = ptr;
        }
        else
        {
          cheats[i].prev = NULL;
        }
      }
    }
  }

  patches_live = 1;
}

static void remove_patches(void)
{
  int i;

  if (!patches_live) return;
  patches_live = 0;

  if (!emu_running) return;

  /* Mega CD games get no ROM patches, so there is nothing to undo. */
  if ((system_hw == SYSTEM_MCD) && !scd.cartridge.boot) return;

  /* Reverse order, in case two patches share an address. */
  i = cheat_count;
  while (i > 0)
  {
    i--;

    if (!cheats[i].enabled || !cheats[i].valid) continue;
    if (cheats[i].address >= cart.romsize) continue;

    if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
    {
      *(uint16 *)(cart.rom + (cheats[i].address & 0xFFFFFE)) = cheats[i].old;
    }
    else if (cheats[i].prev != NULL)
    {
      *cheats[i].prev = cheats[i].orig;
      cheats[i].prev = NULL;
    }
  }
}

void cheats_apply(void)
{
  int i;

  remove_patches();

  ram_patches = 0;
  rom_patches = 0;

  /* Re-decode: what a code means depends on the system that is running. */
  for (i = 0; i < cheat_count; i++)
  {
    cheats[i].prev  = NULL;
    cheats[i].valid = emu_running ? (decode_cheat(cheats[i].code, i) != 0) : 0;
  }

  write_patches();
}

void cheats_suspend(void)
{
  remove_patches();
  ram_patches = 0;
  rom_patches = 0;
}

/* Called once per frame: RAM patches are overwritten by the game otherwise. */
void cheats_ram_update(void)
{
  uint8 *base;
  uint32 mask;
  int index, cnt = ram_patches;

  while (cnt)
  {
    index = cheat_index[--cnt];

    switch ((cheats[index].address >> 20) & 0xF)
    {
      case 0x0:   /* Mega CD PRG-RAM, 512 KB */
        base = scd.prg_ram;
        mask = 0x7fffe;
        break;

      case 0x2:   /* Mega CD 2M Word-RAM, 256 KB */
        base = scd.word_ram_2M;
        mask = 0x3fffe;
        break;

      default:    /* Work RAM, 64 KB */
        base = work_ram;
        mask = 0xfffe;
        break;
    }

    if (cheats[index].data & 0xFF00)
    {
      *(uint16 *)(base + (cheats[index].address & mask)) = cheats[index].data;
    }
    else
    {
      uint32 offset;

      mask |= 1;
      offset = cheats[index].address & mask;

#ifdef LSB_FIRST
      /* On the 16-bit consoles the core keeps its RAM with every pair of bytes
         swapped on a little-endian PC, so byte N lives at N ^ 1 (the word
         writes above already come out right). Without this a byte code such as
         FFFE12:0009 wrote to the neighbouring address. The 8-bit consoles' RAM
         is a plain array and is left alone. */
      if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) offset ^= 1;
#endif

      base[offset] = (uint8)cheats[index].data;
    }
  }
}

/*
 * Called by the core through the CHEATS_UPDATE macro whenever an 8-bit
 * mapper switches banks: the patched bytes live in the banked window, so the
 * old location has to be restored and the new one patched.
 */
void ROMCheatUpdate(void)
{
  int index, cnt = rom_patches;
  uint8 *ptr;

  while (cnt)
  {
    index = cheat_index[CHEAT_MAX - cnt];

    if (cheats[index].prev != NULL)
    {
      *cheats[index].prev = cheats[index].orig;
      cheats[index].prev = NULL;
    }

    ptr = &z80_readmap[(cheats[index].address) >> 10][cheats[index].address & 0x03FF];

    if (!cheats[index].old || ((uint8)cheats[index].old) == *ptr)
    {
      cheats[index].orig = *ptr;
      *ptr = (uint8)cheats[index].data;
      cheats[index].prev = ptr;
    }

    cnt--;
  }
}

/****************************************************************************
 * List management
 ****************************************************************************/

int cheats_count(void)
{
  return cheat_count;
}

const char *cheats_get_code(int index)
{
  if (index < 0 || index >= cheat_count) return "";
  return cheats[index].code;
}

const char *cheats_get_desc(int index)
{
  if (index < 0 || index >= cheat_count) return "";
  return cheats[index].desc;
}

int cheats_is_enabled(int index)
{
  if (index < 0 || index >= cheat_count) return 0;
  return cheats[index].enabled;
}

int cheats_is_valid(int index)
{
  if (index < 0 || index >= cheat_count) return 0;
  return cheats[index].valid;
}

/* Codes that are really changing the game: none while the Enable Cheats
   switch is off, whatever the individual ticks say. */
int cheats_effective(void)
{
  return gui.cheats_enabled ? cheats_active() : 0;
}

int cheats_active(void)
{
  int i, n = 0;
  for (i = 0; i < cheat_count; i++)
  {
    if (cheats[i].enabled && cheats[i].valid) n++;
  }
  return n;
}

/* Normalises to upper case and strips spaces so pasted codes just work. */
static void normalise_code(const char *in, char *out, int out_len)
{
  int n = 0;

  while (*in && n < out_len - 1)
  {
    if (*in != ' ' && *in != '\t')
    {
      out[n++] = (char)((*in >= 'a' && *in <= 'z') ? (*in - 'a' + 'A') : *in);
    }
    in++;
  }
  out[n] = '\0';
}

int cheats_add(const char *code, const char *desc)
{
  char clean[CHEAT_CODE_LEN];
  int index, i;

  if (!code || !code[0]) return -1;
  if (cheat_count >= CHEAT_MAX) return -1;

  normalise_code(code, clean, sizeof(clean));
  if (!clean[0]) return -1;

  /* A code that is already in the list is added again as a new row (for
     example to keep a copy under another name); the same patch applied twice
     changes nothing. */
  (void)i;

  remove_patches();

  index = cheat_count++;
  ZeroMemory(&cheats[index], sizeof(t_cheat));
  lstrcpynA(cheats[index].code, clean, CHEAT_CODE_LEN);
  lstrcpynA(cheats[index].desc, (desc && desc[0]) ? desc : "Unnamed", CHEAT_DESC_LEN);
  cheats[index].enabled = 1;

  if (!defer_apply) cheats_apply();
  return index;
}

void cheats_remove(int index)
{
  int i;

  if (index < 0 || index >= cheat_count) return;

  remove_patches();

  for (i = index; i < cheat_count - 1; i++)
  {
    cheats[i] = cheats[i + 1];
  }
  cheat_count--;

  cheats_apply();
}

void cheats_set_enabled(int index, int on)
{
  if (index < 0 || index >= cheat_count) return;
  if (cheats[index].enabled == (on ? 1 : 0)) return;

  remove_patches();
  cheats[index].enabled = on ? 1 : 0;
  if (!defer_apply) cheats_apply();
}

/* Switches every code on or off at once (one re-patch, not one per code). */
void cheats_set_all_enabled(int on)
{
  int i;

  remove_patches();
  for (i = 0; i < cheat_count; i++) cheats[i].enabled = on ? 1 : 0;
  cheats_apply();
}

/* Replaces the code and description of one entry where it sits, keeping its
   position and its enabled flag. Returns 0 on success, -1 for a bad index or
   empty code, -2 if a different entry already uses that code. */
static int cheats_update_entry(int index, const char *code, const char *desc)
{
  char clean[CHEAT_CODE_LEN];
  int i;

  if (index < 0 || index >= cheat_count) return -1;
  if (!code || !code[0]) return -1;

  normalise_code(code, clean, sizeof(clean));
  if (!clean[0]) return -1;

  for (i = 0; i < cheat_count; i++)
  {
    if (i != index && lstrcmpiA(cheats[i].code, clean) == 0) return -2;
  }

  /* Undo the patches while the entry still describes what was written;
     cheats_apply() then re-decodes every entry, this one included. */
  remove_patches();

  lstrcpynA(cheats[index].code, clean, CHEAT_CODE_LEN);
  lstrcpynA(cheats[index].desc, (desc && desc[0]) ? desc : "Unnamed", CHEAT_DESC_LEN);

  cheats_apply();
  return 0;
}

void cheats_remove_all(void)
{
  remove_patches();
  cheat_count = 0;
  ram_patches = 0;
  rom_patches = 0;
}

/****************************************************************************
 * Per-ROM persistence
 *
 * Plain text so the files can be edited by hand or shared:
 *
 *     1 ABCD-EFGH Infinite lives
 *     0 FFFE21:0063 Max rings
 ****************************************************************************/

/* <exe>\cheats\<Console>\<name> [<id>].cht (older builds: cheats\<name>.cht).
   for_read: the file that exists now; otherwise where to write. */
static void cheat_file_path(const char *rom_base, int for_read, char *out)
{
  (void)rom_base;   /* the loaded (or inspected) game's own name is used */
  emu_data_path("cheats", ".cht", for_read, out, GUI_PATH_LEN);
}

void cheats_load_for_rom(const char *rom_base)
{
  char path[GUI_PATH_LEN];
  char line[256];
  FILE *fp;

  cheats_remove_all();

  if (!rom_base || !rom_base[0]) return;

  cheat_file_path(rom_base, 1, path);
  fp = fopen(path, "r");
  if (!fp) return;

  defer_apply = 1;

  while (fgets(line, sizeof(line), fp))
  {
    char code[CHEAT_CODE_LEN];
    char desc[CHEAT_DESC_LEN];
    char *p = line;
    int enabled, n;

    /* A line longer than the buffer: use what fits and throw away the rest,
       instead of reading the tail as a line of its own. */
    n = (int)strlen(p);
    if (n == (int)sizeof(line) - 1 && p[n - 1] != '\n')
    {
      int c;
      while ((c = fgetc(fp)) != EOF && c != '\n') { }
    }

    /* strip the newline */
    while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r')) p[--n] = '\0';

    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#') continue;

    /* "<0|1> <code> <name>". A line that begins straight with a code (no flag,
       for example one typed in by hand) counts as switched on. */
    enabled = 1;
    if ((*p == '0' || *p == '1') && (p[1] == ' ' || p[1] == '\t'))
    {
      enabled = (*p != '0');
      p++;
    }
    while (*p == ' ' || *p == '\t') p++;

    n = 0;
    while (*p && *p != ' ' && *p != '\t' && n < CHEAT_CODE_LEN - 1) code[n++] = *p++;
    code[n] = '\0';
    if (!code[0]) continue;

    while (*p == ' ' || *p == '\t') p++;
    lstrcpynA(desc, p, CHEAT_DESC_LEN);

    /* Must go through cheats_set_enabled: cheats_add() applies the patch
       straight away, so flipping the flag behind its back would leave a
       patch written that nothing knows how to undo. */
    if (cheats_add(code, desc) >= 0)
    {
      cheats_set_enabled(cheat_count - 1, enabled);
    }
  }

  fclose(fp);
  defer_apply = 0;
  cheats_apply();
}

/****************************************************************************
 * Importing RetroArch-format .cht files
 *
 * A completely different layout from the plain per-ROM format just
 * above (which this app also happens to use the same ".cht" extension
 * for, coincidentally) -- key/value pairs, grouped by a numeric index:
 *
 *     cheats = 2
 *     cheat0_desc = "Infinite Lives"
 *     cheat0_code = "FFFE01:63"
 *     cheat0_enable = true
 *     cheat1_desc = "Infinite Energy"
 *     cheat1_code = "FFFC00:F0+FFFC02:F0"
 *     cheat1_enable = false
 ****************************************************************************/

/* Pulls out the value after '=' -- quoted ("...") for desc/code, bare
   (true/false) for enable. Handles both without needing to know which
   one is being read. */
static void cht_extract_value(const char *line, char *out, int out_len)
{
  const char *eq = strchr(line, '=');
  const char *v;
  int n;

  out[0] = '\0';
  if (!eq) return;
  eq++;

  v = strchr(eq, '"');
  if (v)
  {
    const char *end;
    v++;
    end = strchr(v, '"');
    n = end ? (int)(end - v) : (int)strlen(v);
  }
  else
  {
    while (*eq == ' ' || *eq == '\t') eq++;
    v = eq;
    n = (int)strlen(v);
    while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t' ||
                     v[n - 1] == '\r' || v[n - 1] == '\n')) n--;
  }

  if (n >= out_len) n = out_len - 1;
  if (n < 0) n = 0;
  memcpy(out, v, n);
  out[n] = '\0';
}

/* RetroArch allows several address:value patches in one cheat, joined
   with '+' -- GPGX's own code field is one patch per entry with no
   equivalent multi-patch syntax, so each '+'-separated piece becomes
   its own entry here, all sharing the same description. */
static int cht_add_split_code(const char *code, const char *desc, int enabled)
{
  char part[CHEAT_CODE_LEN];
  const char *p = code;
  int added = 0;

  while (*p)
  {
    int n = 0;
    while (*p && *p != '+' && n < CHEAT_CODE_LEN - 1) part[n++] = *p++;
    part[n] = '\0';
    if (*p == '+') p++;

    if (part[0])
    {
      int idx = cheats_add(part, desc);
      if (idx >= 0)
      {
        cheats_set_enabled(idx, enabled);
        added++;
      }
    }
  }

  return added;
}

/* Returns the number of cheat entries actually added (a multi-patch
   RetroArch cheat counts as more than one), or -1 if the file couldn't
   be opened at all. */
int cheats_import_retroarch_cht(const char *path)
{
  FILE *fp;
  char line[512];
  char cur_desc[CHEAT_DESC_LEN];
  char cur_code[256];
  int  cur_enable;
  int  cur_n;
  int  have_entry;
  int  added;

  fp = fopen(path, "r");
  if (!fp) return -1;

  defer_apply = 1;

  cur_desc[0]  = '\0';
  cur_code[0]  = '\0';
  cur_enable   = 1;
  cur_n        = -1;
  have_entry   = 0;
  added        = 0;

  while (fgets(line, sizeof(line), fp))
  {
    char *p = line;
    char value[256];
    int digits, this_n;

    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#' || *p == ';') continue;
    if (strncmp(p, "cheat", 5) != 0) continue;
    p += 5;

    digits = 0;
    while (p[digits] >= '0' && p[digits] <= '9') digits++;
    if (digits == 0) continue;   /* the "cheats = N" total-count line has no index */

    this_n = atoi(p);
    p += digits;

    if (this_n != cur_n)
    {
      /* Starting a new index -- flush whatever was accumulated for the
         previous one first. */
      if (have_entry && cur_code[0])
        added += cht_add_split_code(cur_code, cur_desc[0] ? cur_desc : "Unnamed", cur_enable);

      cur_desc[0] = '\0';
      cur_code[0] = '\0';
      cur_enable  = 1;
      cur_n       = this_n;
      have_entry  = 1;
    }

    if (strncmp(p, "_desc", 5) == 0)
    {
      cht_extract_value(p, value, sizeof(value));
      lstrcpynA(cur_desc, value, sizeof(cur_desc));
    }
    else if (strncmp(p, "_code", 5) == 0)
    {
      cht_extract_value(p, value, sizeof(value));
      lstrcpynA(cur_code, value, sizeof(cur_code));
    }
    else if (strncmp(p, "_enable", 7) == 0)
    {
      cht_extract_value(p, value, sizeof(value));
      cur_enable = (lstrcmpiA(value, "true") == 0);
    }
  }

  if (have_entry && cur_code[0])
    added += cht_add_split_code(cur_code, cur_desc[0] ? cur_desc : "Unnamed", cur_enable);

  fclose(fp);
  defer_apply = 0;
  cheats_apply();
  return added;
}

void cheats_save_for_rom(const char *rom_base)
{
  char path[GUI_PATH_LEN];
  FILE *fp;
  int i;

  if (!rom_base || !rom_base[0]) return;

  emu_ensure_data_dir("cheats");

  if (!cheat_count)
  {
    /* An empty list means no file -- including any older-named copy, which
       would otherwise be read back in next time. */
    int i;
    for (i = 0; i < 8; i++)
    {
      cheat_file_path(rom_base, 1, path);
      if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) break;
      DeleteFileA(path);
    }
    return;
  }

  cheat_file_path(rom_base, 0, path);
  fp = fopen(path, "w");
  if (!fp) return;

  fprintf(fp, "# Genesis Plus GX cheats\n");
  fprintf(fp, "# <enabled> <code> <description>\n");

  for (i = 0; i < cheat_count; i++)
  {
    fprintf(fp, "%d %s %s\n", cheats[i].enabled ? 1 : 0,
            cheats[i].code, cheats[i].desc);
  }

  fclose(fp);
}

/****************************************************************************
 * Dialog
 ****************************************************************************/

static void cheats_hint_text(char *out, int out_len)
{
  if (!emu_running && !emu_rom_filename()[0])
  {
    lstrcpynA(out, "Load a ROM first: how a code is read depends on the console.", out_len);
  }
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    lstrcpynA(out, "Accepted: Game Genie ABCD-EFGH, or Action Replay aaaaaa:dddd", out_len);
  }
  else
  {
    lstrcpynA(out, "Accepted: Game Genie ABC-DEF(-GHI), Action Replay 00XX-YYYY, or aaaa:dd", out_len);
  }
}

#define CHEATS_KEEP_SELECTION  (-2)   /* cheats_fill_list(): leave the selection as it is */

/* Save only means something with a cheat selected and cheats switched on. */
static void cheats_update_save_state(HWND dlg)
{
  HWND list = GetDlgItem(dlg, IDC_CHEATS_LIST);
  int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);

  EnableWindow(GetDlgItem(dlg, IDC_CHEATS_SAVE),
               (gui.cheats_enabled && sel >= 0 && sel < cheat_count) ? TRUE : FALSE);
}

/* The checkbox in the "Code" column header is a small picture (unticked or
   ticked) shown at the left of the title, drawn here so that it looks right in
   both light and dark mode and does not depend on any header checkbox support. */
#define HDR_BOX 16

static HIMAGELIST hdr_images;

static void cheats_make_header_images(void)
{
  static const COLORREF key = RGB(255, 0, 255);
  HDC screen = GetDC(NULL);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bm = CreateCompatibleBitmap(screen, HDR_BOX * 2, HDR_BOX);
  HGDIOBJ old = SelectObject(mem, bm);
  int dark = theme_is_dark();
  COLORREF line = dark ? RGB(220, 220, 220) : RGB(51, 51, 51);
  COLORREF fill = dark ? RGB(32, 32, 32)    : RGB(255, 255, 255);
  HBRUSH kb = CreateSolidBrush(key), fb = CreateSolidBrush(fill), lb = CreateSolidBrush(line);
  int i;

  {
    RECT all = { 0, 0, HDR_BOX * 2, HDR_BOX };
    FillRect(mem, &all, kb);
  }

  for (i = 0; i < 2; i++)
  {
    int x = i * HDR_BOX;
    RECT box = { x + 1, 1, x + 14, 14 };
    RECT inner = { x + 2, 2, x + 13, 13 };

    FillRect(mem, &box, lb);
    FillRect(mem, &inner, fb);

    if (i == 1)
    {
      HPEN pen = CreatePen(PS_SOLID, 2, line);
      HGDIOBJ op = SelectObject(mem, pen);

      MoveToEx(mem, x + 4, 7, NULL);
      LineTo(mem, x + 6, 10);
      LineTo(mem, x + 11, 4);
      SelectObject(mem, op);
      DeleteObject(pen);
    }
  }

  SelectObject(mem, old);

  if (hdr_images) ImageList_Destroy(hdr_images);
  hdr_images = ImageList_Create(HDR_BOX, HDR_BOX, ILC_COLOR24 | ILC_MASK, 2, 0);
  if (hdr_images) ImageList_AddMasked(hdr_images, bm, key);

  DeleteObject(kb); DeleteObject(fb); DeleteObject(lb);
  DeleteObject(bm);
  DeleteDC(mem);
  ReleaseDC(NULL, screen);
}

/* The state shown in the checkbox in the "Code" column header. */
static void cheats_header_set_check(HWND list, int checked)
{
  HWND header = ListView_GetHeader(list);
  HDITEMA hdi;

  if (!header || !hdr_images) return;
  memset(&hdi, 0, sizeof(hdi));
  hdi.mask = HDI_FORMAT | HDI_IMAGE;
  if (!SendMessageA(header, HDM_GETITEMA, 0, (LPARAM)&hdi)) return;

  hdi.fmt |= HDF_IMAGE;
  hdi.iImage = checked ? 1 : 0;
  SendMessageA(header, HDM_SETITEMA, 0, (LPARAM)&hdi);
}

static int cheats_all_enabled(void)
{
  int i;

  if (cheat_count == 0) return 0;
  for (i = 0; i < cheat_count; i++)
    if (!cheats[i].enabled) return 0;
  return 1;
}

/* The last column takes whatever width is left, so the header has no blank
   cell after it. Redone after every refill, because a vertical scroll bar
   appearing or going away changes how much room there is. */
static void cheats_fit_columns(HWND list)
{
  ListView_SetColumnWidth(list, 2, LVSCW_AUTOSIZE_USEHEADER);
}

/* select: an index to select afterwards, -1 for none, or
   CHEATS_KEEP_SELECTION to reselect whatever was selected before. */
static void cheats_fill_list(HWND dlg, int select)
{
  HWND list = GetDlgItem(dlg, IDC_CHEATS_LIST);
  int i;
  int top = ListView_GetTopIndex(list);

  if (select == CHEATS_KEEP_SELECTION)
  {
    select = ListView_GetNextItem(list, -1, LVNI_SELECTED);
  }

  list_refilling = 1;
  SendMessage(list, LVM_DELETEALLITEMS, 0, 0);

  for (i = 0; i < cheat_count; i++)
  {
    LVITEMA item;

    ZeroMemory(&item, sizeof(item));
    item.mask     = LVIF_TEXT;
    item.iItem    = i;
    item.iSubItem = 0;
    item.pszText  = (LPSTR)cheats[i].code;
    SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);

    item.iSubItem = 1;
    item.pszText  = (LPSTR)cheats[i].desc;
    SendMessageA(list, LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)&item);

    item.iSubItem = 2;
    item.pszText  = (LPSTR)(cheats[i].valid ? "Active" : "Not valid here");
    SendMessageA(list, LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)&item);

    ListView_SetCheckState(list, i, cheats[i].enabled ? TRUE : FALSE);
  }

  /* Scroll to the end first, then back up to the previously-topmost item --
     EnsureVisible on an item that's now below the viewport scrolls up just
     enough to bring it into view, which lands it at the top rather than
     merely "somewhere visible". Harmless when top was already 0. */
  if (cheat_count > 0)
  {
    ListView_EnsureVisible(list, cheat_count - 1, FALSE);
    ListView_EnsureVisible(list, (top < cheat_count) ? top : cheat_count - 1, FALSE);
  }

  if (select >= 0 && select < cheat_count)
  {
    ListView_SetItemState(list, select, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
  }

  list_refilling = 0;

  cheats_header_set_check(list, cheats_all_enabled());
  cheats_fit_columns(list);
  cheats_update_save_state(dlg);
}

/* Selecting a cheat puts it in the Name and Code fields, ready to edit. */
static void cheats_show_selected(HWND dlg, int index)
{
  if (index < 0 || index >= cheat_count) return;

  SetDlgItemTextA(dlg, IDC_CHEATS_CODE, cheats[index].code);
  SetDlgItemTextA(dlg, IDC_CHEATS_DESC, cheats[index].desc);
  SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Change the name or code, then press Save.");
}

#define WM_CHEATS_ALL (WM_APP + 21)   /* wParam: 1 = tick every code, 0 = untick every code */

/* The header has to be watched directly: a click on its checkbox is turned
   into a message for the dialog. */
static LRESULT CALLBACK cheats_header_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                           UINT_PTR id, DWORD_PTR ref)
{
  if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
  {
    RECT r;
    int x = (int)(short)LOWORD(lp), y = (int)(short)HIWORD(lp);

    /* On the box at the left of the first column's title. */
    if (SendMessageA(hwnd, HDM_GETITEMRECT, 0, (LPARAM)&r) &&
        x >= r.left && x < r.left + HDR_BOX + 8 && y >= r.top && y < r.bottom)
    {
      if (gui.cheats_enabled)
        PostMessage((HWND)ref, WM_CHEATS_ALL, cheats_all_enabled() ? 0 : 1, 0);
      return 0;    /* the click is the checkbox's, not a column click */
    }
  }
  else if (msg == WM_NCDESTROY)
  {
    RemoveWindowSubclass(hwnd, cheats_header_proc, id);
  }

  return DefSubclassProc(hwnd, msg, wp, lp);
}

static void cheats_init_list(HWND dlg)
{
  HWND list = GetDlgItem(dlg, IDC_CHEATS_LIST);
  LVCOLUMNA col;

  SendMessage(list, LVM_SETEXTENDEDLISTVIEWSTYLE,
              LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT,
              LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

  ZeroMemory(&col, sizeof(col));
  col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

  col.iSubItem = 0; col.cx = 150; col.pszText = (LPSTR)"Code";
  SendMessageA(list, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);

  col.iSubItem = 1; col.cx = 255; col.pszText = (LPSTR)"Description";
  SendMessageA(list, LVM_INSERTCOLUMNA, 1, (LPARAM)&col);

  col.iSubItem = 2; col.cx = 90;  col.pszText = (LPSTR)"Status";   /* widened to fill by cheats_fit_columns() */
  SendMessageA(list, LVM_INSERTCOLUMNA, 2, (LPARAM)&col);

  /* A checkbox beside the "Code" title that ticks or unticks every code. */
  {
    HWND header = ListView_GetHeader(list);

    if (header)
    {
      cheats_make_header_images();
      if (hdr_images) SendMessageA(header, HDM_SETIMAGELIST, 0, (LPARAM)hdr_images);
      SetWindowSubclass(header, cheats_header_proc, 1, (DWORD_PTR)dlg);
      cheats_header_set_check(list, 0);
    }
  }

  cheats_fit_columns(list);
}

static void cheats_do_add(HWND dlg)
{
  char code[CHEAT_CODE_LEN] = "";
  char desc[CHEAT_DESC_LEN] = "";
  int index;

  GetDlgItemTextA(dlg, IDC_CHEATS_CODE, code, sizeof(code));
  GetDlgItemTextA(dlg, IDC_CHEATS_DESC, desc, sizeof(desc));

  if (!code[0])
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Type a code before adding it.");
    return;
  }

  index = cheats_add(code, desc);

  if (index < 0)
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT,
                    (cheat_count >= CHEAT_MAX) ? "The list is full." : "That code could not be added.");
    return;
  }

  cheats_fill_list(dlg, -1);

  SetDlgItemTextA(dlg, IDC_CHEATS_CODE, "");
  SetDlgItemTextA(dlg, IDC_CHEATS_DESC, "");
  SetFocus(GetDlgItem(dlg, IDC_CHEATS_CODE));

  if (!cheats_is_valid(index))
  {
    /* Kept in the list rather than dropped: it may be a code for another
       console, and losing what the user typed is worse than showing it. */
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT,
                    "Added, but that code is not valid for the running game.");
  }
  else
  {
    char hint[128];
    cheats_hint_text(hint, sizeof(hint));
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, hint);
  }
}

/* Writes the Name and Code fields back into the selected cheat, in place: the
   entry keeps its position in the list and its checkbox state. */
static void cheats_do_save(HWND dlg)
{
  HWND list = GetDlgItem(dlg, IDC_CHEATS_LIST);
  int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);
  char code[CHEAT_CODE_LEN] = "";
  char desc[CHEAT_DESC_LEN] = "";
  char hint[128];
  int result;

  if (sel < 0 || sel >= cheat_count)
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Select a code to edit it.");
    return;
  }

  GetDlgItemTextA(dlg, IDC_CHEATS_CODE, code, sizeof(code));
  GetDlgItemTextA(dlg, IDC_CHEATS_DESC, desc, sizeof(desc));

  if (!code[0])
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Type a code before saving it.");
    return;
  }

  result = cheats_update_entry(sel, code, desc);

  if (result == -2)
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Another entry already uses that code.");
    return;
  }
  if (result < 0)
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "That code could not be saved.");
    return;
  }

  /* Nothing left selected, and the fields emptied, like after Add. Leaving
     the row selected would also mean clicking it again changes nothing, so
     its fields could not be filled in a second time without picking another
     row first. The entry itself stays exactly where it was. */
  cheats_fill_list(dlg, -1);

  SetDlgItemTextA(dlg, IDC_CHEATS_CODE, "");
  SetDlgItemTextA(dlg, IDC_CHEATS_DESC, "");

  cheats_save_current();

  if (!cheats_is_valid(sel))
  {
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Saved, but that code is not valid for the running game.");
  }
  else
  {
    cheats_hint_text(hint, sizeof(hint));
    SetDlgItemTextA(dlg, IDC_CHEATS_HINT, hint);
  }
}

/* Grays out everything in the dialog except the checkbox itself and
   Close when the master toggle is off -- there's nothing useful to do
   with individual cheats while they're all globally disabled. */
static void cheats_update_enable_state(HWND dlg)
{
  static const int controlled[] =
  {
    IDC_CHEATS_DESC, IDC_CHEATS_CODE,
    IDC_CHEATS_ADD, IDC_CHEATS_REMOVE, IDC_CHEATS_CLEAR, IDC_CHEATS_LOADCHT
  };
  int i;
  HWND list = GetDlgItem(dlg, IDC_CHEATS_LIST);
  BOOL on = gui.cheats_enabled ? TRUE : FALSE;

  CheckDlgButton(dlg, IDC_CHEATS_ENABLE, on ? BST_CHECKED : BST_UNCHECKED);

  for (i = 0; i < (int)(sizeof(controlled) / sizeof(controlled[0])); i++)
  {
    EnableWindow(GetDlgItem(dlg, controlled[i]), on);
  }

  /* The list is never disabled: a disabled list view keeps its normal text
     colour in light mode and paints itself in system colours (white on
     white) in dark mode. Instead it stays enabled, is kept inert by refusing
     its item changes (see cheats_proc), and is grayed out by its text colour
     on the same background as before. */
  {
    int dark = theme_is_dark();
    COLORREF normal = dark ? RGB(240, 240, 240) : RGB(0, 0, 0);
    COLORREF gray   = dark ? RGB(120, 120, 120) : GetSysColor(COLOR_GRAYTEXT);

    EnableWindow(list, TRUE);
    ListView_SetTextColor(list, on ? normal : gray);
    InvalidateRect(list, NULL, TRUE);
  }

  cheats_update_save_state(dlg);
}

static INT_PTR CALLBACK cheats_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_INITDIALOG:
    {
      char hint[128];

      cheats_init_list(dlg);
      cheats_fill_list(dlg, -1);
      theme_apply_to_window(dlg);
      {
        HWND header = ListView_GetHeader(GetDlgItem(dlg, IDC_CHEATS_LIST));
        if (header && theme_is_dark())
        {
          SetWindowTheme(header, L"ItemsView", NULL);
          InvalidateRect(header, NULL, TRUE);
        }
      }
      cheats_hint_text(hint, sizeof(hint));
      SetDlgItemTextA(dlg, IDC_CHEATS_HINT, hint);
      SendDlgItemMessage(dlg, IDC_CHEATS_CODE, EM_LIMITTEXT, CHEAT_CODE_LEN - 1, 0);
      SendDlgItemMessage(dlg, IDC_CHEATS_DESC, EM_LIMITTEXT, CHEAT_DESC_LEN - 1, 0);
      cheats_update_enable_state(dlg);
      return TRUE;
    }

    case WM_DESTROY:
      if (hdr_images) { ImageList_Destroy(hdr_images); hdr_images = NULL; }
      return FALSE;

    case WM_CHEATS_ALL:
      if (gui.cheats_enabled)
      {
        cheats_set_all_enabled(wp ? 1 : 0);
        cheats_fill_list(dlg, CHEATS_KEEP_SELECTION);
      }
      return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HWND ctrl = (HWND)lp;
      int ctrl_id = GetDlgCtrlID(ctrl);
      HBRUSH br;

      if (ctrl_id == IDC_CHEATS_CODE || ctrl_id == IDC_CHEATS_DESC)
        br = theme_ctlcolor_custom((HDC)wp, RGB(0x38, 0x38, 0x38), 1);
      else
        br = theme_ctlcolor((HDC)wp);

      if (br) return (LRESULT)br;
      break;
    }

    case WM_NOTIFY:
    {
      NMHDR *hdr = (NMHDR *)lp;
      LRESULT hdr_result = theme_header_customdraw(hdr);

      if (hdr_result != -1)
      {
        SetWindowLongPtr(dlg, DWLP_MSGRESULT, hdr_result);
        return TRUE;
      }

      /* With cheats switched off the list stays enabled (see
         cheats_update_enable_state), so it has to be kept inert here: refuse
         every change a click or key would make. Our own refills are exempt. */
      if (hdr->idFrom == IDC_CHEATS_LIST && hdr->code == LVN_ITEMCHANGING
          && !list_refilling && !gui.cheats_enabled)
      {
        SetWindowLongPtr(dlg, DWLP_MSGRESULT, TRUE);
        return TRUE;
      }

      if (hdr->idFrom == IDC_CHEATS_LIST && hdr->code == LVN_ITEMCHANGED
          && !list_refilling)
      {
        NMLISTVIEW *nm = (NMLISTVIEW *)lp;

        /* State change carries the checkbox in the upper state bits. */
        if (nm->uChanged & LVIF_STATE)
        {
          int was = (int)((nm->uOldState & LVIS_STATEIMAGEMASK) >> 12);
          int now = (int)((nm->uNewState & LVIS_STATEIMAGEMASK) >> 12);

          /* A newly selected cheat is shown in the Name and Code fields. */
          if ((nm->uNewState & LVIS_SELECTED) && !(nm->uOldState & LVIS_SELECTED))
          {
            cheats_show_selected(dlg, nm->iItem);
          }

          if (was && now && was != now)
          {
            cheats_set_enabled(nm->iItem, (now == 2));
            cheats_fill_list(dlg, CHEATS_KEEP_SELECTION);
          }
          else
          {
            cheats_update_save_state(dlg);
          }
        }
      }
      return FALSE;
    }

    case WM_COMMAND:
      switch (LOWORD(wp))
      {
        case IDC_CHEATS_ENABLE:
          gui.cheats_enabled = (IsDlgButtonChecked(dlg, IDC_CHEATS_ENABLE) == BST_CHECKED);
          config_save();
          cheats_apply();
          cheats_update_enable_state(dlg);
          return TRUE;

        case IDC_CHEATS_ADD:
          cheats_do_add(dlg);
          return TRUE;

        case IDC_CHEATS_SAVE:
          cheats_do_save(dlg);
          return TRUE;

        case IDC_CHEATS_REMOVE:
        {
          HWND list = GetDlgItem(dlg, IDC_CHEATS_LIST);
          int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);

          if (sel >= 0)
          {
            cheats_remove(sel);
            cheats_fill_list(dlg, -1);
          }
          else
          {
            SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Select a code to remove it.");
          }
          return TRUE;
        }

        case IDC_CHEATS_CLEAR:
          if (cheat_count &&
              MessageBoxA(dlg, "Remove every code from this list?",
                          "Cheats", MB_YESNO | MB_ICONQUESTION) == IDYES)
          {
            cheats_remove_all();
            cheats_fill_list(dlg, -1);
          }
          return TRUE;

        case IDC_CHEATS_LOADCHT:
        {
          char file[GUI_PATH_LEN] = "";
          char cheat_dir[GUI_PATH_LEN];
          int result;

          /* Always open in ./cheats (next to the exe). */
          CreateDirectoryA(osd_path("cheats"), NULL);
          lstrcpynA(cheat_dir, osd_path("cheats"), sizeof(cheat_dir));

          if (!gui_pick_file(dlg, GUI_PICK_CHEAT, "Load Cheat File (RetroArch .cht format)",
                             "RetroArch cheat files\0*.cht\0All files\0*.*\0\0",
                             cheat_dir, file, sizeof(file)))
            return TRUE;

          result = cheats_import_retroarch_cht(file);
          cheats_fill_list(dlg, CHEATS_KEEP_SELECTION);

          if (result < 0)
          {
            SetDlgItemTextA(dlg, IDC_CHEATS_HINT, "Couldn't open that file.");
          }
          else if (result == 0)
          {
            SetDlgItemTextA(dlg, IDC_CHEATS_HINT,
                            "No cheats found -- expected RetroArch's cheatN_code format.");
          }
          else
          {
            char msg[96];
            wsprintfA(msg, "Loaded %d code%s from file.", result, result == 1 ? "" : "s");
            SetDlgItemTextA(dlg, IDC_CHEATS_HINT, msg);
          }
          return TRUE;
        }

        case IDOK:
          EndDialog(dlg, IDOK);
          return TRUE;

        case IDCANCEL:
          EndDialog(dlg, IDCANCEL);
          return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDOK);
      return TRUE;
  }

  return FALSE;
}

void dlg_cheats(HWND parent)
{
  gui_dialog_box(gui.large_ui ? IDD_CHEATS_LARGE : IDD_CHEATS, parent, cheats_proc);

  /* The list is edited live, so there is nothing to commit -- just persist. */
  cheats_save_current();

  if (cheats_effective())
  {
    gui_status("%d cheat%s active", cheats_effective(), (cheats_effective() == 1) ? "" : "s");
  }
  else
  {
    gui_status("No cheats active");
  }
}
