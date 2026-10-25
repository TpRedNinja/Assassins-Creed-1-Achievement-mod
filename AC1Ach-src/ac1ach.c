// AC1Ach - restores the 44 leftover console achievements of Assassin's Creed (2007, PC) as in-game popups.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). Released under the MIT licence (see LICENSE): keep this notice and credit the author.
//
// Built as an .asi plugin (loaded by Ultimate ASI Loader / any dinput8 ASI loader). Works in the DX10 and the DX9 executable (the address set is chosen at start).
//  - Hooks XenonAccomplishmentOutput vtable slot 9, which the game calls when one of the 44 achievements completes.
//  - Shows the game's own flag-collected popup with custom text, a custom picture (swapped into the flag texture)
//    and a custom sound, built from scratch (no flag has to be collected first).
//  - Hooks AFSHandler vtable slot 11 (per-frame update) and slot 15 (is active) and detours the TextureMap register
//    function to swap the picture pixels.
// Build: i686-w64-mingw32-gcc -O2 -shared -o AC1Ach.asi ac1ach.c -lwinmm -static-libgcc
// stb_image.h (public domain / MIT, Sean Barrett) is included for reading the picture files.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "stb_image.h"

// ---- game addresses (imagebase 0x400000): one set for the DX10 exe and one for the DX9 exe, chosen at start ----
#define FLAG_SPEC_ID    0x1837D619
static uintptr_t A_AFS_VT11, A_AFS_UPDATE, A_AFS_VT15, A_AFS_ISACTIVE;   // AFSHandler vtable slot 11 (per-frame update) and slot 15 (is active)
static uintptr_t A_XENON_VT9, A_XENON_STUB;                              // XenonAccomplishmentOutput vtable slot 9 (called when an achievement completes), retn 4
static uintptr_t A_ENQUEUE, A_MSG_CTOR, A_MSG_DTOR, A_WSTR_ASSIGN;       // enqueue thiscall(afs, msg, type); message ctor/dtor thiscall(msg); wstring assign thiscall(dest, src, 0, -1) retn 0xC
static uintptr_t A_HANDLE_MGR, A_HANDLE_GET;                             // handle manager object; find-or-create thiscall(mgr, id, 0) retn 8, adds a ref
static uintptr_t A_ACC_MANAGER;                                          // AccomplishmentManager static
static uintptr_t A_PLAYER_TABLE;                                         // [[x]+0xA8] low byte = PlayerID (1 Altair, 9 Desmond)
static uintptr_t A_WTM;                                                  // [[[x]+0x12C]+0xC] = loading world id (0 idle)
static uintptr_t A_WORLD_PTR;                                            // [[x]+0xFC] = current world id
static uintptr_t A_MENU_MGR;                                             // g_AssassinMenuManager; byte +92 is set while a menu / the pause screen is up
static uintptr_t A_TEXLIST;                                              // texture list (0 = not used)
static uintptr_t A_TEXMAP_VT;                                            // TextureMap vtable
static uintptr_t A_POST_REGION, A_POST_GAME;                             // cdecl(eventId, handle): region event with a handle (ev+8 = id, ev+0x50 = handle); cdecl(eventId, value): plain game event
static uintptr_t A_OBJ_HANDLE;                                           // thiscall(mgr, object) retn 4 -> handle of the object
static uintptr_t A_CUR_WORLD;                                            // cdecl() -> current world handle
static uintptr_t A_FM_PATCH;                                             // FreeMission completion: push 0 / push 0F4h / call PostGameEvent ... (54 bytes, replaced)
static uintptr_t A_REG_FUNC;                                             // TextureMap register function; first 7 bytes are 64 8B 15 2C 00 00 00
static uintptr_t A_SAVE_MGR;                                             // [x] = SaveGameManager, +0x9C = SaveGameWriter, +0x1C = current save file name (no ".sav")
static int g_build = 0;                                                  // 10 = DX10 exe, 9 = DX9 exe
uint32_t g_regResume = 0;                                                // A_REG_FUNC + 7 (used by the detour)

static int rd32(uintptr_t a, uint32_t* out);
static int select_build(void) {
  uint32_t v;
  if (rd32(0x0174719C, &v) && v == 0x00F19F50) {            // DX10
    A_AFS_VT11 = 0x0174719C; A_AFS_UPDATE = 0x00F19F50; A_AFS_VT15 = 0x017471AC; A_AFS_ISACTIVE = 0x008DF3E0;
    A_XENON_VT9 = 0x0169A244; A_XENON_STUB = 0x005FF030;
    A_ENQUEUE = 0x00F19BC0; A_MSG_CTOR = 0x00E27E50; A_MSG_DTOR = 0x00E27C60; A_WSTR_ASSIGN = 0x0043DF90;
    A_HANDLE_MGR = 0x029A32A8; A_HANDLE_GET = 0x008EEA70;
    A_ACC_MANAGER = 0x0199EA18; A_PLAYER_TABLE = 0x0199FE30; A_WTM = 0x0199EC00; A_WORLD_PTR = 0x029C2EE8; A_MENU_MGR = 0x0199E900;
    A_TEXLIST = 0x019A0F54; A_TEXMAP_VT = 0x016AD2AC;
    A_POST_REGION = 0x00FD4BE0; A_POST_GAME = 0x00FD4AF0; A_OBJ_HANDLE = 0x008EE990; A_CUR_WORLD = 0x004586B0; A_FM_PATCH = 0x00E14899;
    A_REG_FUNC = 0x00844B30; A_SAVE_MGR = 0x029A335C;
    return g_build = 10;
  }
  if (rd32(0x016FE938, &v) && v == 0x00D47110) {            // DX9
    A_AFS_VT11 = 0x016FE938; A_AFS_UPDATE = 0x00D47110; A_AFS_VT15 = 0x016FE948; A_AFS_ISACTIVE = 0x0078DC80;
    A_XENON_VT9 = 0x01691BA0; A_XENON_STUB = 0x005C1340;
    A_ENQUEUE = 0x00D46D80; A_MSG_CTOR = 0x00C53E90; A_MSG_DTOR = 0x00C53CA0; A_WSTR_ASSIGN = 0x008759B0;
    A_HANDLE_MGR = 0x01A1E218; A_HANDLE_GET = 0x0090E5E0;
    A_ACC_MANAGER = 0x01A11A68; A_PLAYER_TABLE = 0x01A12FB0; A_WTM = 0x01A11BF0; A_WORLD_PTR = 0x01A29F58; A_MENU_MGR = 0x01A28450;
    A_TEXLIST = 0; A_TEXMAP_VT = 0x016C0C00;
    A_POST_REGION = 0x00D5F080; A_POST_GAME = 0x00D5EF90; A_OBJ_HANDLE = 0x0090E500; A_CUR_WORLD = 0x00419880; A_FM_PATCH = 0x00C408D9;
    A_REG_FUNC = 0x00A64070; A_SAVE_MGR = 0x01A1E2CC;
    return g_build = 9;
  }
  return 0;
}

#define NSLOTS 44            // the game's own achievements
#define FINAL_SLOT 44        // mod-only final achievement: unlock all the others
#define NALL 45
typedef void (__attribute__((thiscall)) *fn_afs_update)(void*);
typedef int  (__attribute__((thiscall)) *fn_enqueue)(void*, void*, int);
typedef void*(__attribute__((thiscall)) *fn_msg)(void*);
typedef void*(__attribute__((thiscall)) *fn_assign)(void*, void*, int, int);
typedef void*(__attribute__((thiscall)) *fn_handleget)(void*, unsigned, int);

typedef struct { DWORD proxy; union { wchar_t buf[8]; wchar_t* ptr; } u; DWORD size; DWORD cap; } MsWStr;

static const char* DEF_TITLE[NALL] = {
"The Eagle and The Apple - 1191","Visions of the Future","Personal Vendetta","Keeper of the Lions Passant","Keeper of the Creed",
"Keeper of the Four Gospels","Keeper of the Crescent","Absolute Symbiosis","The Blood of a Corrupt Merchant","The Blood Of A Doctor",
"The Blood of a Slave Trader","The Blood of a Merchant King","The Blood of a Regent","The Blood of a Liege-Lord","The Blood of a Teutonic Leader",
"The Blood of a Scribe","The Blood of a Nemesis","Fearless","Hungerer of Knowledge","Defender of the People: Acre",
"Defender of the People: Jerusalem","Defender of the People: Damascus","Conversationalist","Disciple of the Creed","Eagle's Will",
"Eagle's Flight","Eagle's Prey","Blade in the Crowd","Eagle's Challenge","Eagle's Swiftness",
"Eagle's Dive","Eagle's Talon","Eagle's Dance","The hands of a Thief","March of the Pious",
"Eagle's Eye","Enemy of the Poor","Gifted Escapist","Keeper of the Black Cross","Keeper of the Order",
"Keeper of the 8 Virtues","Welcome to the Animus","Hero of Masyaf","The Punishment for Treason",
"Nothing is True, Everything is Permitted"};
static const char* DEF_DESC[NALL] = {
"Complete Assassin's Creed.","A strange vision has appeared to you. What could it mean?","Kill every Templar.","Find All of Richard's Flags in the Kingdom.","Find All Flags in Masyaf.",
"Find All Flags in Jerusalem.","Find All Flags in Damascus.","Have a complete Synchronization bar.","You've slain Tamir, Black Market Merchant in Damascus.","You've slain Garnier de Naplouse, Hospitalier Leader in Acre.",
"You've slain Talal, Slave Trader of Jerusalem.","You've slain Abul Nuqoud, Merchant King of Damascus.","You've slain Majd Addin, Regent of Jerusalem.","You've slain William of Montferrat, Liege-Lord of Acre.","You've slain Sibrand, the Teutonic Leader of Acre.",
"You've slain Jubair, the Scribe of Damascus.","You've slain Robert de Sable, but there is one more...","Complete all Reach High Points.","See 85% of all the memory glitches.","Complete every free mission in Acre.",
"Complete every free mission in Jerusalem.","Complete every free mission in Damascus.","Go through every dialog with Lucy.","Assassinate all your targets with a full DNA bar.","Defeat 100 opponents without dying.",
"Last 10 minutes in open conflict.","Assassinate 100 guards.","Kill one of your main targets like a true assassin.","Defeat 25 guards in a single fight.","Perform 100 Counter Kill in Fights.",
"Perform 50 Combo Kills in Fights.","Perform 50 stealth assassinations.","Perform 50 leap of faith.","Pickpocket 200 throwing knives.","Use Scholar blending 20 times.",
"Kill 75 guards by throwing knives.","Grab and Throw 25 Harassers.","Jump through 20 merchant stands.","Find All Teutonic Flags in Acre.","Find all Templar Flags in Acre.",
"Find All Hospitalier Flags in Acre.","You've successfully completed the Animus tutorial.","You've protected Masyaf from the Templar invasion.","You have found the traitor and have brought him before Al Mualim.",
"Unlock all other achievements."};

// ---- state ----
static char g_dir[MAX_PATH];        // folder of the .asi
static char g_mod[MAX_PATH];        // folder of the .asi plus the AC1Ach subfolder
static CRITICAL_SECTION g_cs;
static int  g_pending[256]; static int g_npend = 0;
static void* g_afs = NULL;
static DWORD g_frame = 0;
static ULONGLONG g_lastShow = 0;
static double g_gapSec = 9.5;
static int g_soundDelay = 30;
static char g_soundFile[MAX_PATH] = "xbox360.wav";
static int g_soundArmed = 0;
static int g_soundCountdown = 0; static char g_soundPath[MAX_PATH];
static uint8_t* g_pix = NULL;       // 0x20000 bytes BC2
static int g_swapActive = 0;
static ULONGLONG g_forceUntil = 0;
static int g_wantShow = 0;
static wchar_t g_titleW[256]; static int g_applied = 0;
static uint8_t* g_orig = NULL;            // the game's own flag picture pixels (saved before the first swap)
static DWORD g_gameMs = 0; static ULONGLONG g_lastTick = 0; static DWORD g_swapUntil = 0; static int g_firstSwap = 0;
static int g_ready = 0;

static void logf_(const char* fmt, ...) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.log", g_mod);
  FILE* f = fopen(p, "a"); if (!f) return;
  SYSTEMTIME t; GetLocalTime(&t);
  fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
  va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
  fputc('\n', f); fclose(f);
}
static int rd32(uintptr_t a, uint32_t* out) {
  SIZE_T n = 0; return ReadProcessMemory(GetCurrentProcess(), (void*)a, out, 4, &n) && n == 4;
}
static int wr32(uintptr_t a, uint32_t v) {
  SIZE_T n = 0; return WriteProcessMemory(GetCurrentProcess(), (void*)a, &v, 4, &n) && n == 4;
}
static uint32_t rdv(uintptr_t a) { uint32_t v = 0; rd32(a, &v); return v; }

// ---- config ----
static void ini_get(const char* sec, const char* key, const char* def, char* out, int n) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod);
  GetPrivateProfileStringA(sec, key, def, out, n, p);
}
static void write_default_ini(void) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod);
  if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) return;
  FILE* f = fopen(p, "w"); if (!f) return;
  fprintf(f, "; AC1 achievements. Save this file as UTF-8. Edit freely; delete a line to get its default back.\n");
  fprintf(f, "; Text:     Title and Description of each [NN] section is what the popup shows (translate them here).\n");
  fprintf(f, "; Pictures: AC1Ach\\Pictures\\NN.png (00 to 44) is used by default; set Picture=name.png in a [NN] section to use another file there.\n");
  fprintf(f, "; Sounds:   put .wav files in AC1Ach\\Sounds; Sound= under [Settings] is the default for all, Sound= in a [NN] section overrides it, Sound=none is silent.\n");
  fprintf(f, "[Settings]\nSound=xbox360.wav\nSoundDelayFrames=30\nGapSeconds=9.5\n\n");
  for (int i = 0; i < NALL; i++) fprintf(f, "[%02d]\nTitle=%s\nDescription=%s\n\n", i, DEF_TITLE[i], DEF_DESC[i]);
  fclose(f);
}
static int g_key[3];                 // virtual-key codes: replay next earned, show earned-before-mod ones, reset history (0 = off)
static int parse_key(const char* n) {
  if (!n[0] || !_stricmp(n, "none")) return 0;
  if ((n[0] == 'F' || n[0] == 'f') && atoi(n + 1) >= 1 && atoi(n + 1) <= 12) return VK_F1 + atoi(n + 1) - 1;
  if (!_stricmp(n, "INS") || !_stricmp(n, "INSERT")) return VK_INSERT;
  if (!_stricmp(n, "DEL") || !_stricmp(n, "DELETE")) return VK_DELETE;
  if (!_stricmp(n, "HOME")) return VK_HOME;
  if (!_stricmp(n, "END")) return VK_END;
  if (!_stricmp(n, "PGUP")) return VK_PRIOR;
  if (!_stricmp(n, "PGDN")) return VK_NEXT;
  if (n[1] == 0) { char c = n[0]; if (c >= 'a' && c <= 'z') c -= 32; return c; }   // single letter or digit
  return 0;
}
static void load_settings(void) {
  char b[MAX_PATH];
  ini_get("Settings", "Sound", "xbox360.wav", g_soundFile, sizeof g_soundFile);
  ini_get("Settings", "SoundDelayFrames", "30", b, sizeof b); g_soundDelay = atoi(b);
  ini_get("Settings", "GapSeconds", "9.5", b, sizeof b); g_gapSec = atof(b);
  { char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod); ini_get("44", "Title", "", b, sizeof b);      // the final achievement was added after the first ini files were written
    if (!b[0]) { WritePrivateProfileStringA("44", "Title", DEF_TITLE[FINAL_SLOT], p); WritePrivateProfileStringA("44", "Description", DEF_DESC[FINAL_SLOT], p); } }
  static const char* keyNames[3] = { "ReplayNextKey", "ReplayNewKey", "ResetHistoryKey" };
  static const char* keyDefs[3] = { "F11", "F12", "F9" };
  for (int i = 0; i < 3; i++) {
    ini_get("Settings", keyNames[i], "", b, sizeof b);
    if (i == 2 && !b[0]) {         // ResetHistoryKey deletes the save's popup history, so it ships switched off: only a commented-out example line is written
      char p[MAX_PATH], c[16]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod); ini_get("Settings", "; ResetHistoryKey", "", c, sizeof c);
      if (!c[0]) WritePrivateProfileStringA("Settings", "; ResetHistoryKey", "F9", p);
    } else if (!b[0]) { char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod); WritePrivateProfileStringA("Settings", keyNames[i], keyDefs[i], p); strcpy(b, keyDefs[i]); }
    g_key[i] = parse_key(b);
  }
}

// ---- text ----
static void make_wstr(MsWStr* w, const char* utf8) {
  memset(w, 0, sizeof *w);
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0); if (n < 1) n = 1;
  wchar_t* tmp = (wchar_t*)calloc(n + 1, sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, tmp, n);
  int len = (int)wcslen(tmp);
  if (len < 8) { memcpy(w->u.buf, tmp, (len + 1) * 2); w->cap = 7; free(tmp); }
  else { w->u.ptr = tmp; w->cap = len; }
  w->size = len;
}

// ---- picture: png/jpg -> flipped 512x256 BC2 ----
static uint16_t to565(int r, int g, int b) { return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }
static void from565(uint16_t c, int* r, int* g, int* b) {
  *r = ((c >> 11) & 31) * 255 / 31; *g = ((c >> 5) & 63) * 255 / 63; *b = (c & 31) * 255 / 31;
}
static uint8_t* encode_bc2(const uint8_t* rgba /*512*256*4, already flipped*/) {
  uint8_t* out = (uint8_t*)malloc(0x20000); uint8_t* o = out;
  for (int by = 0; by < 64; by++) for (int bx = 0; bx < 128; bx++) {
    const uint8_t* px[16]; for (int i = 0; i < 16; i++) px[i] = rgba + ((by * 4 + i / 4) * 512 + bx * 4 + i % 4) * 4;
    for (int r = 0; r < 4; r++) {
      uint16_t w = 0; for (int x = 0; x < 4; x++) w |= (uint16_t)((px[r * 4 + x][3] >> 4) << (4 * x));
      *o++ = w & 255; *o++ = w >> 8;
    }
    int lmax = -1, lmin = 1 << 30, imax = 0, imin = 0, any = 0;
    for (int i = 0; i < 16; i++) { if (px[i][3] == 0) continue; any = 1;
      int l = px[i][0] * 299 + px[i][1] * 587 + px[i][2] * 114;
      if (l > lmax) { lmax = l; imax = i; } if (l < lmin) { lmin = l; imin = i; } }
    uint16_t c0 = any ? to565(px[imax][0], px[imax][1], px[imax][2]) : 0;
    uint16_t c1 = any ? to565(px[imin][0], px[imin][1], px[imin][2]) : 0;
    int pr[4], pg[4], pb[4]; from565(c0, &pr[0], &pg[0], &pb[0]); from565(c1, &pr[1], &pg[1], &pb[1]);
    for (int k = 0; k < 3; k++) { /*placeholder*/ }
    pr[2] = (2 * pr[0] + pr[1]) / 3; pg[2] = (2 * pg[0] + pg[1]) / 3; pb[2] = (2 * pb[0] + pb[1]) / 3;
    pr[3] = (pr[0] + 2 * pr[1]) / 3; pg[3] = (pg[0] + 2 * pg[1]) / 3; pb[3] = (pb[0] + 2 * pb[1]) / 3;
    *o++ = c0 & 255; *o++ = c0 >> 8; *o++ = c1 & 255; *o++ = c1 >> 8;
    for (int r = 0; r < 4; r++) { uint8_t bits = 0;
      for (int x = 0; x < 4; x++) { const uint8_t* p = px[r * 4 + x]; int best = 0, bd = 1 << 30;
        for (int k = 0; k < 4; k++) { int dr = p[0] - pr[k], dg = p[1] - pg[k], db = p[2] - pb[k]; int d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = k; } }
        bits |= (uint8_t)(best << (2 * x)); }
      *o++ = bits; }
  }
  return out;
}
static double cubic_w(double t) { t = t < 0 ? -t : t; return t < 1 ? 1.5 * t * t * t - 2.5 * t * t + 1 : t < 2 ? -0.5 * t * t * t + 2.5 * t * t - 4 * t + 2 : 0; }
static uint8_t* build_picture(const char* path) {
  int w, h, n; uint8_t* img = stbi_load(path, &w, &h, &n, 4); if (!img) return NULL;
  uint8_t* canvas = (uint8_t*)calloc(512 * 256, 4);
  double s = 256.0 / (w > h ? w : h); int dw = (int)(w * s + 0.5), dh = (int)(h * s + 0.5); if (dw > 512) dw = 512; if (dh > 256) dh = 256;
  int ox = (512 - dw) / 2, oy = (256 - dh) / 2;
  uint8_t* flipped = (uint8_t*)calloc(512 * 256, 4);
  for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
    double fx = (x + 0.5) / s - 0.5, fy = (y + 0.5) / s - 0.5;      // source position of this output pixel
    double acc[4] = { 0, 0, 0, 0 };
    if (s >= 1.0) {                                                  // enlarging: bicubic (Catmull-Rom) instead of blocky pixels
      int ix = (int)floor(fx), iy = (int)floor(fy); double tx = fx - ix, ty = fy - iy;
      double wx[4], wy[4];
      for (int k = 0; k < 4; k++) { wx[k] = cubic_w(k - 1 - tx); wy[k] = cubic_w(k - 1 - ty); }
      for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
        int px = ix - 1 + i, py = iy - 1 + j; if (px < 0) px = 0; if (px >= w) px = w - 1; if (py < 0) py = 0; if (py >= h) py = h - 1;
        const uint8_t* p = img + (py * w + px) * 4; double wgt = wx[i] * wy[j], a = p[3] / 255.0;
        acc[0] += wgt * p[0] * a; acc[1] += wgt * p[1] * a; acc[2] += wgt * p[2] * a; acc[3] += wgt * a;   // premultiplied so transparent edges do not bleed
      }
    } else {                                                         // shrinking: average the covered source pixels
      int x0 = (int)floor(x / s), x1 = (int)ceil((x + 1) / s), y0 = (int)floor(y / s), y1 = (int)ceil((y + 1) / s);
      if (x1 > w) x1 = w; if (y1 > h) y1 = h; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
      double cnt = 0;
      for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        const uint8_t* p = img + (py * w + px) * 4; double a = p[3] / 255.0;
        acc[0] += p[0] * a; acc[1] += p[1] * a; acc[2] += p[2] * a; acc[3] += a; cnt++;
      }
      for (int c = 0; c < 4; c++) acc[c] /= cnt;
    }
    uint8_t* o = canvas + ((oy + y) * 512 + ox + x) * 4;
    double a = acc[3]; if (a < 0) a = 0; if (a > 1) a = 1;
    for (int c = 0; c < 3; c++) { double v = a > 0.0001 ? acc[c] / acc[3] : 0; o[c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5); }
    o[3] = (uint8_t)(a * 255 + 0.5);
  }
  for (int y = 0; y < 256; y++) memcpy(flipped + (255 - y) * 512 * 4, canvas + y * 512 * 4, 512 * 4);
  uint8_t* bc2 = encode_bc2(flipped);
  free(canvas); free(flipped); stbi_image_free(img);
  return bc2;
}
static uint8_t* load_slot_picture(int slot) {
  char sec[8], name[MAX_PATH], path[MAX_PATH]; snprintf(sec, sizeof sec, "%02d", slot);
  ini_get(sec, "Picture", "", name, sizeof name);
  static const char* ext[] = { "png", "jpg", "jpeg", "bmp", "tga" };
  if (name[0]) { snprintf(path, sizeof path, "%sPictures\\%s", g_mod, name); uint8_t* r = build_picture(path); if (r) return r; }
  for (int i = 0; i < 5; i++) { snprintf(path, sizeof path, "%sPictures\\%02d.%s", g_mod, slot, ext[i]); uint8_t* r = build_picture(path); if (r) return r; }
  return NULL;
}

// ---- swap pixels of the flag picture texture (memory writes only) ----
static uint32_t g_flagEntry = 0;       // handle entry of the flag picture (cached: polling the handle function every frame must be avoided)
static int g_lastStage = -1;
static int swap_stage(int st) { if (st != g_lastStage) { g_lastStage = st; logf_("swap stage %d", st); } return 0; }
// Which message is the popup handler showing right now: 1 ours, 0 something else (a flag popup), -1 unknown.
static int front_is_ours(void) {
  uint32_t afs = (uint32_t)(uintptr_t)g_afs; if (!afs) return -1;
  uint32_t idx = rdv(afs + 356); if (idx > 5) return -1;
  if (!(rdv(afs + 8 * idx + 224) & 0x3FFF)) return -1;
  uint32_t msg = rdv(afs + 8 * idx + 220); if (!msg) return -1;
  uint32_t size = rdv(msg + 0x14), cap = rdv(msg + 0x18); if (size > 255) return 0;
  const wchar_t* p = (const wchar_t*)(uintptr_t)(cap < 8 ? msg + 4 : rdv(msg + 4)); if (!p) return -1;
  return (size == wcslen(g_titleW) && memcmp(p, g_titleW, size * 2) == 0) ? 1 : 0;
}
static int apply_pixels(const uint8_t* src, int backup) {
  if (!src) return 1;
  if (!g_flagEntry) { g_flagEntry = (uint32_t)(uintptr_t)((fn_handleget)A_HANDLE_GET)((void*)A_HANDLE_MGR, FLAG_SPEC_ID, 0);
    InterlockedDecrement((volatile LONG*)(uintptr_t)(g_flagEntry + 4)); }       // drop the ref the lookup added
  uint32_t entry = g_flagEntry;
  uint32_t spec = rdv(entry); if (!spec) return swap_stage(1);
  uint32_t h2 = rdv(spec + 0x3C); if (!h2) return swap_stage(2);
  uint32_t tm = rdv(h2); if (!tm) return swap_stage(3);
  if (rdv(tm) != A_TEXMAP_VT) { logf_("swap: texmap vtable %08X", rdv(tm)); return swap_stage(4); }
  if (rdv(tm + 8) != 0x200 || rdv(tm + 0xC) != 0x100) { logf_("swap: size %u x %u", rdv(tm + 8), rdv(tm + 0xC)); return swap_stage(5); }
  uint32_t st = rdv(tm + 0x4C); if (!st) return swap_stage(6);
  uint32_t px = rdv(st + 4); if (!px || rdv(st + 8) != 0x20000) { logf_("swap: settings px=%08X size=%08X", px, rdv(st + 8)); return swap_stage(7); }
  if (backup && !g_orig) { g_orig = (uint8_t*)malloc(0x20000); memcpy(g_orig, (void*)(uintptr_t)px, 0x20000); }
  memcpy((void*)(uintptr_t)px, src, 0x20000);
  uint32_t list = rdv(A_TEXLIST), idx = rdv(tm + 0x58);
  if (list && idx) { uint32_t arr = rdv(list + 0xC); if (arr) wr32(arr + 4 * (idx - 1), 0); }
  logf_("%s done (tm=%08X slot=%u)", backup ? "swap" : "restore", tm, idx); g_lastStage = -1;
  return 1;
}
// 1 if the shared flag picture texture is currently loaded (its GPU copy would be stale if we only changed the pixels)
static int flag_resident(void) {
  if (!g_flagEntry) return 0;                                   // not looked up yet: nothing has been shown, so nothing is loaded by us
  uint32_t spec = rdv(g_flagEntry); if (!spec) return 0;
  uint32_t h2 = rdv(spec + 0x3C); if (!h2) return 0;
  uint32_t tm = rdv(h2); if (!tm) return 0;
  return rdv(tm) == A_TEXMAP_VT && rdv(tm + 8) == 0x200 && rdv(tm + 0xC) == 0x100;
}
static int try_swap(void) { return g_pix ? apply_pixels(g_pix, 1) : 1; }

// ---- per-save history: AC1Ach\Saves\<save name>\shown.txt and earned.txt ----
#include <shlobj.h>
static char g_save[40] = "";         // current save name ("" = none known)
static int g_awaitLoad = 0;          // a save switch happened and the states have not been read from the new save yet
static int read_save_name(char* out, int n) {
  out[0] = 0;
  uint32_t m = rdv(A_SAVE_MGR); if (!m) return 0;
  uint32_t w = rdv(m + 0x9C); if (!w) return 0;
  char b[40]; SIZE_T r = 0; if (!ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)(w + 0x1C), b, 39, &r) || r < 1) return 0; b[39] = 0;
  int len = (int)strlen(b); if (len < 1) snprintf(b, sizeof b, "assassin"), len = 8;      // the game falls back to assassin.sav
  for (int i = 0; i < len; i++) if (!((b[i] >= '0' && b[i] <= '9') || (b[i] >= 'a' && b[i] <= 'z') || (b[i] >= 'A' && b[i] <= 'Z') || b[i] == '_')) return 0;
  snprintf(out, n, "%s", b); return 1;
}
static int hist_path(char* out, int n, const char* file) {       // "" when no save is known
  if (!g_save[0]) { out[0] = 0; return 0; }
  snprintf(out, n, "%sSaves\\%s\\%s", g_mod, g_save, file); return 1;
}
static void hist_mkdir(void) { char p[MAX_PATH]; snprintf(p, sizeof p, "%sSaves", g_mod); CreateDirectoryA(p, NULL); snprintf(p, sizeof p, "%sSaves\\%s", g_mod, g_save); CreateDirectoryA(p, NULL); }
// Remove the history of saves whose .sav file no longer exists. Does nothing unless the game's save folder can be listed and holds at least one .sav.
static void cleanup_histories(void) {
  char dir[MAX_PATH]; if (SHGetSpecialFolderPathA(NULL, dir, CSIDL_APPDATA, FALSE) == 0) return;
  strcat(dir, "\\Ubisoft\\Assassin's Creed\\Saved Games");
  char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s\\*.sav", dir);
  char names[512][40]; int nn = 0; WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do { if (nn < 512) { snprintf(names[nn], 40, "%s", fd.cFileName); char* d = strrchr(names[nn], '.'); if (d) *d = 0; nn++; } } while (FindNextFileA(h, &fd));
  FindClose(h); if (nn == 0) return;
  snprintf(pat, sizeof pat, "%sSaves\\*", g_mod); h = FindFirstFileA(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
    int keep = (g_save[0] && _stricmp(g_save, fd.cFileName) == 0);
    for (int i = 0; i < nn && !keep; i++) if (_stricmp(names[i], fd.cFileName) == 0) keep = 1;
    if (keep) continue;
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sSaves\\%s\\shown.txt", g_mod, fd.cFileName); DeleteFileA(p);
    snprintf(p, sizeof p, "%sSaves\\%s\\earned.txt", g_mod, fd.cFileName); DeleteFileA(p);
    snprintf(p, sizeof p, "%sSaves\\%s", g_mod, fd.cFileName);
    if (RemoveDirectoryA(p)) logf_("save %s no longer exists: its history was removed", fd.cFileName);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}

// ---- popup ----
static void write_earned_file(void);
static void show_popup(int slot) {
  char sec[8], title[256], desc[256]; snprintf(sec, sizeof sec, "%02d", slot);
  ini_get(sec, "Title", DEF_TITLE[slot], title, sizeof title);
  ini_get(sec, "Description", DEF_DESC[slot], desc, sizeof desc);
  free(g_pix); g_pix = load_slot_picture(slot);
  if (!g_pix) logf_("slot %d: no picture found in Pictures (using the existing flag picture)", slot);
  MsWStr wt, wd; make_wstr(&wt, title); make_wstr(&wd, desc);
  MultiByteToWideChar(CP_UTF8, 0, title, -1, g_titleW, 255); g_titleW[255] = 0;
  g_applied = 0;
  uint8_t msg[0x80]; memset(msg, 0, sizeof msg);
  ((fn_msg)A_MSG_CTOR)(msg);
  ((fn_assign)A_WSTR_ASSIGN)(msg + 0x00, &wt, 0, -1);
  ((fn_assign)A_WSTR_ASSIGN)(msg + 0x20, &wd, 0, -1);
  void* h = ((fn_handleget)A_HANDLE_GET)((void*)A_HANDLE_MGR, FLAG_SPEC_ID, 0); g_flagEntry = (uint32_t)(uintptr_t)h;
  uint32_t old = *(uint32_t*)(msg + 0x1C); *(void**)(msg + 0x1C) = h;
  InterlockedDecrement((volatile LONG*)(uintptr_t)(old + 4));
  *(float*)(msg + 0x3C) = 8.0f;
  g_lastStage = -1;
  int res = try_swap(); g_applied = res && g_pix;   // picture already resident: swapped now
  g_swapActive = 1; g_firstSwap = res;          // stays armed while the popup lives so a reload (pause/unpause) is swapped again
  g_swapUntil = g_gameMs + (res ? 11000 : 15000);
  ((fn_enqueue)A_ENQUEUE)(g_afs, msg, 4);
  g_forceUntil = GetTickCount64() + 3000; g_wantShow = 1;
  ((fn_msg)A_MSG_DTOR)(msg);
  if (wt.cap > 7) free(wt.u.ptr);
  if (wd.cap > 7) free(wd.u.ptr);
  // sound
  char sn[MAX_PATH]; ini_get(sec, "Sound", "", sn, sizeof sn); if (!sn[0]) snprintf(sn, sizeof sn, "%s", g_soundFile);
  if (_stricmp(sn, "none") != 0) { snprintf(g_soundPath, sizeof g_soundPath, "%sSounds\\%s", g_mod, sn); g_soundArmed = 1; }
  if (g_firstSwap || !g_pix) { g_soundCountdown = g_soundArmed ? g_soundDelay + 1 : 0; g_soundArmed = 0; }   // picture resident (or none): start now, else when the game registers the picture
  g_lastShow = GetTickCount64();
  logf_("popup shown for slot %d: %s", slot, title);
  char p[MAX_PATH]; FILE* f = hist_path(p, sizeof p, "shown.txt") ? fopen(p, "a") : NULL; if (f) { SYSTEMTIME t; GetLocalTime(&t); fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %02d  %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, slot, title); fclose(f); }
  write_earned_file();
}

static void queue_slot(int slot) {
  EnterCriticalSection(&g_cs);
  if (g_npend < 256) g_pending[g_npend++] = slot;
  LeaveCriticalSection(&g_cs);
}

// ---- earned state, history and replay ----
static int g_state[NSLOTS]; static int g_haveStates = 0; static int g_replayNext = 0; static int g_bfTotal = 0;
static int all_earned(void) { if (!g_haveStates) return 0; for (int s = 0; s < NSLOTS; s++) if (g_state[s] != 2) return 0; return 1; }
static int slot_earned(int s) { return s == FINAL_SLOT ? all_earned() : (g_haveStates && g_state[s] == 2); }
static int read_states(int* st) {
  uint32_t am = rdv(A_ACC_MANAGER); if (!am) return 0;
  uint32_t arr = rdv(rdv(rdv(am + 8) + 4) + 8); if (!arr) return 0;
  for (int s = 0; s < NSLOTS; s++) { uint32_t o = rdv(rdv(arr + 4 * s)); if (!o) return 0; st[s] = (int)rdv(o + 8); }
  return 1;
}
static void read_shown(int* shown) {            // slots that have had a popup (from shown.txt)
  memset(shown, 0, NALL * sizeof(int));
  char p[MAX_PATH]; if (!hist_path(p, sizeof p, "shown.txt")) return;
  FILE* f = fopen(p, "r"); if (!f) return;
  char line[512]; while (fgets(line, sizeof line, f)) { char d[32], t[32]; int sl; if (sscanf(line, "%31s %31s %d", d, t, &sl) == 3 && sl >= 0 && sl < NALL) shown[sl] = 1; }
  fclose(f);
}
static void write_earned_file(void) {
  if (!g_haveStates || !g_save[0]) return;
  int shown[NALL]; read_shown(shown); hist_mkdir();
  char p[MAX_PATH]; hist_path(p, sizeof p, "earned.txt");
  FILE* f = fopen(p, "w"); if (!f) return;
  fprintf(f, "Achievements on the loaded save (rewritten by the mod; one folder per save in AC1Ach\\Saves).\n"
             "earned           = completed and its popup has been shown\n"
             "earned before    = completed but no popup has been shown yet (earned before the mod, or in a previous save)\n\n");
  for (int s = 0; s < NALL; s++)
    fprintf(f, "%02d  %-44s %s\n", s, DEF_TITLE[s], slot_earned(s) ? (shown[s] ? "earned" : "earned before") : "not earned");
  fclose(f);
}
static void refresh_states(void) {
  if (!g_save[0]) return;
  if (g_awaitLoad) { uint32_t w = rdv(rdv(A_WORLD_PTR) + 0xFC); if (w == 0 || w == 0x1E3889FE || w == 0x1A06968B) return; g_awaitLoad = 0; }   // the states belong to the new save only once its world is loaded
  int st[NSLOTS]; if (!read_states(st)) return;
  int changed = !g_haveStates || memcmp(st, g_state, sizeof st) != 0;
  memcpy(g_state, st, sizeof st); g_haveStates = 1;
  if (changed) write_earned_file();
}
static void check_save(void) {
  char nm[40]; if (!read_save_name(nm, sizeof nm)) return;
  if (strcmp(nm, g_save) == 0) return;
  logf_("save changed: %s -> %s", g_save[0] ? g_save : "(none)", nm);
  snprintf(g_save, sizeof g_save, "%s", nm);
  EnterCriticalSection(&g_cs); g_npend = 0; LeaveCriticalSection(&g_cs);   // popups queued for the previous save must not show
  g_haveStates = 0; g_awaitLoad = 1; g_replayNext = 0; g_bfTotal = 0; hist_mkdir(); cleanup_histories();
}
static void queue_unique(int slot) {
  EnterCriticalSection(&g_cs);
  int dup = 0; for (int i = 0; i < g_npend; i++) if (g_pending[i] == slot) dup = 1;
  if (!dup && g_npend < 256) g_pending[g_npend++] = slot;
  LeaveCriticalSection(&g_cs);
}
static void replay_next(void) {                  // next earned achievement after the last one replayed
  refresh_states();
  if (!g_haveStates) { logf_("replay: achievement states not read yet"); return; }
  for (int i = 0; i < NALL; i++) { int s = (g_replayNext + i) % NALL; if (slot_earned(s)) { queue_unique(s); g_replayNext = (s + 1) % NALL; logf_("replay slot %d", s); return; } }
  logf_("replay: no earned achievement on this save");
}
static void replay_new(void) {                   // every earned achievement that has not had a popup yet
  refresh_states();
  if (!g_haveStates) { logf_("replay: achievement states not read yet"); return; }
  int shown[NALL]; read_shown(shown); int n = 0;
  for (int s = 0; s < NALL; s++) if (slot_earned(s) && !shown[s]) { queue_unique(s); n++; }   // the final achievement is last, so it comes after the 44
  logf_("replay: %d earned-before achievements queued", n);
}

// ---- hooks ----
static int in_game_focus(void) { DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid); return pid == GetCurrentProcessId(); }


static int hook_isactive_raw(void* self) { return ((unsigned char (__attribute__((thiscall)) *)(void*))A_AFS_ISACTIVE)(self) & 1; }
static unsigned char __fastcall hook_isactive(void* self) {
  if (self == g_afs && g_wantShow) {
    uint8_t pending = *(uint8_t*)((uintptr_t)self + 352);      // set by the enqueue, cleared when the game starts showing the message
    if (!pending || GetTickCount64() > g_forceUntil) { logf_("show wait ended (pending=%d showing=%d)", pending, *(uint8_t*)((uintptr_t)self + 268)); g_wantShow = 0; }
    else return 0;      // the game starts a queued message when it is pending and the handler is NOT active
  }
  return hook_isactive_raw(self);
}
static int in_altair_gameplay(void) {
  uint32_t pl = rdv(rdv(A_PLAYER_TABLE)); if (!pl) return 0;
  if ((rdv(pl + 0xA8) & 0xFF) != 1) return 0;                          // Altair only (value seen live: 0x3001, the low byte is the id)
  uint32_t lh = rdv(rdv(A_WTM) + 0x12C); if (lh && rdv(lh + 0xC) != 0) return 0;   // a world is loading
  { uint32_t mm = rdv(A_MENU_MGR); if (mm) { uint8_t b = 0; SIZE_T n = 0; ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)(mm + 92), &b, 1, &n); if (n == 1 && b) return 0; } }   // menu or pause screen open
  uint32_t w = rdv(rdv(A_WORLD_PTR) + 0xFC);
  if (w == 0 || w == 0x1E3889FE || w == 0x1A06968B) return 0;         // switching / main menu / loading room
  return 1;
}
// ---- Defender of the People fix ----
// The game posts the "free mission done" event 0xF4 as a plain event whose value is always 0, so the three Defender monitors
// (Acre, Jerusalem, Damascus), which count distinct values, never get past one entry. The 261 event right after it is a region
// event carrying the mission handle. The patch posts 0xF4 the same way, so each rescue is a distinct entry.
static int g_fmPatched = 0;
static int patch_fm_event(void) {
  uint8_t orig[54]; int q = 0; uintptr_t b0 = A_FM_PATCH;
  #define PUT(...) do { const uint8_t t_[] = { __VA_ARGS__ }; memcpy(orig + q, t_, sizeof t_); q += (int)sizeof t_; } while (0)
  #define PUT32(v) do { uint32_t t_ = (uint32_t)(v); memcpy(orig + q, &t_, 4); q += 4; } while (0)
  PUT(0x6A,0x00, 0x68,0xF4,0,0,0, 0xC6,0x86,0x41,1,0,0,1);                          // push 0 / push 0F4h / mov byte [esi+141h], 1
  PUT(0xE8); PUT32(A_POST_GAME - (b0 + q + 4));                                       // call PostGameEvent
  PUT(0x83,0xC4,4, 0x89,0x64,0x24,0x18, 0x8B,0xFC, 0x56, 0xB9); PUT32(A_HANDLE_MGR);  // add esp,4 / mov [esp+18h],esp / mov edi,esp / push esi / mov ecx,HandleManager
  PUT(0xE8); PUT32(A_OBJ_HANDLE - (b0 + q + 4));                                      // call object -> handle
  PUT(0x68,5,1,0,0, 0x89,7);                                                          // push 105h / mov [edi], eax
  PUT(0xE8); PUT32(A_POST_REGION - (b0 + q + 4));                                     // call PostCollectableRegionEvent
  PUT(0x83,0xC4,8);                                                                   // add esp,8
  uint8_t cur[54]; SIZE_T r = 0;
  if (q != 54 || !ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)A_FM_PATCH, cur, 54, &r) || r != 54 || memcmp(cur, orig, 54)) return 0;
  uint8_t n[54]; memset(n, 0x90, sizeof n); int o = 0; uintptr_t b = A_FM_PATCH;
  n[o++] = 0x6A; n[o++] = 0x00;                                                         // push 0               (slot that holds the handle)
  n[o++] = 0xC6; n[o++] = 0x86; n[o++] = 0x41; n[o++] = 1; n[o++] = 0; n[o++] = 0; n[o++] = 1;   // mov byte [esi+141h], 1
  n[o++] = 0x56;                                                                        // push esi
  n[o++] = 0xB9; *(uint32_t*)(n + o) = A_HANDLE_MGR; o += 4;                            // mov ecx, g_HandleManager
  n[o] = 0xE8; *(int32_t*)(n + o + 1) = (int32_t)(A_OBJ_HANDLE - (b + o + 5)); o += 5;  // call object -> handle
  n[o++] = 0x89; n[o++] = 0x04; n[o++] = 0x24;                                          // mov [esp], eax
  n[o++] = 0x68; *(uint32_t*)(n + o) = 0xF4; o += 4;                                    // push 0F4h
  n[o] = 0xE8; *(int32_t*)(n + o + 1) = (int32_t)(A_POST_REGION - (b + o + 5)); o += 5; // call PostCollectableRegionEvent(0xF4, handle)
  n[o++] = 0x83; n[o++] = 0xC4; n[o++] = 0x04;                                          // add esp, 4
  n[o++] = 0x68; *(uint32_t*)(n + o) = 0x105; o += 4;                                   // push 105h
  n[o] = 0xE8; *(int32_t*)(n + o + 1) = (int32_t)(A_POST_REGION - (b + o + 5)); o += 5; // call PostCollectableRegionEvent(261, handle)
  n[o++] = 0x83; n[o++] = 0xC4; n[o++] = 0x08;                                          // add esp, 8
  DWORD old; if (!VirtualProtect((void*)b, 54, PAGE_EXECUTE_READWRITE, &old)) return 0;
  memcpy((void*)b, n, 54); VirtualProtect((void*)b, 54, old, &old); FlushInstructionCache(GetCurrentProcess(), (void*)b, 54);
  return 1;
}
// Rescues made before the fix (and the stale entry the old behaviour left in the saved list) are corrected once the player is in
// the city: a list entry whose mission is not complete is removed, and every completed mission that is missing is posted.
static void defender_pass(void) {
  if (!g_haveStates || g_awaitLoad || g_bfTotal > 600) return;
  uint32_t am = rdv(A_ACC_MANAGER); if (!am) return;
  uint32_t arr = rdv(rdv(rdv(am + 8) + 4) + 8); if (!arr) return;
  uint32_t cur = ((uint32_t (*)(void))A_CUR_WORLD)(); if (!cur) return;
  for (int s = 19; s <= 21; s++) {
    uint32_t o = rdv(rdv(arr + 4 * s)); if (!o) continue;
    uint32_t mon = rdv(o + 0x14); if (!mon || rdv(mon + 8) != 0xF4) continue;
    if (rdv(mon + 4) & 0xFF) continue;                                    // already complete
    uint32_t tbl = rdv(mon + 0x18); if (!tbl) continue;
    uint32_t ents = rdv(tbl + 0xC), nent = rdv(tbl + 0x10) & 0x3FFF; if (!ents || nent != 1) continue;
    uint32_t wref = rdv(ents), list = rdv(ents + 4), n = rdv(ents + 8) & 0x3FFF;
    uint32_t wh = (rdv(wref + 8) & 0x10000000) ? rdv(wref) : 0;
    if (wh != cur || !list || n == 0 || n > 255) continue;               // the player is not in this city
    uint32_t used = rdv(mon + 0x1C), hdr = rdv(mon + 0x20), nused = hdr & 0x3FFF; if (!used || nused > 255) continue;
    uint8_t keep[256]; int nk = 0, removed = 0;
    for (uint32_t i = 0; i < nused; i++) {
      uint8_t u = 0; SIZE_T r = 0; if (!ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)(used + i), &u, 1, &r) || r != 1) { nk = -1; break; }
      uint32_t h = u < n ? rdv(list + 4 * u) : 0, item = h ? rdv(h) : 0;
      if (item && rdv(item + 0x44) == 3) keep[nk++] = u; else removed++;
    }
    if (nk < 0) continue;
    if (removed) {
      SIZE_T w = 0; WriteProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)used, keep, nk, &w);
      wr32(mon + 0x20, (hdr & ~0x3FFFu) | (uint32_t)nk);
      logf_("defender slot %d: removed %d stale list entries", s, removed); nused = (uint32_t)nk;
    }
    int posted = 0;
    for (uint32_t i = 0; i < n && posted < 40; i++) {
      uint32_t h = rdv(list + 4 * i); if (!h || !(rdv(h + 8) & 0x10000000)) continue;
      uint32_t item = rdv(h); if (!item || rdv(item + 0x44) != 3) continue;     // only completed rescues
      int have = 0; for (uint32_t k = 0; k < nused; k++) { uint8_t u = 0; SIZE_T r = 0; ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)(used + k), &u, 1, &r); if (r == 1 && u == (uint8_t)i) { have = 1; break; } }
      if (have) continue;
      ((void (__cdecl*)(int, int))A_POST_REGION)(0xF4, (int)h); posted++; g_bfTotal++;
    }
    if (posted) logf_("defender slot %d: posted %d completed rescues that were missing from the list", s, posted);
  }
}
static void log_gate(void) {
  uint32_t pl = rdv(rdv(A_PLAYER_TABLE)); uint32_t lh = rdv(rdv(A_WTM) + 0x12C);
  logf_("gate: player=%08X load=%08X world=%08X altair=%d", pl ? rdv(pl + 0xA8) : 0, lh ? rdv(lh + 0xC) : 0, rdv(rdv(A_WORLD_PTR) + 0xFC), in_altair_gameplay());
}
static void pump(void* self) {
  g_afs = self; g_frame++;
  if (!g_ready) { write_default_ini(); load_settings(); g_ready = 1; logf_("pump started"); }
  if (g_frame % 60 == 1) check_save();
  if (in_game_focus()) {
    static int was[3] = { 0, 0, 0 };
    for (int k = 0; k < 3; k++) {
      int d = g_key[k] && (GetAsyncKeyState(g_key[k]) & 0x8000) != 0;
      if (d && !was[k]) {
        if (k == 0) replay_next(); else if (k == 1) replay_new();
        else { char p[MAX_PATH]; if (hist_path(p, sizeof p, "shown.txt")) { DeleteFileA(p); write_earned_file(); logf_("shown.txt of save %s deleted (history reset)", g_save); } }
      }
      was[k] = d;
    }
  }
  // The popup handler only reports active while its UI clip is present (Animus gameplay). Otherwise keep everything queued.
  if (!in_altair_gameplay() || !in_game_focus()) { g_soundCountdown = 0; g_lastTick = 0; return; }
  { ULONGLONG now = GetTickCount64(); if (g_lastTick) { ULONGLONG d = now - g_lastTick; if (d > 200) d = 200; g_gameMs += (DWORD)d; } g_lastTick = now; }
  if (g_frame % 180 == 0) { refresh_states(); if (g_fmPatched) defender_pass(); }
  if (g_soundCountdown > 0 && --g_soundCountdown == 0) PlaySoundA(g_soundPath, 0, SND_FILENAME | SND_ASYNC);
  if (g_swapActive && g_firstSwap && g_gameMs <= g_swapUntil) {          // keep the shared texture matching whichever popup is on screen
    int f = front_is_ours();
    if (f == 0 && g_applied) { if (apply_pixels(g_orig, 0)) g_applied = 0; }
    else if (f == 1 && !g_applied && g_pix) { if (apply_pixels(g_pix, 1)) g_applied = 1; }
  }
  if (g_swapActive && g_gameMs > g_swapUntil) { g_swapActive = 0; g_soundArmed = 0; if (g_firstSwap) { if (g_applied) apply_pixels(g_orig, 0); g_applied = 0; } else logf_("swap hook disarmed (no matching texture registered)"); g_firstSwap = 0; }
  EnterCriticalSection(&g_cs);
  int slot = -1;
  static ULONGLONG waitSince = 0;
  int ready = 0;
  if (g_npend > 0 && (GetTickCount64() - g_lastShow) >= (ULONGLONG)(g_gapSec * 1000)) {
    // wait until the previous picture texture has been unloaded and no game popup is in front, otherwise the new picture never reaches the screen
    if (!waitSince) waitSince = GetTickCount64();
    if ((!flag_resident() && front_is_ours() != 0) || GetTickCount64() - waitSince > 25000) { ready = 1; waitSince = 0; }
  } else waitSince = 0;
  if (ready) { slot = g_pending[0]; memmove(g_pending, g_pending + 1, (--g_npend) * sizeof(int)); }
  LeaveCriticalSection(&g_cs);
  if (slot >= 0) show_popup(slot);
}
static void __fastcall hook_update(void* self) { pump(self); ((fn_afs_update)A_AFS_UPDATE)(self); }

static void __fastcall hook_unlock(void* output, void* edx, void* acc) {
  (void)output; (void)edx;
  uint32_t am = rdv(A_ACC_MANAGER), cfgs = rdv(am + 8), cfg1 = rdv(cfgs + 4), arr = rdv(cfg1 + 8);
  for (int s = 0; s < NSLOTS; s++) {
    uint32_t e = rdv(arr + 4 * s); if (rdv(e) == (uint32_t)(uintptr_t)acc) { logf_("unlock slot %d (%s)", s, DEF_TITLE[s]); if (!g_haveStates) refresh_states(); if (g_haveStates) g_state[s] = 2; queue_unique(s); if (all_earned()) { logf_("all 44 achievements earned: final achievement"); queue_unique(FINAL_SLOT); } return; }
  }
  logf_("unlock notify for an accomplishment not found in Config[1]");
}


// ---- hook at the TextureMap register function (0x844B30): swap pixels before the GPU texture is built ----
void __attribute__((used, noinline, cdecl)) on_register(uint32_t tm) {
  if (!g_swapActive || !g_pix || g_gameMs > g_swapUntil) return;
  if (rdv(tm) != A_TEXMAP_VT || rdv(tm + 8) != 0x200 || rdv(tm + 0xC) != 0x100 || rdv(tm + 0x14) != 4) return;
  uint32_t st = rdv(tm + 0x4C); if (!st) return;
  uint32_t px = rdv(st + 4); if (!px || rdv(st + 8) != 0x20000) return;
  if (front_is_ours() == 0) return;            // a flag popup (not ours) is loading its picture: leave it alone
  if (!g_orig) { g_orig = (uint8_t*)malloc(0x20000); memcpy(g_orig, (void*)(uintptr_t)px, 0x20000); }
  memcpy((void*)(uintptr_t)px, g_pix, 0x20000); g_applied = 1;
  logf_("swap done at register (tm=%08X)", tm);
  if (!g_firstSwap) { g_firstSwap = 1; g_swapUntil = g_gameMs + 11000; if (g_soundArmed) { g_soundCountdown = g_soundDelay + 1; g_soundArmed = 0; } }
}
__attribute__((naked)) void reg_hook(void) {
  __asm__ volatile(
    "pushal\n\t pushfl\n\t pushl %ecx\n\t call _on_register\n\t addl $4,%esp\n\t popfl\n\t popal\n\t"
    "movl %fs:0x2c,%edx\n\t pushl _g_regResume\n\t ret\n\t");
}
static int patch_code_jmp(uintptr_t addr, const uint8_t* expect, int n, void* to) {
  uint8_t cur[16]; SIZE_T r = 0; if (!ReadProcessMemory(GetCurrentProcess(), (void*)addr, cur, n, &r) || memcmp(cur, expect, n)) return 0;
  uint8_t p[16]; memset(p, 0x90, sizeof p); p[0] = 0xE9; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (addr + 5));
  DWORD old; if (!VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
  memcpy((void*)addr, p, n); VirtualProtect((void*)addr, n, old, &old); FlushInstructionCache(GetCurrentProcess(), (void*)addr, n); return 1;
}
static int patch_ptr(uintptr_t addr, uintptr_t expect, void* hook) {
  if (rdv(addr) != expect) return 0;
  DWORD old; if (!VirtualProtect((void*)addr, 4, PAGE_READWRITE, &old)) return 0;
  *(void**)addr = hook; VirtualProtect((void*)addr, 4, old, &old); return 1;
}
static DWORD WINAPI init_thread(LPVOID p) {
  (void)p;
  for (int i = 0; i < 600; i++) {   // wait for the exe's data to be ready (up to 60 s)
    if (select_build() && rdv(A_XENON_VT9) == A_XENON_STUB) {
      int a = patch_ptr(A_AFS_VT11, A_AFS_UPDATE, (void*)hook_update);
      int b = patch_ptr(A_XENON_VT9, A_XENON_STUB, (void*)hook_unlock);
      static const uint8_t orig[7] = { 0x64, 0x8B, 0x15, 0x2C, 0x00, 0x00, 0x00 };
      int d = patch_ptr(A_AFS_VT15, A_AFS_ISACTIVE, (void*)hook_isactive);
      g_regResume = (uint32_t)(A_REG_FUNC + 7);
      int c = patch_code_jmp(A_REG_FUNC, orig, 7, (void*)reg_hook);
      g_fmPatched = patch_fm_event();
      logf_("build: DX%d. hooks installed: afs=%d xenon=%d register=%d isactive=%d freemission=%d", g_build, a, b, c, d, g_fmPatched); return 0;
    }
    Sleep(100);
  }
  logf_("hooks NOT installed: the expected DX10 or DX9 addresses were not found");
  return 0;
}

// ---- entry ----
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID res) {
  (void)res;
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(inst);
    InitializeCriticalSection(&g_cs);
    GetModuleFileNameA(inst, g_dir, MAX_PATH); char* s = strrchr(g_dir, '\\'); if (s) s[1] = 0;   // the folder this .asi is in (the game folder or its Scripts folder); the AC1Ach folder lives next to it
    snprintf(g_mod, sizeof g_mod, "%sAC1Ach\\", g_dir);
    CreateDirectoryA(g_mod, NULL);
    char sub[MAX_PATH]; snprintf(sub, sizeof sub, "%sPictures", g_mod); CreateDirectoryA(sub, NULL);
    snprintf(sub, sizeof sub, "%sSounds", g_mod); CreateDirectoryA(sub, NULL);
    CreateThread(NULL, 0, init_thread, NULL, 0, NULL);
  }
  return TRUE;
}
