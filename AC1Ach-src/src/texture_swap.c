// texture_swap.c
// Puts the popup picture into the game: swaps the pixels of the shared flag picture texture and detours the TextureMap register function so the swap happens before the GPU copy is built.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

uint32_t g_flagEntry = 0;       // handle entry of the flag picture (cached: polling the handle function every frame must be avoided)
int g_lastStage = -1;
static int swap_stage(int st) { if (st != g_lastStage) { g_lastStage = st; logf_("swap stage %d", st); } return 0; }
// Which message is the popup handler showing right now: 1 ours, 0 something else (a flag popup), -1 unknown.
// Is the popup the game is showing right now ours (1), a normal flag popup (0), or unknown (-1)?
int front_is_ours(void) {
  uint32_t afs = (uint32_t)(uintptr_t)g_afs; if (!afs) return -1;
  uint32_t idx = rdv(afs + 356); if (idx > 5) return -1;
  if (!(rdv(afs + 8 * idx + 224) & 0x3FFF)) return -1;
  uint32_t msg = rdv(afs + 8 * idx + 220); if (!msg) return -1;
  uint32_t size = rdv(msg + 0x14), cap = rdv(msg + 0x18); if (size > 255) return 0;
  const wchar_t* p = (const wchar_t*)(uintptr_t)(cap < 8 ? msg + 4 : rdv(msg + 4)); if (!p) return -1;
  return (size == wcslen(g_titleW) && memcmp(p, g_titleW, size * 2) == 0) ? 1 : 0;
}
// Copies picture pixels into the game's flag texture by following its pointers; reports (in the log) at which step it failed if the texture is not there.
int apply_pixels(const uint8_t* src, int backup) {
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
// Is the shared flag picture currently loaded in the game?
int flag_resident(void) {
  if (!g_flagEntry) return 0;                                   // not looked up yet: nothing has been shown, so nothing is loaded by us
  uint32_t spec = rdv(g_flagEntry); if (!spec) return 0;
  uint32_t h2 = rdv(spec + 0x3C); if (!h2) return 0;
  uint32_t tm = rdv(h2); if (!tm) return 0;
  return rdv(tm) == A_TEXMAP_VT && rdv(tm + 8) == 0x200 && rdv(tm + 0xC) == 0x100;
}
// Swaps in the current popup picture right away if the texture is already loaded.
int try_swap(void) { return g_pix ? apply_pixels(g_pix, 1) : 1; }

// ---- hook at the TextureMap register function (0x844B30): swap pixels before the GPU texture is built ----
// Called just before the game builds the GPU copy of a texture; if it is the flag picture of our popup, puts our pixels in first.
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
// Machine-code trampoline: saves registers, calls on_register, runs the instruction it replaced, then jumps back into the game.
__attribute__((naked)) void reg_hook(void) {
  __asm__ volatile(
    "pushal\n\t pushfl\n\t pushl %ecx\n\t call _on_register\n\t addl $4,%esp\n\t popfl\n\t popal\n\t"
    "movl %fs:0x2c,%edx\n\t pushl _g_regResume\n\t ret\n\t");
}
