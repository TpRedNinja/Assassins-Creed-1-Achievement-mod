// config.c
// AC1Ach.ini: writing the default file, reading settings and parsing the hotkeys.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// Reads one value from AC1Ach.ini.
void ini_get(const char* sec, const char* key, const char* def, char* out, int n) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod);
  GetPrivateProfileStringA(sec, key, def, out, n, p);
}
// Creates AC1Ach.ini with the default titles and descriptions the first time the mod runs.
void write_default_ini(void) {
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
int g_key[3];                 // virtual-key codes: replay next earned, show earned-before-mod ones, reset history (0 = off)
// Turns a key name from the ini ("F11", "INS", "A") into a Windows virtual-key code.
int parse_key(const char* n) {
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
// Reads the sound, timing and hotkey settings from the ini.
void load_settings(void) {
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
