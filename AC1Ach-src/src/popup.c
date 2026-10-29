// popup.c
// Builds the game's own popup message from scratch (title, description, picture handle) and queues it; starts the sound.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// Builds a string in the game's own wide-string format.
void make_wstr(MsWStr* w, const char* utf8) {
  memset(w, 0, sizeof *w);
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0); if (n < 1) n = 1;
  wchar_t* tmp = (wchar_t*)calloc(n + 1, sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, tmp, n);
  int len = (int)wcslen(tmp);
  if (len < 8) { memcpy(w->u.buf, tmp, (len + 1) * 2); w->cap = 7; free(tmp); }
  else { w->u.ptr = tmp; w->cap = len; }
  w->size = len;
}

// Shows the popup for one achievement: builds the game's message with our title, description and picture, queues it, starts the sound and records it in shown.txt.
void show_popup(int slot) {
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

// Adds an achievement to the list of popups waiting to be shown.
void queue_slot(int slot) {
  EnterCriticalSection(&g_cs);
  if (g_npend < 256) g_pending[g_npend++] = slot;
  LeaveCriticalSection(&g_cs);
}
