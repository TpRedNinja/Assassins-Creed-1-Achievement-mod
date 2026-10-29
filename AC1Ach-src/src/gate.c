// gate.c
// Checks on the game state: is the game window focused, and are we in Altair gameplay (not a menu, loading screen or the modern day).
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// Is the game window the active window? (Hotkeys and popups only work then.)
int in_game_focus(void) { DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid); return pid == GetCurrentProcessId(); }

// True only while playing as Altair with no menu, pause screen or loading in progress. Popups wait until this is true.
int in_altair_gameplay(void) {
  uint32_t pl = rdv(rdv(A_PLAYER_TABLE)); if (!pl) return 0;
  if ((rdv(pl + 0xA8) & 0xFF) != 1) return 0;                          // Altair only (value seen live: 0x3001, the low byte is the id)
  uint32_t lh = rdv(rdv(A_WTM) + 0x12C); if (lh && rdv(lh + 0xC) != 0) return 0;   // a world is loading
  { uint32_t mm = rdv(A_MENU_MGR); if (mm) { uint8_t b = 0; SIZE_T n = 0; ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)(mm + 92), &b, 1, &n); if (n == 1 && b) return 0; } }   // menu or pause screen open
  uint32_t w = rdv(rdv(A_WORLD_PTR) + 0xFC);
  if (w == 0 || w == 0x1E3889FE || w == 0x1A06968B) return 0;         // switching / main menu / loading room
  return 1;
}
// Writes the values behind in_altair_gameplay to the log (for debugging).
void log_gate(void) {
  uint32_t pl = rdv(rdv(A_PLAYER_TABLE)); uint32_t lh = rdv(rdv(A_WTM) + 0x12C);
  logf_("gate: player=%08X load=%08X world=%08X altair=%d", pl ? rdv(pl + 0xA8) : 0, lh ? rdv(lh + 0xC) : 0, rdv(rdv(A_WORLD_PTR) + 0xFC), in_altair_gameplay());
}
