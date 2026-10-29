// patch.c
// Helpers that write hooks into the running game: vtable pointer patches and 5-byte code jumps.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

// Overwrites game code with a jump to our function, after checking the original bytes are what we expect.
int patch_code_jmp(uintptr_t addr, const uint8_t* expect, int n, void* to) {
  uint8_t cur[16]; SIZE_T r = 0; if (!ReadProcessMemory(GetCurrentProcess(), (void*)addr, cur, n, &r) || memcmp(cur, expect, n)) return 0;
  uint8_t p[16]; memset(p, 0x90, sizeof p); p[0] = 0xE9; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (addr + 5));
  DWORD old; if (!VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
  memcpy((void*)addr, p, n); VirtualProtect((void*)addr, n, old, &old); FlushInstructionCache(GetCurrentProcess(), (void*)addr, n); return 1;
}
// Replaces one pointer in a game table (vtable) with our function, after checking its current value.
int patch_ptr(uintptr_t addr, uintptr_t expect, void* hook) {
  if (rdv(addr) != expect) return 0;
  DWORD old; if (!VirtualProtect((void*)addr, 4, PAGE_READWRITE, &old)) return 0;
  *(void**)addr = hook; VirtualProtect((void*)addr, 4, old, &old); return 1;
}
