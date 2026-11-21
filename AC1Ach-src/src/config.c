// config.c
// AC1Ach.ini (settings) and the language files (texts): writing the defaults, choosing the language, reading settings and parsing the hotkeys.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// Reads one value from AC1Ach.ini.
void ini_get(const char* sec, const char* key, const char* def, char* out, int n) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod);
  GetPrivateProfileStringA(sec, key, def, out, n, p);
}
// Creates AC1Ach.ini (the settings only) the first time the mod runs. The texts live in the language files, see write_default_language_file.
void write_default_ini(void) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod);
  if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) return;
  FILE* f = fopen(p, "w"); if (!f) return;
  fprintf(f, "; AC1 achievements settings. Save this file as UTF-8.\n");
  fprintf(f, "; Language: auto = use the language the game is set to. To force another one write its code (en fr es de it ru pl) or the file name from AC1Ach\\Languages, e.g. Language=es or Language=MyTranslation.ini.\n");
  fprintf(f, "; Text:     titles and descriptions are in the language files (AC1Ach\\Languages), one [NN] section per achievement.\n");
  fprintf(f, "; Pictures: AC1Ach\\Pictures\\NN.png (00 to 44) is used by default; set Picture=name.png in a [NN] section of the language file to use another file there.\n");
  fprintf(f, "; Sounds:   put .wav files in AC1Ach\\Sounds; Sound= under [Settings] is the default for all, Sound= in a [NN] section of the language file overrides it, Sound=none is silent.\n");
  fprintf(f, "[Settings]\nLanguage=auto\nSound=xbox360.wav\nSoundDelayFrames=30\nGapSeconds=9.5\nRequireFocus=1\n");
  fclose(f);
}
// Creates Languages\AC1Ach_en.ini with the built-in English text if it is missing (the release ships the other languages next to it).
void write_default_language_file(void) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sLanguages\\AC1Ach_en.ini", g_mod);
  if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) return;
  FILE* f = fopen(p, "w"); if (!f) return;
  fprintf(f, "; AC1 achievements - ENGLISH. Save this file as UTF-8. Title and Description of each [NN] section is what the popup shows.\n");
  fprintf(f, "; Optional per achievement: Picture=name.png (from AC1Ach\\Pictures) and Sound=name.wav (from AC1Ach\\Sounds, or none).\n\n");
  for (int i = 0; i < NALL; i++) fprintf(f, "[%02d]\nTitle=%s\nDescription=%s\n%s\n", i, DEF_TITLE[i], DEF_DESC[i], i == FINAL_SLOT ? "Sound=xbox_mythic_achievement.wav\n" : "");
  fclose(f);
}
char g_langPath[MAX_PATH];
// Picks the language file for the next popup: Language=auto (default) follows the game's own language, anything else is a code or file name from AC1Ach\Languages. Falls back to English.
void pick_language(void) {
  static const char* code[11] = { "en", "en", "fr", "es", "de", "it", "ja", "ko", "ru", "cs", "pl" };   // indexed by the game's language id
  char v[MAX_PATH], c[MAX_PATH], found[MAX_PATH] = "";
  ini_get("Settings", "Language", "auto", v, sizeof v);
  if (!v[0] || !_stricmp(v, "auto")) { uint32_t id = 1; if (!rd32(A_LANGUAGE, &id) || id < 1 || id > 10) id = 1; snprintf(v, sizeof v, "%s", code[id]); }
  static const char* fmt[] = { "%sLanguages\\AC1Ach_%s.ini", "%sLanguages\\%s.ini", "%sLanguages\\%s", "%s%s" };
  for (int i = 0; i < 4 && !found[0]; i++) { snprintf(c, sizeof c, fmt[i], g_mod, v); if (GetFileAttributesA(c) != INVALID_FILE_ATTRIBUTES) strcpy(found, c); }
  if (!found[0]) { snprintf(c, sizeof c, "%sLanguages\\AC1Ach_en.ini", g_mod); if (GetFileAttributesA(c) != INVALID_FILE_ATTRIBUTES) strcpy(found, c); }
  if (strcmp(found, g_langPath) != 0) { strcpy(g_langPath, found); logf_("language file: %s", found[0] ? found : "(built-in English)"); }
}
// Reads a per-achievement value: the language file first, then the same section of AC1Ach.ini (older ini files keep working), then the default.
void lang_get(const char* sec, const char* key, const char* def, char* out, int n) {
  out[0] = 0;
  if (g_langPath[0]) GetPrivateProfileStringA(sec, key, "", out, n, g_langPath);
  if (!out[0]) ini_get(sec, key, def, out, n);
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
  ini_get("Settings", "RequireFocus", "1", b, sizeof b); g_requireFocus = atoi(b) != 0;
  { char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.ini", g_mod); ini_get("Settings", "Language", "", b, sizeof b); if (!b[0]) WritePrivateProfileStringA("Settings", "Language", "auto", p); }   // older ini files get the new key
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
