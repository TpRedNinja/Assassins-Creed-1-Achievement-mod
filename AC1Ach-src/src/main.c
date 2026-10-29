// main.c
// Entry point of the .asi: finds the mod folder next to the .asi, then installs the hooks from a worker thread once the game's data is ready.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
//
// What the mod does, in short:
// Built as an .asi plugin (loaded by Ultimate ASI Loader / any dinput8 ASI loader). Works in the DX10 and the DX9 executable (the address set is chosen at start).
//  - Hooks XenonAccomplishmentOutput vtable slot 9, which the game calls when one of the 44 achievements completes.
//  - Shows the game's own flag-collected popup with custom text, a custom picture (swapped into the flag texture)
//    and a custom sound, built from scratch (no flag has to be collected first).
//  - Hooks AFSHandler vtable slot 11 (per-frame update) and slot 15 (is active) and detours the TextureMap register
//    function to swap the picture pixels.
// Build: i686-w64-mingw32-gcc -O2 -shared -o AC1Ach.asi ac1ach.c -lwinmm -static-libgcc

#include "ac1ach.h"

// Waits for the game to be ready, picks DX9/DX10 addresses and installs every hook, then logs the result.
DWORD WINAPI init_thread(LPVOID p) {
  (void)p;
  for (int i = 0; i < 600; i++) {   // wait for the exe's data to be ready (up to 60 s)
    if (select_build() && rdv(A_XENON_VT9) == A_XENON_STUB) {
      int a = patch_ptr(A_AFS_VT11, A_AFS_UPDATE, (void*)hook_update);
      int b = patch_ptr(A_XENON_VT9, A_XENON_STUB, (void*)hook_unlock);
      static const uint8_t orig[7] = { 0x64, 0x8B, 0x15, 0x2C, 0x00, 0x00, 0x00 };
      int d = patch_ptr(A_AFS_VT15, A_AFS_ISACTIVE, (void*)hook_isactive);
      g_regResume = (uint32_t)(A_REG_FUNC + 7);
      int c = patch_code_jmp(A_REG_FUNC, orig, 7, (void*)reg_hook);
      g_fmPatched = patch_fm_event();
      logf_("build: DX%d. hooks installed: afs=%d xenon=%d register=%d isactive=%d freemission=%d", g_build, a, b, c, d, g_fmPatched); return 0;
    }
    Sleep(100);
  }
  logf_("hooks NOT installed: the expected DX10 or DX9 addresses were not found");
  return 0;
}

// ---- entry ----
// Entry point: finds the AC1Ach folder next to the .asi, creates its folders and starts init_thread.
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID res) {
  (void)res;
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(inst);
    InitializeCriticalSection(&g_cs);
    GetModuleFileNameA(inst, g_dir, MAX_PATH); char* s = strrchr(g_dir, '\\'); if (s) s[1] = 0;   // the folder this .asi is in (the game folder or its Scripts folder); the AC1Ach folder lives next to it
    snprintf(g_mod, sizeof g_mod, "%sAC1Ach\\", g_dir);
    CreateDirectoryA(g_mod, NULL);
    char sub[MAX_PATH]; snprintf(sub, sizeof sub, "%sPictures", g_mod); CreateDirectoryA(sub, NULL);
    snprintf(sub, sizeof sub, "%sSounds", g_mod); CreateDirectoryA(sub, NULL);
    CreateThread(NULL, 0, init_thread, NULL, 0, NULL);
  }
  return TRUE;
}
