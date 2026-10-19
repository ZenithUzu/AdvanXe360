# vba360-enhanced-ai

🌐 **English** · [Español](README.md)

[![Platform](https://img.shields.io/badge/platform-Xbox%20360-107C10)](#installation)
[![Consoles](https://img.shields.io/badge/emulates-GBA%20%C2%B7%20GB%20%C2%B7%20GBC-6F64F0)](#what-it-emulates-and-what-it-doesnt)
[![License](https://img.shields.io/badge/license-GPL%20v2-blue)](#notes--credits)
[![Download](https://img.shields.io/github/v/release/ZenithUzu/vba360-enhanced-ai?label=download)](https://github.com/ZenithUzu/vba360-enhanced-ai/releases/latest)

**Game Boy Advance, Game Boy and Game Boy Color emulator for the Xbox 360** (homebrew: BadUpdate, RGH or JTAG). Based on VisualBoyAdvance-M, with the **Gambatte** core for Game Boy / Color, **Super Game Boy borders**, its own menu and per-game options.

Fork of [frankischilling/vba360-enhanced](https://github.com/frankischilling/vba360-enhanced) · **[⬇️ Download the latest release](https://github.com/ZenithUzu/vba360-enhanced-ai/releases/latest)**

| | |
|---|---|
| ![Castlevania: Aria of Sorrow](docs/screenshots/gba-castlevania.png) | ![Super Mario Bros. (Classic NES Series)](docs/screenshots/nes-smb-game.png) |
| ![Zelda: Link's Awakening DX](docs/screenshots/gbc-zelda-dx.png) | ![Pokémon Yellow with its Super Game Boy border](docs/screenshots/sgb-yellow-border.png) |

*All screenshots come from a real Xbox 360 (taken with Aurora). These four show the four things it runs: GBA, Classic NES Series, Game Boy Color and Super Game Boy.*

> 🤖 The changes in this fork were made with the help of artificial intelligence (Claude, by Anthropic). I maintain it as a **hobby**, non-commercial and with no warranty. You are free to use, copy, modify and share it under the GPL license.

---

# Features

- **GBA** on the VBA-M core, faster and more compatible thanks to VBA-Next improvements. **Classic NES Series / Famicom Mini** games now start.
- **Game Boy and Game Boy Color** on **Gambatte** (RetroArch's core), including Color-only games.
- **Super Game Boy:** the border and colors each game came with.
- **Hacks:** automatic **BPS / UPS / IPS** patches and save type read from the ROM (Pokémon hacks save properly).
- **Cartridge sensors** on the right stick: sun (Boktai), tilt (Yoshi, Koro Koro, Kirby Tilt 'n' Tumble), gyroscope and rumble (WarioWare: Twisted!, Drill Dozer).
- **States** in 9 slots with thumbnails, save on exit and **continue where you left off**.
- **Fast forward** from 2x to 8x or max (hold or toggle), **cheats** (GameShark, Action Replay, CodeBreaker, Game Genie) and an adjustable **clock**.
- **Audio:** volume from 25% to 200% and adjustable sound latency.
- **Safe saves:** an automatic `.sav` backup every time a game is opened, plus options to delete broken states or reset all settings.
- **Picture:** filters (sharp, smooth, Scale2x, LQ2x), sizes (original, full screen, integer and 16:9), **CRT / LCD** effects, GBA **color correction**, **frame blending**, 41 Game Boy palettes and a background with your own image.
- **Per-game options:** picture, buttons, cheats, save type and patch, stored by file name.
- **Its own menu** in English and Spanish, with 7 themes (including **Frutiger Aero**), background image, **moving bubbles and water**, covers, search and favorites.

## Game Boy Advance

| | |
|---|---|
| ![Castlevania](docs/screenshots/gba-castlevania.png) | ![Pokémon Unbound](docs/screenshots/gba-unbound-patch.png) |

These show GBA games: *Castlevania: Aria of Sorrow* as is, and *Pokémon Unbound*, a hack opened as a **clean ROM + `.bps` patch** with the same name (the patch is applied on its own, in memory).

| | |
|---|---|
| ![Super Mario Bros. title](docs/screenshots/nes-smb-title.png) | ![Super Mario Bros. playing](docs/screenshots/nes-smb-game.png) |

These show **Classic NES Series** (*Super Mario Bros.* for GBA): it didn't start before; now it reaches the title screen and plays.

## Game Boy, Game Boy Color and Super Game Boy

| | |
|---|---|
| ![Zelda DX](docs/screenshots/gbc-zelda-dx.png) | ![Pokémon Crystal](docs/screenshots/gbc-crystal.png) |

These show the **Gambatte** core: *Zelda: Link's Awakening DX* in color and *Pokémon Crystal*, a **Game Boy Color-only** game that didn't run before.

| | |
|---|---|
| ![Pokémon Yellow with border](docs/screenshots/sgb-yellow-border.png) | ![Pokémon Yellow with border and background](docs/screenshots/sgb-yellow-background.png) |

These show *Pokémon Yellow*'s **Super Game Boy border**: the first one on a black background, the second one with your own background image on the sides.

## Cartridge sensors

| | |
|---|---|
| ![Boktai: solar sensor](docs/screenshots/gba-boktai-sun.png) | ![Yoshi Topsy-Turvy](docs/screenshots/gba-yoshi-tilt.png) |
| ![Controls: sensors](docs/screenshots/controls-sensors.png) | ![Kirby Tilt 'n' Tumble controls](docs/screenshots/controls-kirby.png) |

These show the **sensors**: on top, *Boktai* with the on-screen sun meter (*Luz solar 0/10*, raised and lowered with the right stick) and *Yoshi Topsy-Turvy*, tilted with the stick. Below, the *Motion sensors* section of Controls and *Kirby Tilt 'n' Tumble* (GBC)'s own controls, which only show tilt because that cart has no sun sensor.

## Picture: effects, colors and backgrounds

| | |
|---|---|
| ![Castlevania with LCD effect](docs/screenshots/fx-castlevania-lcd.png) | ![Super Mario Bros. with LCD effect](docs/screenshots/fx-smb-lcd.png) |
| ![Link's Awakening with background](docs/screenshots/gb-zelda-background.png) | ![Zelda DX title with background](docs/screenshots/gbc-zelda-dx-title.png) |

These show the **picture** options: on top, the **LCD** effect with GBA **color correction** (less garish colors, like the real screen); below, Game Boy games at their original size with your own **background image** on the sides.

| | |
|---|---|
| ![No effect, gray palette](docs/screenshots/gb-no-effect.png) | ![CRT, green palette](docs/screenshots/gb-crt-green.png) |
| ![LCD, Pocket palette](docs/screenshots/gb-lcd-pocket.png) | ![LCD, Light palette](docs/screenshots/gb-lcd-light.png) |

These show **Game Boy effects and palettes** in *Pokémon Red*: no effect with the gray palette, **CRT** with the green palette and **LCD** with the Pocket and Light palettes.

## Menu and settings

| | |
|---|---|
| ![Video](docs/screenshots/menu-video.png) | ![Audio](docs/screenshots/menu-audio.png) |
| ![Emulation](docs/screenshots/menu-emulation.png) | ![Saving](docs/screenshots/menu-saving.png) |
| ![Interface & language](docs/screenshots/menu-interface.png) | ![Game clock](docs/screenshots/menu-clock.png) |

These show the **Settings** screens (here in Spanish): Video, Audio (volume and latency), Emulation, Saving (with *Troubleshooting* to delete broken states), Interface & language and the custom **clock** for games such as Pokémon Emerald.

| | |
|---|---|
| ![Game options](docs/screenshots/game-options-custom.png) | ![Game options: custom picture](docs/screenshots/game-options-custom-2.png) |
| ![Cheats and codes](docs/screenshots/game-cheats.png) | ![A game's controls](docs/screenshots/game-controls-gbc.png) |

These show the **per-game options** (**Y** on a game): the *Custom* picture (Super Game Boy mode, filter, size, effect, frame blending, FPS and frame skip for that game only), **cheats** and a Game Boy Color game's **own buttons**.

| | |
|---|---|
| ![Side panel](docs/screenshots/picker-panel.png) | ![Pop-up list](docs/screenshots/picker-list.png) |
| ![Picking a background image](docs/screenshots/picker-background.png) | ![Controls over the game](docs/screenshots/ingame-controls-overlay.png) |

These show **how an option is changed**: with the **side panel** or the **pop-up list** (A opens the values, B cancels), picking a background image, and the menu opened **over the game** (right stick click).

| | |
|---|---|
| ![Default theme](docs/screenshots/theme-default.png) | ![Red theme](docs/screenshots/theme-red.png) |
| ![Game Boy Advance theme](docs/screenshots/theme-gba.png) | ![Frutiger Aero theme](docs/screenshots/theme-frutiger-aero.png) |
| ![Menu background image](docs/screenshots/menu-background-image.png) | ![Menu in English](docs/screenshots/controls-english.png) |

These show the **themes**: Default, Red, Game Boy Advance and **Frutiger Aero** (glass buttons and bubbles); below, the menu with your own **background image** and the menu in **English**.

---

# Installation

1. Download the `.zip` from the [latest release](https://github.com/ZenithUzu/vba360-enhanced-ai/releases/latest).
2. Copy the `VBA360` folder to your console (for example `Hdd1:\Emulators\VBA360\`) over FTP or USB.
3. Open it from Aurora (`default.xex`). The other folders are created on their own.
4. Put your ROMs anywhere (hard drive or USB) and browse to them from the emulator. `.zip` and `.7z` work too.

# Usage

**In the game list**

| Button | What it does |
|---|---|
| A | Play / open folder |
| B | Go up a folder (or clear the search) |
| X | Add to / remove from favorites |
| Y | Game options |
| BACK | Search (the list filters as you type; START searches) |
| START | Change the order (A-Z, Z-A, recent) |
| LT / RT | Jump to the previous / next letter |
| LB / RB | Tabs: Games · Recent · Favorites · Settings |

**In game** (everything can be changed in *Settings → Controls*)

| Button | What it does |
|---|---|
| D-pad or left stick | D-pad |
| B / A | GBA A / B button (Nintendo layout) |
| LB / RB | L / R (GBA only) |
| START / BACK | Start / Select |
| Y / X | Turbo A / Turbo B |
| RT | Fast forward |
| Right stick click | Menu: states, cheats, options, screenshot, restart and quit |
| (unassigned) | Quick save and quick load: assign them in *Controls* |
| Right stick | Sensors (only in the games that have them) |

**Options**

- **Settings** (every option has a help line):

| Screen | What's in it |
|---|---|
| Video | Filter, size, CRT / LCD effect, Game Boy colors, Super Game Boy, background and its fit, color correction, frame blending, FPS and frame skip |
| Audio | Sound, mute while fast-forwarding, volume and sound latency |
| Emulation | Fast-forward speed and mode, original BIOS and 16-digit code format |
| Saving | Save and state folders, save state on exit, continue where you left off, `.sav` backups and delete all states |
| Controls | Every button (turbo, fast forward, menu and quick save / load too) and the sensors |
| Game clock | Console time or a custom date and time |
| Interface & language | Language, show only games, how an option is changed, theme, background image and reset everything |

- **Game options** (Y on a game, or in the in-game menu): save type and clock (GBA), patch, cheats, **picture** (*As in Settings* or *Custom*) and **buttons** (*As in Settings* or *Custom*).
- **Cheats:** typed with the on-screen keyboard or imported from `.cht` / `.txt` (RetroArch's too). Each game keeps its own list, separate for GBA (`cheats\gba\`) and GB/GBC (`cheats\gb\`).

# Tips, tricks and common issues

## Hacks and patches

- **Already-patched hack (`.gba`):** open it like any game. It runs best.
- **Clean ROM + patch:** put them together with the **same name** (`Pokemon Unbound.gba` + `Pokemon Unbound.bps`). The patch is applied when you open it (up to 32 MB) and the ROM isn't changed. Patches can cause FPS drops; if so, use the patched ROM (*Game options → Save patched ROM* while playing).

## Background around the game

Put images (PNG, JPG, BMP, TGA or DDS; 1280×720 is best) in `borders\` and pick them in *Settings → Video → Screen background* (*Centered*, *Fill* or *Stretch*). There's a sample in [`extras/borders/border.png`](extras/borders/border.png). The game covers this rectangle of a 1280×720 image:

| Console | Size | Game rectangle (x, y, width × height) |
|---|---|---|
| GBA | Original | 100, 0, 1080 × 720 |
| GBA | Integer | 160, 40, 960 × 640 |
| GB | Original / Integer | 240, 0, 800 × 720 |
| GBA and GB | Full screen | 80, 40, 1120 × 640 |
| GBA and GB | 16:9 | the whole screen |

## Menu: themes, background and covers

- **Menu theme and background:** in *Settings → Interface & language*. Menu images go in `backgrounds\`.
- **Game list** (*Interface & language*): **Carousel** (the default) puts one big box up front, its neighbours beside it and the rest edge-on, showing the box spine; **Shelf** shows the boxes on two shelves; **Names** is the usual file list. Games without box art show as a plain box with their name.
- **Moving background effect** (*Interface & language*): rising **Bubbles**, **Water** (swaying light rays and waves along the bottom), **Bubbles and water**, or **None**. **Frutiger Aero** theme only; it also works over your own background image.
- **Game list preview** (*Interface & language*): *Box art or last played*, *Box art only* or *Last played only* (the picture of the last moment you played).
- **Automatic box art:** with the console online, go to *Game options* (Y on a game) → **Box art** → *Download box art* (that game) or *Download box art for this folder* (every game without one). They come from [thumbnails.libretro.com](https://thumbnails.libretro.com) (the same as RetroArch) for GBA, GB and GBC. They're found best when the ROM has its **official name** (No-Intro), e.g. `Pokemon - Emerald Version (USA, Europe).gba`; short names like `Pokemon Emerald` usually work too, but hacks or translated names don't. If that server ever stops working, another one can be set in `vba360.ini` with `cover_server=`.
- **Covers by hand:** put `covers\<ROM name>.png` (or `.jpg`), for example `covers\Pokemon Emerald.png`. About 400×400 px is enough. Downloads never replace a game's existing cover, except *Download box art* for that game.

## Super Game Boy

In *Settings → Video → Super Game Boy (borders)*: **Off**, **Game Boy games** or **Also Color-compatible ones** (Pokémon Yellow, Gold/Silver, Zelda DX... play with the border instead of in color). Also per game: *Game options → Custom → Game mode*. It applies when the game is opened; the game itself sends the border when it starts.

## If something goes wrong

- **Broken save:** when a game is opened, a copy of its `.sav` goes to `saves\backups\` (the last 3; `-1` is the newest). Copy it over the `.sav`.
- **A game hangs when loading or opening:** *Settings → Saving → Delete all save states* removes old or broken states.
- **Something looks or acts odd:** *Settings → Interface & language → Reset all settings* goes back to defaults (saves, states and cheats stay).
- **To report a bug:** send `vba360.log` and `vba360-anterior.log` (next to `default.xex`).
- **BIOS (optional):** `bios\gba_bios.bin`, `bios\gb_bios.bin` or `bios\gbc_bios.bin`, then turn on *Use original BIOS* (Emulation).

Folders created next to `default.xex`: `saves\`, `states\`, `cheats\`, `screenshots\`, `thumbs\`, `config\`, `borders\`, `backgrounds\`, `covers\` and `bios\`. Settings are in `vba360.ini`.

# What it emulates and what it doesn't

**Yes:**
- Game Boy Advance, including **Classic NES Series / Famicom Mini**.
- Game Boy and Game Boy Color (Color-only games too), with clock and rumble.
- Super Game Boy (each game's borders and colors).
- Sensors: sun (Boktai), tilt (Yoshi Topsy-Turvy, Koro Koro Puzzle, **Kirby Tilt 'n' Tumble**), gyroscope and rumble (WarioWare: Twisted!, Drill Dozer).

**Not (yet):**
- **Link cable** and **wireless adapter** (multiplayer and trading).
- **e-Reader**, **Game Boy Camera**, **Game Boy Printer** and the Game Boy Color's **infrared**.
- Bandai's **TAMA5** cartridge (Tamagotchi).
- Full Super Game Boy: the generic border, the boot animation and sounds, and the extra sound of a few games are missing.

# Building

1. Visual Studio 2010 with the Xbox 360 SDK (tested with 21256.3).
2. Open `vba360-enhanced/vbam.sln`, pick **Release | Xbox 360** and use **Rebuild Solution** (it builds `libSDLx360` too).
3. The result is `default.xex`.

More technical details in the [original README](docs/README-original.md).

# Notes & Credits

- **This fork (vba360-enhanced-ai):** [**ZenithUzu** (ZenithARC)](https://github.com/ZenithUzu): idea, console testing, builds and screenshots.
- **AI programming:** **Claude** ([Anthropic](https://www.anthropic.com)): code changes and documentation.
- **vba360-enhanced fork:** [**frankischilling**](https://github.com/frankischilling), who fixed the port's build; the base of this project.
- **Xbox 360 port:** the authors of **VBA360** and **libSDLx360**, with support from **Team Avalaunch** and **Team XeLove**.
- **Emulator:** Forgotten (VisualBoyAdvance), the **VBA-M team** and the **VBA-GX** team.
- **Game Boy / Color core:** [Gambatte](https://github.com/libretro/gambatte-libretro), by **Sindre Aamås** and the libretro team (GPL v2). Its Game Boy palettes too.
- **Super Game Boy and MBC7 (Kirby Tilt 'n' Tumble):** adapted from [SameBoy](https://github.com/LIJI32/SameBoy), by **Lior Halphon** (MIT).
- **GBA core improvements and Classic NES Series fixes:** [VBA-Next](https://github.com/libretro/vba-next) (libretro, **Squarepusher / twinaphex**, GPL v2).
- **Cartridge sensors:** from [current VisualBoyAdvance-M](https://github.com/visualboyadvance-m/visualboyadvance-m) (GPL v2+).
- **GBA color correction:** **Pokefan531**'s *gba-color* shader.
- **Screen effects:** *zfast_crt* by **Greg Hogan** (GPL v2+) and *lcd3x* by **Gigaherz** (public domain), from [libretro/glsl-shaders](https://github.com/libretro/glsl-shaders).
- **Testing:** [gba-tests](https://github.com/jsmolka/gba-tests) by **jsmolka**, **Blargg**'s *cpu_instrs* and [dmg-acid2 / cgb-acid2](https://github.com/mattcurrie/dmg-acid2) by **Matt Currie**.
- **Game Boy sound and compressed files:** Gb_Apu, Blip_Buffer and File_Extractor by **Shay Green (blargg)**.
- **Button icons:** Xbox 360 controller images from **Microsoft**'s [Xbox 360 Controller Button Images](https://github.com/SimonDarksideJ/XNAGameStudio/wiki/Xbox-360-Controller-Button-Images) pack (XNA Game Studio), Ms-PL license ([text](vba360-enhanced/src/ui/LICENSE-ButtonImages.txt)).
- **Typeface:** [Inter](https://rsms.me/inter/), by **Rasmus Andersson** (SIL OFL). **Libraries:** zlib, libpng and SDL.

Licensed under **GPL v2** (like VisualBoyAdvance-M). The original project's full credits are in the [original README](docs/README-original.md).
