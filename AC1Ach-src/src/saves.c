// saves.c
// Per-save history: finds the current save's name and keeps AC1Ach\Saves\<name>\ (shown.txt, earned.txt); removes the history of deleted saves.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"
#include <shlobj.h>

char g_save[40] = "";         // current save name ("" = none known)
int g_awaitLoad = 0;          // a save switch happened and the states have not been read from the new save yet
// Reads the name of the loaded save from the game (letters, digits and _ only).
int read_save_name(char* out, int n) {
  out[0] = 0;
  uint32_t m = rdv(A_SAVE_MGR); if (!m) return 0;
  uint32_t w = rdv(m + 0x9C); if (!w) return 0;
  char b[40]; SIZE_T r = 0; if (!ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)(w + 0x1C), b, 39, &r) || r < 1) return 0; b[39] = 0;
  int len = (int)strlen(b); if (len < 1) snprintf(b, sizeof b, "assassin"), len = 8;      // the game falls back to assassin.sav
  for (int i = 0; i < len; i++) if (!((b[i] >= '0' && b[i] <= '9') || (b[i] >= 'a' && b[i] <= 'z') || (b[i] >= 'A' && b[i] <= 'Z') || b[i] == '_')) return 0;
  snprintf(out, n, "%s", b); return 1;
}
// Builds the path of a history file for the current save.
int hist_path(char* out, int n, const char* file) {       // "" when no save is known
  if (!g_save[0]) { out[0] = 0; return 0; }
  snprintf(out, n, "%sSaves\\%s\\%s", g_mod, g_save, file); return 1;
}
// Creates the history folder for the current save.
void hist_mkdir(void) { char p[MAX_PATH]; snprintf(p, sizeof p, "%sSaves", g_mod); CreateDirectoryA(p, NULL); snprintf(p, sizeof p, "%sSaves\\%s", g_mod, g_save); CreateDirectoryA(p, NULL); }
// Remove the history of saves whose .sav file no longer exists. Does nothing unless the game's save folder can be listed and holds at least one .sav.
// Deletes the history of saves whose .sav file no longer exists.
void cleanup_histories(void) {
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
