// ac1ach.h - shared declarations for every file of the AC1Ach mod.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
//
// Source map:
//   main.c            DllMain and the thread that installs the hooks
//   addresses.c       game addresses for the DX10 and DX9 exe, select_build()
//   patch.c           helpers that write hooks into the running game
//   hooks.c           pump() (per-frame update), is-active hook, achievement-unlocked hook
//   gate.c            "is the player in Altair gameplay?" checks
//   popup.c           builds and queues the game's own popup message, starts the sound
//   picture.c         png/jpg -> 512x256 BC2 pixels
//   texture_swap.c    swaps the popup picture into the game's flag texture
//   earned.c          earned states, earned.txt report, unlock queue, F11/F12 replays
//   saves.c           per-save history folders
//   defender.c        fix for the Defender of the People achievements
//   config.c          AC1Ach.ini and hotkeys
//   achievement_text.c default titles and descriptions
//   state.c           shared runtime state
//   util.c            logging, memory reads/writes
#ifndef AC1ACH_H
#define AC1ACH_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

// ---- constants and types ----
#define FLAG_SPEC_ID    0x1837D619   // id of the game's flag-collected popup picture
#define NSLOTS 44                    // the game's own achievements
#define FINAL_SLOT 44                // mod-only final achievement: unlock all the others
#define NALL 45
typedef void (__attribute__((thiscall)) *fn_afs_update)(void*);
typedef int  (__attribute__((thiscall)) *fn_enqueue)(void*, void*, int);
typedef void*(__attribute__((thiscall)) *fn_msg)(void*);
typedef void*(__attribute__((thiscall)) *fn_assign)(void*, void*, int, int);
typedef void*(__attribute__((thiscall)) *fn_handleget)(void*, unsigned, int);
typedef struct { DWORD proxy; union { wchar_t buf[8]; wchar_t* ptr; } u; DWORD size; DWORD cap; } MsWStr;   // the game's wstring

// ---- game addresses (addresses.c) ----
extern uintptr_t A_AFS_VT11, A_AFS_UPDATE, A_AFS_VT15, A_AFS_ISACTIVE;
extern uintptr_t A_XENON_VT9, A_XENON_STUB;
extern uintptr_t A_ENQUEUE, A_MSG_CTOR, A_MSG_DTOR, A_WSTR_ASSIGN;
extern uintptr_t A_HANDLE_MGR, A_HANDLE_GET;
extern uintptr_t A_ACC_MANAGER, A_PLAYER_TABLE, A_WTM, A_WORLD_PTR, A_MENU_MGR;
extern uintptr_t A_TEXLIST, A_TEXMAP_VT;
extern uintptr_t A_POST_REGION, A_POST_GAME, A_OBJ_HANDLE, A_CUR_WORLD;
extern uintptr_t A_FM_PATCH, A_REG_FUNC, A_SAVE_MGR;
extern int g_build;                  // 10 = DX10 exe, 9 = DX9 exe
extern uint32_t g_regResume;         // A_REG_FUNC + 7 (used by the detour in texture_swap.c)
int select_build(void);

// ---- shared state (state.c) ----
extern char g_dir[MAX_PATH];         // folder of the .asi
extern char g_mod[MAX_PATH];         // folder of the .asi plus the AC1Ach subfolder
extern CRITICAL_SECTION g_cs;
extern int  g_pending[256]; extern int g_npend;
extern void* g_afs;
extern DWORD g_frame;
extern ULONGLONG g_lastShow;
extern double g_gapSec;
extern int g_soundDelay;
extern char g_soundFile[MAX_PATH];
extern int g_soundArmed;
extern int g_soundCountdown; extern char g_soundPath[MAX_PATH];
extern uint8_t* g_pix;               // 0x20000 bytes BC2
extern int g_swapActive;
extern ULONGLONG g_forceUntil;
extern int g_wantShow;
extern wchar_t g_titleW[256]; extern int g_applied;
extern uint8_t* g_orig;              // the game's own flag picture pixels (saved before the first swap)
extern DWORD g_gameMs; extern ULONGLONG g_lastTick; extern DWORD g_swapUntil; extern int g_firstSwap;
extern int g_ready;

// ---- achievement_text.c ----
extern const char* DEF_TITLE[NALL];
extern const char* DEF_DESC[NALL];

// ---- util.c ----
void logf_(const char* fmt, ...);
int rd32(uintptr_t a, uint32_t* out);
int wr32(uintptr_t a, uint32_t v);
uint32_t rdv(uintptr_t a);

// ---- config.c ----
extern int g_key[3];                 // virtual-key codes: replay next earned, show earned-before-mod ones, reset history (0 = off)
void ini_get(const char* sec, const char* key, const char* def, char* out, int n);
void write_default_ini(void);
int parse_key(const char* n);
void load_settings(void);

// ---- picture.c ----
uint8_t* load_slot_picture(int slot);

// ---- texture_swap.c ----
extern uint32_t g_flagEntry;
extern int g_lastStage;
int front_is_ours(void);
int apply_pixels(const uint8_t* src, int backup);
int flag_resident(void);
int try_swap(void);
void on_register(uint32_t tm);
void reg_hook(void);

// ---- saves.c ----
extern char g_save[40];              // current save name ("" = none known)
extern int g_awaitLoad;              // a save switch happened and the states have not been read from the new save yet
int read_save_name(char* out, int n);
int hist_path(char* out, int n, const char* file);
void hist_mkdir(void);
void cleanup_histories(void);

// ---- popup.c ----
void make_wstr(MsWStr* w, const char* utf8);
void show_popup(int slot);
void queue_slot(int slot);

// ---- earned.c ----
extern int g_state[NSLOTS]; extern int g_haveStates; extern int g_replayNext; extern int g_bfTotal;
int all_earned(void);
int slot_earned(int s);
int read_states(int* st);
void read_shown(int* shown);
void write_earned_file(void);
void refresh_states(void);
void check_save(void);
void queue_unique(int slot);
void replay_next(void);
void replay_new(void);

// ---- gate.c ----
int in_game_focus(void);
int in_altair_gameplay(void);
void log_gate(void);

// ---- defender.c ----
extern int g_fmPatched;
int patch_fm_event(void);
void defender_pass(void);

// ---- hooks.c ----
unsigned char __fastcall hook_isactive(void* self);
void __fastcall hook_update(void* self);
void __fastcall hook_unlock(void* output, void* edx, void* acc);

// ---- patch.c ----
int patch_code_jmp(uintptr_t addr, const uint8_t* expect, int n, void* to);
int patch_ptr(uintptr_t addr, uintptr_t expect, void* hook);

#endif
