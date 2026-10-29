// defender.c
// Fix for the three Defender of the People achievements (Acre, Jerusalem, Damascus): the game counts every free-mission rescue as the same one.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// The game posts the "free mission done" event 0xF4 as a plain event whose value is always 0, so the three Defender monitors
// (Acre, Jerusalem, Damascus), which count distinct values, never get past one entry. The 261 event right after it is a region
// event carrying the mission handle. The patch posts 0xF4 the same way, so each rescue is a distinct entry.
int g_fmPatched = 0;
// Rewrites the 54 bytes where the game posts "free mission done" so every rescue counts separately. Only patches if the original bytes match exactly.
int patch_fm_event(void) {
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
// Fixes rescues made before the mod: removes bad entries from the saved list and re-posts completed rescues that are missing.
void defender_pass(void) {
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
