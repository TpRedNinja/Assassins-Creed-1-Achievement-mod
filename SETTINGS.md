# AC1Ach settings guide

Everything you can change lives in `AC1Ach\AC1Ach.ini` (created on the first run) and the folders next to it. The `AC1Ach` folder sits next to `AC1Ach.asi` (normally `Scripts\AC1Ach`). Save the ini as UTF-8.

## What is read when

- `[Settings]` is read once when the game starts. Close and reopen the game after changing it.
- The `[00]` to `[44]` sections are read each time a popup shows. Changes there apply to the next popup, no restart needed.
- Pictures are also loaded each time a popup shows, so replacing a picture file applies to the next popup.

## Text (translations)

Each achievement has a section named by its slot number, `[00]` to `[44]`. `Title=` is the big line and `Description=` the small line of the popup. Change them to anything you like, in any language the game's font can draw. The default texts are the Xbox 360 ones, so the file is also the complete list of strings to translate. Delete a line to get its default back.

## Sounds

1. Put your `.wav` file in `AC1Ach\Sounds`.
2. For every achievement: set `Sound=yourfile.wav` under `[Settings]`.
3. For a single achievement: add `Sound=yourfile.wav` under that `[NN]` section. It wins over the default.
4. `Sound=none` makes that achievement (or all of them, under `[Settings]`) silent.

Write only the file name, not the path. The file must be a plain uncompressed (PCM) `.wav`; MP3 and other compressed formats will not play. If a file stays silent, re-save it as 16-bit PCM in an audio editor. `SoundDelayFrames=` (default 30) is how many frames after the picture appears the sound starts; raise it to start the sound later.

## Pictures

- `AC1Ach\Pictures\NN.png` (`00` to `44`, the slot number) is used by default. Replace a file with your own picture under the same name; png, jpg, bmp and tga work (not WebP).
- To use a differently named file for one achievement, add `Picture=name.png` under its `[NN]` section.
- Any size works, the picture is scaled to fit 256 pixels high and centred. A larger picture looks sharper than a 64x64 one that gets enlarged. Square pictures work best.

## Timing

`GapSeconds=` (default 9.5) is the minimum time between two popups, so a queue of achievements is shown one after another.

## Hotkeys

Set under `[Settings]` as F1 to F12, a single letter or digit, or `none`. They only work while the game window is in front.

| Setting | Default | What it does |
| --- | --- | --- |
| `ReplayNextKey` | F11 | Replay the next achievement the loaded save has earned |
| `ReplayNewKey` | F12 | Show every earned achievement that has not had a popup yet |
| `ResetHistoryKey` | off (F9 if enabled) | Written commented out (`; ResetHistoryKey=F9`) so it cannot be pressed by accident; remove the `; ` to enable. Delete the loaded save's `shown.txt`, so every earned achievement counts as not shown again |

## Files the mod writes

- `AC1Ach.log`: what the mod did; send it with a bug report.
- `Saves\<save name>\earned.txt`: the state of all 44 achievements on the loaded save ("earned", "earned before" = completed but no popup shown yet, "not earned").
- `Saves\<save name>\shown.txt`: the history of popups already shown on that save. Every save has its own folder, named like the game's save file (`<number>.sav`), so achievements can be earned again on each save. When a save is deleted, the mod removes its folder the next time it runs. F9 clears only the loaded save's history.

## When popups appear

Popups only show while you play as Altair, not in menus, loading screens, the pause menu or the modern day. An achievement earned elsewhere waits in the queue and shows when you are back in Altair gameplay. If another game popup (a flag, a Templar) is on screen, the achievement waits for it to finish.
