# vba360-enhanced-ai

🌐 **English** · [Español](README.md)

**Game Boy Advance and Game Boy emulator for Xbox 360** (homebrew; works with BadUpdate / RGH / JTAG).
Fork of [frankischilling/vba360-enhanced](https://github.com/frankischilling/vba360-enhanced).

**[⬇️ Download the latest release](https://github.com/axelconde7w7-gif/vba360-enhanced-ai/releases/latest)**

> 🤖 **Notice:** the changes in this fork were made with the help of artificial intelligence (Claude, by Anthropic). I maintain it as a **hobby**, non-commercial and with no warranty of any kind.

You are **free to use it or not**, and to copy, modify and share it under the GPL terms of the original project. All credit for the emulator and the port goes to their original authors (see the [original README](README.md#original-readme-vba360-enhanced)).

## Install

1. Download the `.zip` from the [latest release](https://github.com/axelconde7w7-gif/vba360-enhanced-ai/releases/latest).
2. Copy the `VBA360` folder to your console (for example `Hdd1:\Emulators\VBA360\`) over FTP or USB.
3. Launch it from Aurora (`default.xex`). The other folders are created automatically.
4. Put your ROMs anywhere (hard drive or USB) and browse to them from the emulator.

## Features

**Interface**
- New interface, simple and fast, drawn by the emulator itself (the original XUI scenes are no longer used).
- **Games · Recent · Favorites · Settings** tabs (LB / RB).
- **Drive and folder** browser (hard drive, USB, memory units, emulator folder) with a preview of the last moment played and a marker when the ROM has a patch.
- **Spanish and English**, with a help line on every option.

**In-game menu** (right stick click; can be changed in Controls)
- Save and load **states in 9 slots**, each with a thumbnail.
- **Fast forward** at 2x, 3x, 4x, 6x, 8x or maximum, holding or toggling the button.
- Cheats, game options, settings, screenshot, restart and quit.

**ROM hack and ROM compatibility**
- **Automatic BPS / UPS / IPS patching** up to 32 MB: the clean ROM is never modified.
- **Battery saves** (`.sav`) for GBA and GB: what you save inside the game is written when you quit, when you open the menu and periodically.
- The **save type** (Flash 64/128 KB, SRAM, EEPROM) and the **RTC clock** are detected **by reading the ROM**, not by the cartridge ID. Pokémon hacks save correctly even when they keep the original ID.
- **Per-game options** (Y button on a game), stored by file name: save type, clock and patch.
- Compressed ROMs in `.zip` and `.7z`.

**Cheats**
- GameShark, Action Replay, CodeBreaker and VBA codes.
- Typed with an **on-screen keyboard** or **imported** from `.cht` / `.txt` files (RetroArch format too). Saved per game.

**Settings**
- **Video:** filter (sharp, smooth, Scale2x, LQ2x); original size (GBA 3:2, GB 10:9), full screen, **integer** (pixel perfect: GBA x4, GB x5) or **16:9** (fills the whole TV, stretched picture); **screen effect** (CRT-style scanlines or LCD-style grid); **Game Boy colors** (40 palettes: gray, classic green, original Game Boy, Pocket, Light, the **32 official Super Game Boy palettes** 1-A to 4-H, plus sepia, blue and red); **black or custom-image background** (centered, fill or stretch); FPS counter and frame skip.
- **Audio:** sound and mute during fast forward.
- **Emulation:** fast forward speed and mode, optional real BIOS and format of 16-digit codes.
- **Saving:** save and state folders (next to the ROM or in the emulator folder), save state on exit and resume where you left off.
- **Controls:** every button can be remapped (also turbo A/B, fast forward, menu, quick save and quick load). Pick from the list or just press the new button.

## Screenshots

**Pokémon SoulGold (hack) with a border image and FPS counter:**

| | |
|---|---|
| ![Gameplay 1](docs/screenshots/gameplay-soulgold-1.png) | ![Gameplay 2](docs/screenshots/gameplay-soulgold-2.png) |

**Game Boy with screen effects and palettes** (*Pokémon Red*, Spanish edition):

| | |
|---|---|
| ![No effect, gray palette](docs/screenshots/gb-no-effect.png) | ![CRT, green palette](docs/screenshots/gb-crt-green.png) |
| *No effect, Gray (original) palette* | *CRT, green palette* |
| ![LCD, Pocket palette](docs/screenshots/gb-lcd-pocket.png) | ![LCD, Light palette](docs/screenshots/gb-lcd-light.png) |
| *LCD, Game Boy Pocket palette* | *LCD, Game Boy Light palette* |

**Settings:**

| | |
|---|---|
| ![Video](docs/screenshots/settings-video-en.png) | ![Controls](docs/screenshots/controls.png) |
| ![Saving](docs/screenshots/settings-saving.png) | ![Emulation](docs/screenshots/settings-emulation.png) |
| ![Audio](docs/screenshots/settings-audio.png) | ![About](docs/screenshots/about.png) |
| ![Interface (Spanish)](docs/screenshots/settings-interface-es.png) | ![Interface (English)](docs/screenshots/settings-interface-en.png) |

## Playing a ROM hack

There are two ways:

**1. Pre-patched hack (`.gba`).** Open it like any other game. Hacks distributed as `.gba` files boot and save normally; for example, the **Pokémon SoulGold** in the screenshots is a GBA hack in this format. This is the recommended way, because it runs better.

**2. Clean ROM + patch.** Put the clean ROM and the patch in the same folder, with **the same name**:

```
Pokemon Unbound.gba
Pokemon Unbound.bps
```

When you open the ROM, the patch is applied automatically, in memory. Note: patched games may have performance drops (see [Known issues](#known-issues)); if that happens, use a pre-patched ROM.

## Screen background

By default the area around the game is **black**. To use an image, put it in the `borders\` folder next to `default.xex` (you can keep several, with any name) and pick it in **Settings → Video → Screen background**. Formats: **PNG, JPG, BMP, TGA and DDS** (not WebP). 1280×720 is ideal, but **Background fit** can keep it **Centered** (whole image in the middle), **Fill** the screen or **Stretch** it. The game is drawn on top, so only the sides show. There is an example in [`extras/borders/border.png`](extras/borders/border.png).

**Sizes for designing your own background** (a **1280×720** image, fit *Centered* or *Stretch*; for 1920×1080 multiply everything by 1.5). The game covers this rectangle:

| Console | Picture size | Game rectangle (x, y, width × height) | Free space at the sides |
|---|---|---|---|
| GBA | Original | 100, 0, 1080 × 720 | 100 px on each side |
| GBA | Integer | 160, 40, 960 × 640 | 160 px left/right, 40 px top/bottom |
| GB | Original / Integer | 240, 0, 800 × 720 | 240 px on each side |
| GBA and GB | Full screen | 80, 40, 1120 × 640 | 80 px left/right, 40 px top/bottom |
| GBA and GB | 16:9 | 0, 0, 1280 × 720 | none (the game covers the whole screen) |

If you use the same background for GBA and GB, keep the important artwork within the outer 100 px.

## Screen effects and colors

In **Settings → Video**:

- **Screen effect:**
  - **CRT (old TV):** scanlines that get thinner on bright colors plus a phosphor-grille mask. Adapted from the *zfast_crt* shader by Greg Hogan (GPL v2+), taken from [libretro/glsl-shaders → `crt/shaders/zfast_crt.glsl`](https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/zfast_crt.glsl).
  - **LCD (handheld screen):** a fine grid and red/green/blue sub-pixels, like a handheld's screen. Adapted from the *lcd3x* shader by Gigaherz (public domain), taken from [libretro/glsl-shaders → `handheld/shaders/lcd3x.glsl`](https://github.com/libretro/glsl-shaders/blob/master/handheld/shaders/lcd3x.glsl).
  - Both were translated from GLSL (RetroArch) to HLSL for the Xbox 360 and work for GBA and GB. They look best with **Picture size → Integer**.
- **Colors (Game Boy):** 40 palettes for Game Boy games. The original Game Boy (DMG), Pocket, Light and the **32 Super Game Boy** presets (1-A to 4-H) come from RetroArch's Gambatte core: [libretro/gambatte-libretro → `libgambatte/libretro/gbcpalettes.h`](https://github.com/libretro/gambatte-libretro/blob/master/libgambatte/libretro/gbcpalettes.h) (Sindre Aamås, GPL v2). Plus Gray (original), Classic green and the extras Sepia, Blue and Red.

## Games with sensors

Some GBA cartridges carried extra hardware. The emulator recognizes them by their game code and imitates it with the controller:

| Game | Cartridge hardware | How to use it on the Xbox |
|---|---|---|
| *Boktai 1 and 2*, *Shin Bokura no Taiyou* | Solar light sensor + clock | **Right stick up = more sun, down = less** (0 to 10, shown on screen). Level 10 fills every bar |
| *WarioWare: Twisted!* | Gyroscope + rumble | Rotate = **right stick** left or right; rumble goes to the controller |
| *Drill Dozer* | Rumble | Rumble goes to the controller |
| *Yoshi Topsy-Turvy*, *Koro Koro Puzzle* | Tilt sensor | Tilt = **right stick** |

In **Settings → Controls**, below the buttons, there is a **Sensors (right stick)** section:

- **Left / right:** tilt (Yoshi, Koro Koro) and rotate (WarioWare: Twisted!).
- **Up / down:** tilt forward or back.
- **Sunlight:** which way of the stick raises the sun in Boktai.
- To map one, select it, press **A** and push the right stick the way you want (the opposite way = inverted). You can also pick from the list with left/right, or leave it unassigned with **X**.
- **Sensitivity** (50% to 200%) and **dead zone**.

Sensors **only work in the games that use them**, so the right stick never interferes with anything else.

The solar sensor, gyroscope and rumble were adapted from current VisualBoyAdvance-M ([`src/core/gba/gbaRtc.cpp`](https://github.com/visualboyadvance-m/visualboyadvance-m/blob/master/src/core/gba/gbaRtc.cpp), GPL v2+). The sun scale was extended beyond VBA-M's (whose maximum only showed 6 of Boktai's 8 bars).

## Game clock

GBA games with a clock (Pokémon Ruby/Sapphire/Emerald, Boktai...) use the console time. **Settings → Game clock → Custom** lets you pick another year, month, day, hour and minute; from there the clock keeps running. *Back to the console time* undoes it.

## Folders the emulator creates

Next to `default.xex`: `saves\`, `states\`, `cheats\`, `screenshots\`, `thumbs\`, `config\`, `borders\` and `bios\` (put `gba_bios.bin` there if you want to use the real BIOS). Settings are stored in `vba360.ini`.

## Limitations

- **Game Boy Advance and Game Boy; not Game Boy Color.** Game Boy (`.gb`) games work. Cartridges made for both consoles ("black" `.gbc` carts) start in Game Boy mode with the chosen palette. Games made **only** for the Game Boy Color (such as Pokémon Crystal or Super Mario Bros. Deluxe) are not supported.
- **Some GBA games that need add-ons don't work:**
  - **Classic NES Series / Famicom Mini** (the NES games for GBA, such as *Super Mario Bros.*): they use special protections and don't start.
  - Accessories such as the **e-Reader** or the link cable are not emulated either.

## Known issues

- ~~**Colored lines / the menu around the game**~~ **(fixed):** with the black background or a screen effect on, colored lines or the menu could sometimes show at the sides of the game. The emulator now paints the whole background black every frame and clears the game picture when it starts. If it ever happens again, a **background image** avoids it (Settings → Video → Screen background).

- **Performance with patches:** games loaded with a **UPS / IPS / BPS** patch applied on the fly can have FPS drops. Pre-patched `.gba` hacks don't have this problem.

## Status

Tested on console (BadUpdate + Aurora). Work in progress: if you find bugs, please open an *issue*.

## Credits

- **Original emulator authors:** Forgotten (VisualBoyAdvance) and the **VBA-M team**, plus the **VBA-GX** team, whose code the port uses.
- **Original Xbox 360 port:** the authors of **VBA360** and the **libSDLx360** library, with support from **Team Avalaunch** and **Team XeLove**.
- **vba360-enhanced fork:** [**frankischilling**](https://github.com/frankischilling), who fixed the port's build and whose work this project is based on.
- **This fork (vba360-enhanced-ai):** [**Axel** (axelconde7w7-gif)](https://github.com/axelconde7w7-gif). Idea, console testing, builds and screenshots.
- **AI programming:** **Claude** (by [Anthropic](https://www.anthropic.com)). Wrote the code changes and documentation of this fork.
- **Interface font:** [Inter](https://rsms.me/inter/) by Rasmus Andersson (SIL OFL license).
- **Screen effects:** adapted from [libretro/glsl-shaders](https://github.com/libretro/glsl-shaders): *zfast_crt* by Greg Hogan (GPL v2+) for CRT and *lcd3x* by Gigaherz (public domain) for LCD.
- **Game Boy palettes:** taken from the [Gambatte libretro core](https://github.com/libretro/gambatte-libretro) (Sindre Aamås, GPL v2): original Game Boy, Pocket, Light and the 32 Super Game Boy presets.
- **Cartridge sensors** (sun, gyroscope, rumble): adapted from [current VisualBoyAdvance-M](https://github.com/visualboyadvance-m/visualboyadvance-m) (GPL v2+).
- **Game Boy sound and archive reading:** Gb_Apu, Blip_Buffer and File_Extractor by Shay Green (blargg).

The full list of credits and libraries is in the [original README](README.md#original-readme-vba360-enhanced).

## License

This program is free software under the **GNU General Public License v2 or later**, distributed WITHOUT ANY WARRANTY. Users are responsible for having the legal right to use any ROM with this emulator. Not affiliated with Nintendo or Microsoft.
