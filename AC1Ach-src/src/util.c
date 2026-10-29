// util.c
// Logging and safe memory reads/writes.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// Appends a timestamped line to AC1Ach.log.
void logf_(const char* fmt, ...) {
  char p[MAX_PATH]; snprintf(p, sizeof p, "%sAC1Ach.log", g_mod);
  FILE* f = fopen(p, "a"); if (!f) return;
  SYSTEMTIME t; GetLocalTime(&t);
  fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
  va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
  fputc('\n', f); fclose(f);
}
// Reads 4 bytes of game memory without crashing if the address is bad.
int rd32(uintptr_t a, uint32_t* out) {
  SIZE_T n = 0; return ReadProcessMemory(GetCurrentProcess(), (void*)a, out, 4, &n) && n == 4;
}
// Writes 4 bytes into game memory.
int wr32(uintptr_t a, uint32_t v) {
  SIZE_T n = 0; return WriteProcessMemory(GetCurrentProcess(), (void*)a, &v, 4, &n) && n == 4;
}
// Reads 4 bytes of game memory and returns them (0 if unreadable). Used everywhere to follow the game's pointers.
uint32_t rdv(uintptr_t a) { uint32_t v = 0; rd32(a, &v); return v; }
