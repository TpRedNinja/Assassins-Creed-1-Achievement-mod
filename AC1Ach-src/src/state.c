// state.c
// Shared runtime state of the mod (folders, popup queue, sound, picture swap). Everything here is declared extern in ac1ach.h.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"

char g_dir[MAX_PATH];        // folder of the .asi
char g_mod[MAX_PATH];        // folder of the .asi plus the AC1Ach subfolder
CRITICAL_SECTION g_cs;
int  g_pending[256]; int g_npend = 0;
void* g_afs = NULL;
DWORD g_frame = 0;
ULONGLONG g_lastShow = 0;
double g_gapSec = 9.5;
int g_soundDelay = 30;
char g_soundFile[MAX_PATH] = "xbox360.wav";
int g_soundArmed = 0;
int g_soundCountdown = 0; char g_soundPath[MAX_PATH];
uint8_t* g_pix = NULL;       // 0x20000 bytes BC2
int g_swapActive = 0;
ULONGLONG g_forceUntil = 0;
int g_wantShow = 0;
wchar_t g_titleW[256]; int g_applied = 0;
uint8_t* g_orig = NULL;            // the game's own flag picture pixels (saved before the first swap)
DWORD g_gameMs = 0; ULONGLONG g_lastTick = 0; DWORD g_swapUntil = 0; int g_firstSwap = 0;
int g_ready = 0;
