# AC1Ach

Restores the 44 leftover console achievements of Assassin's Creed (2007, PC) as in-game popups, and adds one more: **Nothing is True, Everything is Permitted**, for unlocking all the others. When an achievement completes, the game's own "flag collected" popup is shown with the achievement's title and description, a picture and a sound.

- Works in both the DX10 (`AssassinsCreed_Dx10.exe`) and the DX9 (`AssassinsCreed_Dx9.exe`) executable. The mod detects which one it is in. Every hooked address is checked before anything is written; in any other executable the mod stays idle and says so in its log.
- Achievements are tracked per save: every save has its own earned and shown history, so you can earn them again on a new save.
- Everything is editable: titles and descriptions (translations), pictures and sounds. See `SETTINGS.md`.
- Also fixes the three **Defender of the People** achievements (Acre, Jerusalem, Damascus), which never completed in the PC game.

## AI disclosure

This mod was made with AI. The reverse engineering of the game and the code were done by TpRedNinja together with Claude (Anthropic's AI model).

## Install

1. Install [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) as `dinput8.dll` in the game folder (skip this if you already use it; it loads plugins from the game folder and from a `Scripts` folder).
2. Copy the `Scripts` folder of this download into the game folder. You should end up with:
   - `Scripts\AC1Ach.asi`
   - `Scripts\AC1Ach\Pictures\00.png` to `44.png`
   - `Scripts\AC1Ach\Sounds\` (the `.wav` files)

   If you do not want a `Scripts` folder, `AC1Ach.asi` and the `AC1Ach` folder can also sit together in the game folder. The `AC1Ach` folder must always be next to `AC1Ach.asi`.
3. Start the game and play. `Scripts\AC1Ach\AC1Ach.ini` is created on the first run, and `AC1Ach.log` is written next to it.

Uninstall: delete `AC1Ach.asi` and the `AC1Ach` folder.

## Keys

- `F11`: replay the next achievement your loaded save has earned (cycles through them).
- `F12`: show every earned achievement that has not had a popup yet. The game only announces an achievement the moment it completes, so ones a save already had are never announced; this replays them.
- `ResetHistoryKey`: off by default (the ini ships `; ResetHistoryKey=F9`; remove the `; ` to switch it on). It deletes the loaded save's `shown.txt`, so every earned achievement counts as not shown again.

All keys can be changed in the ini. See `SETTINGS.md` for every setting.

## When popups appear

Only in Altair gameplay: not in menus, loading screens, the pause menu or the modern day. An achievement earned elsewhere waits in the queue and shows when you are back. If another game popup (a flag, a Templar) is on screen, the achievement waits for it to finish.

## Credits and sources

- **Mod, reverse engineering:** TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja), with Claude (AI, Anthropic).
- **Achievement pictures, titles and descriptions:** sourced from [TrueAchievements](https://www.trueachievements.com/game/Assassins-Creed/achievements).
- **Picture of the extra achievement ("Nothing is True, Everything is Permitted"):** made with ChatGPT (AI).
- **Sounds:** taken from [myinstants.com](https://www.myinstants.com), [101soundboards.com](https://www.101soundboards.com) and [zedge.net](https://www.zedge.net); MP3 files were converted to WAV with [freeconvert.com](https://www.freeconvert.com).
- **Blox:** help with the save file format (the notes refer to it).
- `stb_image.h` by Sean Barrett (public domain / MIT) reads the picture files.

Assassin's Creed is a trademark of Ubisoft. This mod is a fan project and is not affiliated with or endorsed by Ubisoft. The pictures, titles and sounds belong to their respective owners and are not covered by the MIT licence below.

## How it works

See `ac1ach.c` (it is commented) and the reverse engineering notes. In short: a vtable pointer of `XenonAccomplishmentOutput` (empty on PC) receives the completed accomplishment; the popup is built with the game's own message constructor, the picture pixels are swapped into the flag texture when the game registers it, and the popup is only shown in Altair gameplay. The Defender fix changes how the game posts the "free mission done" event so that every rescue counts as its own entry (and corrects rescues made before the mod).

## Build

`build.sh` or `build.bat` (32-bit MinGW-w64).

## Licence

The code is MIT, see `LICENSE`. You must keep the copyright notice and credit the author, TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja).
