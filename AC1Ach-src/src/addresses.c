// addresses.c
// The game addresses. One set for the DX10 exe and one for the DX9 exe; select_build() picks the set that matches the running exe.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

uintptr_t A_AFS_VT11, A_AFS_UPDATE, A_AFS_VT15, A_AFS_ISACTIVE;   // AFSHandler vtable slot 11 (per-frame update) and slot 15 (is active)
uintptr_t A_XENON_VT9, A_XENON_STUB;                              // XenonAccomplishmentOutput vtable slot 9 (called when an achievement completes), retn 4
uintptr_t A_ENQUEUE, A_MSG_CTOR, A_MSG_DTOR, A_WSTR_ASSIGN;       // enqueue thiscall(afs, msg, type); message ctor/dtor thiscall(msg); wstring assign thiscall(dest, src, 0, -1) retn 0xC
uintptr_t A_HANDLE_MGR, A_HANDLE_GET;                             // handle manager object; find-or-create thiscall(mgr, id, 0) retn 8, adds a ref
uintptr_t A_ACC_MANAGER;                                          // AccomplishmentManager static
uintptr_t A_PLAYER_TABLE;                                         // [[x]+0xA8] low byte = PlayerID (1 Altair, 9 Desmond)
uintptr_t A_WTM;                                                  // [[[x]+0x12C]+0xC] = loading world id (0 idle)
uintptr_t A_WORLD_PTR;                                            // [[x]+0xFC] = current world id
uintptr_t A_MENU_MGR;                                             // g_AssassinMenuManager; byte +92 is set while a menu / the pause screen is up
uintptr_t A_TEXLIST;                                              // texture list (0 = not used)
uintptr_t A_TEXMAP_VT;                                            // TextureMap vtable
uintptr_t A_POST_REGION, A_POST_GAME;                             // cdecl(eventId, handle): region event with a handle (ev+8 = id, ev+0x50 = handle); cdecl(eventId, value): plain game event
uintptr_t A_OBJ_HANDLE;                                           // thiscall(mgr, object) retn 4 -> handle of the object
uintptr_t A_CUR_WORLD;                                            // cdecl() -> current world handle
uintptr_t A_FM_PATCH;                                             // FreeMission completion: push 0 / push 0F4h / call PostGameEvent ... (54 bytes, replaced)
uintptr_t A_REG_FUNC;                                             // TextureMap register function; first 7 bytes are 64 8B 15 2C 00 00 00
uintptr_t A_SAVE_MGR;                                             // [x] = SaveGameManager, +0x9C = SaveGameWriter, +0x1C = current save file name (no ".sav")
int g_build = 0;                                                  // 10 = DX10 exe, 9 = DX9 exe
uint32_t g_regResume = 0;                                                // A_REG_FUNC + 7 (used by the detour)

// Works out whether the running exe is DX10 or DX9 by looking for a known value in the exe, then fills in the matching addresses. Returns 0 if neither matches.
int select_build(void) {
  uint32_t v;
  if (rd32(0x0174719C, &v) && v == 0x00F19F50) {            // DX10
    A_AFS_VT11 = 0x0174719C; A_AFS_UPDATE = 0x00F19F50; A_AFS_VT15 = 0x017471AC; A_AFS_ISACTIVE = 0x008DF3E0;
    A_XENON_VT9 = 0x0169A244; A_XENON_STUB = 0x005FF030;
    A_ENQUEUE = 0x00F19BC0; A_MSG_CTOR = 0x00E27E50; A_MSG_DTOR = 0x00E27C60; A_WSTR_ASSIGN = 0x0043DF90;
    A_HANDLE_MGR = 0x029A32A8; A_HANDLE_GET = 0x008EEA70;
    A_ACC_MANAGER = 0x0199EA18; A_PLAYER_TABLE = 0x0199FE30; A_WTM = 0x0199EC00; A_WORLD_PTR = 0x029C2EE8; A_MENU_MGR = 0x0199E900;
    A_TEXLIST = 0x019A0F54; A_TEXMAP_VT = 0x016AD2AC;
    A_POST_REGION = 0x00FD4BE0; A_POST_GAME = 0x00FD4AF0; A_OBJ_HANDLE = 0x008EE990; A_CUR_WORLD = 0x004586B0; A_FM_PATCH = 0x00E14899;
    A_REG_FUNC = 0x00844B30; A_SAVE_MGR = 0x029A335C;
    return g_build = 10;
  }
  if (rd32(0x016FE938, &v) && v == 0x00D47110) {            // DX9
    A_AFS_VT11 = 0x016FE938; A_AFS_UPDATE = 0x00D47110; A_AFS_VT15 = 0x016FE948; A_AFS_ISACTIVE = 0x0078DC80;
    A_XENON_VT9 = 0x01691BA0; A_XENON_STUB = 0x005C1340;
    A_ENQUEUE = 0x00D46D80; A_MSG_CTOR = 0x00C53E90; A_MSG_DTOR = 0x00C53CA0; A_WSTR_ASSIGN = 0x008759B0;
    A_HANDLE_MGR = 0x01A1E218; A_HANDLE_GET = 0x0090E5E0;
    A_ACC_MANAGER = 0x01A11A68; A_PLAYER_TABLE = 0x01A12FB0; A_WTM = 0x01A11BF0; A_WORLD_PTR = 0x01A29F58; A_MENU_MGR = 0x01A28450;
    A_TEXLIST = 0; A_TEXMAP_VT = 0x016C0C00;
    A_POST_REGION = 0x00D5F080; A_POST_GAME = 0x00D5EF90; A_OBJ_HANDLE = 0x0090E500; A_CUR_WORLD = 0x00419880; A_FM_PATCH = 0x00C408D9;
    A_REG_FUNC = 0x00A64070; A_SAVE_MGR = 0x01A1E2CC;
    return g_build = 9;
  }
  return 0;
}
