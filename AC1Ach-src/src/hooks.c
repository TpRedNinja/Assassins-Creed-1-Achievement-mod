// hooks.c
// The functions the game calls into: the per-frame update (pump), the is-active check and the achievement-unlocked notification.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

static int hook_isactive_raw(void* self) { return ((unsigned char (__attribute__((thiscall)) *)(void*))A_AFS_ISACTIVE)(self) & 1; }
// Replaces the game's "is the popup handler busy" check so a queued popup is allowed to start.
unsigned char __fastcall hook_isactive(void* self) {
  if (self == g_afs && g_wantShow) {
    uint8_t pending = *(uint8_t*)((uintptr_t)self + 352);      // set by the enqueue, cleared when the game starts showing the message
    if (!pending || GetTickCount64() > g_forceUntil) { logf_("show wait ended (pending=%d showing=%d)", pending, *(uint8_t*)((uintptr_t)self + 268)); g_wantShow = 0; }
    else return 0;      // the game starts a queued message when it is pending and the handler is NOT active
  }
  return hook_isactive_raw(self);
}
// Runs every game frame (from hook_update): hotkeys, sound timing, picture swapping and starting the next queued popup.
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
// Replaces the game's per-frame update: runs pump(), then calls the original function.
void __fastcall hook_update(void* self) { pump(self); ((fn_afs_update)A_AFS_UPDATE)(self); }

// Called by the game when an achievement completes: logs it, marks it earned and queues its popup (and the final achievement when all 44 are done).
void __fastcall hook_unlock(void* output, void* edx, void* acc) {
  (void)output; (void)edx;
  uint32_t am = rdv(A_ACC_MANAGER), cfgs = rdv(am + 8), cfg1 = rdv(cfgs + 4), arr = rdv(cfg1 + 8);
  for (int s = 0; s < NSLOTS; s++) {
    uint32_t e = rdv(arr + 4 * s); if (rdv(e) == (uint32_t)(uintptr_t)acc) { logf_("unlock slot %d (%s)", s, DEF_TITLE[s]); if (!g_haveStates) refresh_states(); if (g_haveStates) g_state[s] = 2; queue_unique(s); if (all_earned()) { logf_("all 44 achievements earned: final achievement"); queue_unique(FINAL_SLOT); } return; }
  }
  logf_("unlock notify for an accomplishment not found in Config[1]");
}
