// earned.c
// Which achievements the save has earned, the earned.txt report, unlock queueing and the F11/F12 replays.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

int g_state[NSLOTS]; int g_haveStates = 0; int g_replayNext = 0; int g_bfTotal = 0;
// True when all 44 game achievements are earned (this unlocks the final one).
int all_earned(void) { if (!g_haveStates) return 0; for (int s = 0; s < NSLOTS; s++) if (g_state[s] != 2) return 0; return 1; }
// True if achievement number s is earned on the loaded save.
int slot_earned(int s) { return s == FINAL_SLOT ? all_earned() : (g_haveStates && g_state[s] == 2); }
// Reads the earned/not earned state of all 44 achievements from the game's memory.
int read_states(int* st) {
  uint32_t am = rdv(A_ACC_MANAGER); if (!am) return 0;
  uint32_t arr = rdv(rdv(rdv(am + 8) + 4) + 8); if (!arr) return 0;
  for (int s = 0; s < NSLOTS; s++) { uint32_t o = rdv(rdv(arr + 4 * s)); if (!o) return 0; st[s] = (int)rdv(o + 8); }
  return 1;
}
// Reads shown.txt to find which achievements have already had a popup.
void read_shown(int* shown) {            // slots that have had a popup (from shown.txt)
  memset(shown, 0, NALL * sizeof(int));
  char p[MAX_PATH]; if (!hist_path(p, sizeof p, "shown.txt")) return;
  FILE* f = fopen(p, "r"); if (!f) return;
  char line[512]; while (fgets(line, sizeof line, f)) { char d[32], t[32]; int sl; if (sscanf(line, "%31s %31s %d", d, t, &sl) == 3 && sl >= 0 && sl < NALL) shown[sl] = 1; }
  fclose(f);
}
// Rewrites earned.txt for the loaded save (earned / earned before / not earned).
void write_earned_file(void) {
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
// Re-reads the achievement states and updates earned.txt when something changed.
void refresh_states(void) {
  if (!g_save[0]) return;
  if (g_awaitLoad) { uint32_t w = rdv(rdv(A_WORLD_PTR) + 0xFC); if (w == 0 || w == 0x1E3889FE || w == 0x1A06968B) return; g_awaitLoad = 0; }   // the states belong to the new save only once its world is loaded
  int st[NSLOTS]; if (!read_states(st)) return;
  int changed = !g_haveStates || memcmp(st, g_state, sizeof st) != 0;
  memcpy(g_state, st, sizeof st); g_haveStates = 1;
  if (changed) write_earned_file();
}
// Notices when the player loads a different save and switches the history over to it.
void check_save(void) {
  char nm[40]; if (!read_save_name(nm, sizeof nm)) return;
  if (strcmp(nm, g_save) == 0) return;
  logf_("save changed: %s -> %s", g_save[0] ? g_save : "(none)", nm);
  snprintf(g_save, sizeof g_save, "%s", nm);
  EnterCriticalSection(&g_cs); g_npend = 0; LeaveCriticalSection(&g_cs);   // popups queued for the previous save must not show
  g_haveStates = 0; g_awaitLoad = 1; g_replayNext = 0; g_bfTotal = 0; hist_mkdir(); cleanup_histories();
}
// Queues a popup unless that achievement is already waiting.
void queue_unique(int slot) {
  EnterCriticalSection(&g_cs);
  int dup = 0; for (int i = 0; i < g_npend; i++) if (g_pending[i] == slot) dup = 1;
  if (!dup && g_npend < 256) g_pending[g_npend++] = slot;
  LeaveCriticalSection(&g_cs);
}
// F11: replays the next earned achievement.
void replay_next(void) {                  // next earned achievement after the last one replayed
  refresh_states();
  if (!g_haveStates) { logf_("replay: achievement states not read yet"); return; }
  for (int i = 0; i < NALL; i++) { int s = (g_replayNext + i) % NALL; if (slot_earned(s)) { queue_unique(s); g_replayNext = (s + 1) % NALL; logf_("replay slot %d", s); return; } }
  logf_("replay: no earned achievement on this save");
}
// F12: shows every earned achievement that has not had a popup yet.
void replay_new(void) {                   // every earned achievement that has not had a popup yet
  refresh_states();
  if (!g_haveStates) { logf_("replay: achievement states not read yet"); return; }
  int shown[NALL]; read_shown(shown); int n = 0;
  for (int s = 0; s < NALL; s++) if (slot_earned(s) && !shown[s]) { queue_unique(s); n++; }   // the final achievement is last, so it comes after the 44
  logf_("replay: %d earned-before achievements queued", n);
}
