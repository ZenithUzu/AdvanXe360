# XeFork360

🌐 **Español** · [English](README.en.md)

[![Plataforma](https://img.shields.io/badge/plataforma-Xbox%20360-107C10)](#instalación)
[![Consolas](https://img.shields.io/badge/emula-GBA%20%C2%B7%20GB%20%C2%B7%20GBC-6F64F0)](#qué-emula-y-qué-no)
[![Licencia](https://img.shields.io/badge/licencia-GPL%20v2-blue)](#notas-y-créditos)
[![Descarga](https://img.shields.io/github/v/release/ZenithUzu/XeFork360?label=descargar)](https://github.com/ZenithUzu/XeFork360/releases/latest)

**XeFork360** (antes *VBA360 enhanced-ai*) es un emulador de **Game Boy Advance, Game Boy y Game Boy Color para Xbox 360** (homebrew: BadUpdate, RGH o JTAG). Basado en VisualBoyAdvance-M, con el núcleo **Gambatte** para Game Boy / Color, **marcos de Super Game Boy**, menú propio y opciones por juego.

Fork de [frankischilling/vba360-enhanced](https://github.com/frankischilling/vba360-enhanced) · **[⬇️ Descargar la última versión](https://github.com/ZenithUzu/XeFork360/releases/latest)**

| | |
|---|---|
| ![Castlevania: Aria of Sorrow](docs/screenshots/gba-castlevania.png) | ![Super Mario Bros. (Classic NES Series)](docs/screenshots/nes-smb-game.png) |
| ![Zelda: Link's Awakening DX](docs/screenshots/gbc-zelda-dx.png) | ![Pokémon Amarillo con marco de Super Game Boy](docs/screenshots/sgb-yellow-border.png) |

*Todas las capturas son de una Xbox 360 real (tomadas con Aurora). Estas cuatro muestran las cuatro cosas que corre: GBA, Classic NES Series, Game Boy Color y Super Game Boy.*

> 🤖 Los cambios de este fork se hicieron con ayuda de inteligencia artificial (Claude, de Anthropic). Lo mantengo por **pasatiempo**, sin fines de lucro y sin garantía. Eres libre de usarlo, copiarlo, modificarlo y compartirlo bajo la licencia GPL.

---

# Funciones

- **GBA** con el núcleo VBA-M, más rápido y compatible gracias a mejoras de VBA-Next. Ya arrancan los juegos de **Classic NES Series / Famicom Mini**.
- **Game Boy y Game Boy Color** con **Gambatte** (el núcleo de RetroArch), incluidos los juegos que son sólo de Color.
- **Super Game Boy:** el marco y los colores que cada juego traía.
- **Hacks:** parches **BPS / UPS / IPS** automáticos y guardado detectado leyendo la ROM (los hacks de Pokémon guardan bien).
- **Sensores de cartucho** con el stick derecho: sol (Boktai), inclinación (Yoshi, Koro Koro, Kirby Tilt 'n' Tumble), giroscopio y vibración (WarioWare: Twisted!, Drill Dozer).
- **Estados** en 9 ranuras con miniatura, guardar al salir y **continuar donde lo dejaste**.
- **Avance rápido** de 2x a 8x o máximo (mantener o alternar), **trucos** (GameShark, Action Replay, CodeBreaker, Game Genie) y **reloj** ajustable.
- **Audio:** volumen de 25 % a 200 % y retraso del sonido ajustable.
- **Partidas seguras:** copia automática del `.sav` al abrir cada juego, y opciones para borrar estados dañados o restablecer la configuración.
- **Imagen:** filtros (nítido, suave, Scale2x, LQ2x), tamaños (original, pantalla completa, entera y 16:9), efectos **CRT / LCD**, **corrección de color** de GBA, **mezcla de cuadros**, 41 paletas de Game Boy y fondo con tu propia imagen.
- **Opciones por juego:** imagen, botones, trucos, tipo de guardado y parche, guardados por nombre de archivo.
- **Menú propio** en español e inglés, con 7 temas (incluido **Frutiger Aero**), imagen de fondo, **burbujas y agua en movimiento**, portadas, búsqueda y favoritos.

## Game Boy Advance

| | |
|---|---|
| ![Castlevania](docs/screenshots/gba-castlevania.png) | ![Pokémon Unbound](docs/screenshots/gba-unbound-patch.png) |

Estas muestran juegos de GBA: *Castlevania: Aria of Sorrow* tal cual, y *Pokémon Unbound*, un hack que se abrió como **ROM limpia + parche `.bps`** con el mismo nombre (el parche se aplica solo, en memoria).

| | |
|---|---|
| ![Super Mario Bros. título](docs/screenshots/nes-smb-title.png) | ![Super Mario Bros. jugando](docs/screenshots/nes-smb-game.png) |

Estas muestran **Classic NES Series** (*Super Mario Bros.* para GBA): antes no arrancaba; ahora llega al título y se juega.

## Game Boy, Game Boy Color y Super Game Boy

| | |
|---|---|
| ![Zelda DX](docs/screenshots/gbc-zelda-dx.png) | ![Pokémon Cristal](docs/screenshots/gbc-crystal.png) |

Estas muestran el núcleo **Gambatte**: *Zelda: Link's Awakening DX* a color y *Pokémon Cristal*, un juego **sólo de Game Boy Color** que antes no corría.

| | |
|---|---|
| ![Pokémon Amarillo con marco](docs/screenshots/sgb-yellow-border.png) | ![Pokémon Amarillo con marco y fondo](docs/screenshots/sgb-yellow-background.png) |

Estas muestran el **marco de Super Game Boy** de *Pokémon Amarillo*: la primera con fondo negro y la segunda con una imagen de fondo propia a los lados.

## Sensores del cartucho

| | |
|---|---|
| ![Boktai: sensor solar](docs/screenshots/gba-boktai-sun.png) | ![Yoshi Topsy-Turvy](docs/screenshots/gba-yoshi-tilt.png) |
| ![Controles: sensores](docs/screenshots/controls-sensors.png) | ![Controles de Kirby Tilt 'n' Tumble](docs/screenshots/controls-kirby.png) |

Estas muestran los **sensores**: arriba, *Boktai* con el medidor de sol en pantalla (*Luz solar 0/10*, se sube y baja con el stick derecho) y *Yoshi Topsy-Turvy*, que se inclina con el stick. Abajo, la sección *Sensores de movimiento* de Controles y los controles propios de *Kirby Tilt 'n' Tumble* (GBC), que sólo muestran la inclinación porque ese cartucho no tiene sensor de sol.

## Imagen: efectos, colores y fondos

| | |
|---|---|
| ![Castlevania con efecto LCD](docs/screenshots/fx-castlevania-lcd.png) | ![Super Mario Bros. con efecto LCD](docs/screenshots/fx-smb-lcd.png) |
| ![Link's Awakening con fondo](docs/screenshots/gb-zelda-background.png) | ![Zelda DX título con fondo](docs/screenshots/gbc-zelda-dx-title.png) |

Estas muestran la **imagen**: arriba, el efecto **LCD** con la **corrección de color** de GBA (colores menos chillones, como la pantalla real); abajo, juegos de Game Boy en su tamaño original con una **imagen de fondo** propia a los lados.

| | |
|---|---|
| ![Sin efecto, paleta gris](docs/screenshots/gb-no-effect.png) | ![CRT, paleta verde](docs/screenshots/gb-crt-green.png) |
| ![LCD, paleta Pocket](docs/screenshots/gb-lcd-pocket.png) | ![LCD, paleta Light](docs/screenshots/gb-lcd-light.png) |

Estas muestran **efectos y paletas de Game Boy** en *Pokémon Rojo*: sin efecto con paleta gris, **CRT** con paleta verde y **LCD** con las paletas Pocket y Light.

## Menú y ajustes

| | |
|---|---|
| ![Video](docs/screenshots/menu-video.png) | ![Audio](docs/screenshots/menu-audio.png) |
| ![Emulación](docs/screenshots/menu-emulation.png) | ![Guardado](docs/screenshots/menu-saving.png) |
| ![Interfaz e idioma](docs/screenshots/menu-interface.png) | ![Reloj de los juegos](docs/screenshots/menu-clock.png) |

Estas muestran las pantallas de **Ajustes**: Video, Audio (volumen y retraso), Emulación, Guardado (con *Solución de problemas* para borrar estados dañados), Interfaz e idioma y el **reloj** personalizado para juegos como Pokémon Esmeralda.

| | |
|---|---|
| ![Opciones del juego](docs/screenshots/game-options-custom.png) | ![Opciones del juego: imagen personalizada](docs/screenshots/game-options-custom-2.png) |
| ![Trucos y códigos](docs/screenshots/game-cheats.png) | ![Controles de un juego](docs/screenshots/game-controls-gbc.png) |

Estas muestran las **opciones por juego** (botón **Y** sobre un juego): la imagen *Personalizada* (modo Super Game Boy, filtro, tamaño, efecto, mezcla de cuadros, FPS y salto de cuadros sólo para ese juego), los **trucos** y los **botones propios** de un juego de Game Boy Color.

| | |
|---|---|
| ![Panel lateral](docs/screenshots/picker-panel.png) | ![Lista emergente](docs/screenshots/picker-list.png) |
| ![Elegir imagen de fondo](docs/screenshots/picker-background.png) | ![Controles dentro del juego](docs/screenshots/ingame-controls-overlay.png) |

Estas muestran **cómo se cambia una opción**: con **panel lateral** o **lista emergente** (A abre los valores, B cancela), eligiendo una imagen de fondo, y el menú abierto **encima del juego** (clic del stick derecho).

| | |
|---|---|
| ![Tema Predeterminado](docs/screenshots/theme-default.png) | ![Tema Rojo](docs/screenshots/theme-red.png) |
| ![Tema Game Boy Advance](docs/screenshots/theme-gba.png) | ![Tema Frutiger Aero](docs/screenshots/theme-frutiger-aero.png) |
| ![Imagen de fondo del menú](docs/screenshots/menu-background-image.png) | ![Menú en inglés](docs/screenshots/controls-english.png) |

Estas muestran los **temas**: Predeterminado, Rojo, Game Boy Advance y **Frutiger Aero** (botones de cristal y burbujas); abajo, el menú con una **imagen de fondo** propia y el menú en **inglés**.

---

# Instalación

1. Descarga el `.zip` de la [última versión](https://github.com/ZenithUzu/XeFork360/releases/latest).
2. Copia la carpeta `XeFork360` a tu consola (por ejemplo `Hdd1:\Emulators\XeFork360\`) por FTP o USB.
3. Ábrela desde Aurora (`default.xex`). Las demás carpetas se crean solas.
4. Pon tus ROMs donde quieras (disco duro o USB) y búscalas desde el emulador. También abre `.zip` y `.7z`.

# Uso

**En la lista de juegos**

| Botón | Qué hace |
|---|---|
| A | Jugar / abrir carpeta |
| B | Subir de carpeta (o quitar la búsqueda) |
| X | Agregar o quitar de favoritos |
| Y | Opciones del juego |
| BACK | Buscar (la lista se filtra mientras escribes; START busca) |
| START | Cambiar el orden (A-Z, Z-A, recientes) |
| LT / RT | Saltar a la letra anterior / siguiente |
| LB / RB | Pestañas: Juegos · Recientes · Favoritos · Ajustes |

**Dentro del juego** (todo se puede cambiar en *Ajustes → Controles*)

| Botón | Qué hace |
|---|---|
| Cruceta o stick izquierdo | Cruceta |
| B / A | Botón A / B del GBA (como en la consola de Nintendo) |
| LB / RB | L / R (sólo GBA) |
| START / BACK | Start / Select |
| Y / X | Turbo A / Turbo B |
| RT | Avance rápido |
| Clic del stick derecho | Menú: estados, trucos, opciones, captura, reiniciar y salir |
| (sin asignar) | Guardado y carga rápida: asígnalos en *Controles* |
| Stick derecho | Sensores (sólo en los juegos que los tienen) |

**Opciones**

- **Ajustes** (cada opción tiene una línea de ayuda):

| Pantalla | Qué tiene |
|---|---|
| Video | Filtro, tamaño, efecto CRT / LCD, colores de Game Boy, Super Game Boy, fondo y su ajuste, corrección de color, mezcla de cuadros, FPS y salto de cuadros |
| Audio | Sonido, silenciar en avance rápido, volumen y retraso del sonido |
| Emulación | Velocidad y modo del avance rápido, BIOS original y formato de códigos de 16 dígitos |
| Guardado | Carpeta de partidas y de estados, guardar estado al salir, continuar donde lo dejaste, copias del `.sav` y borrar todos los estados |
| Controles | Todos los botones (también turbo, avance rápido, menú y guardado / carga rápida) y los sensores |
| Reloj de los juegos | Hora de la consola o fecha y hora personalizadas |
| Interfaz e idioma | Idioma, mostrar sólo juegos, cómo se cambia una opción, tema, imagen de fondo y restablecer todo |

- **Opciones del juego** (Y sobre un juego, o en el menú dentro del juego): tipo de guardado y reloj (GBA), parche, trucos, **imagen** (*Como en Ajustes* o *Personalizada*) y **botones** (*Como en Ajustes* o *Personalizados*).
- **Trucos:** se escriben con el teclado en pantalla o se importan de `.cht` / `.txt` (también de RetroArch). Cada juego guarda su lista, separada para GBA (`cheats\gba\`) y GB/GBC (`cheats\gb\`).

# Consejos y problemas comunes

## Hacks y parches

- **Hack ya parchado (`.gba`):** ábrelo como cualquier juego. Es lo que rinde mejor.
- **ROM limpia + parche:** ponlos juntos con el **mismo nombre** (`Pokemon Unbound.gba` + `Pokemon Unbound.bps`). El parche se aplica solo al abrirla (hasta 32 MB) y la ROM no se modifica. Con parches puede haber bajones de FPS; si pasa, usa la ROM ya parchada (en *Opciones del juego → Guardar ROM ya parchada*).

## Fondo alrededor del juego

Pon imágenes (PNG, JPG, BMP, TGA o DDS; ideal 1280×720) en `borders\` y elígelas en *Ajustes → Video → Fondo de la pantalla* (*Centrada*, *Rellenar* o *Estirar*). Hay un ejemplo en [`extras/borders/border.png`](extras/borders/border.png). El juego tapa este rectángulo de una imagen de 1280×720:

| Consola | Tamaño | Rectángulo del juego (x, y, ancho × alto) |
|---|---|---|
| GBA | Original | 100, 0, 1080 × 720 |
| GBA | Entera | 160, 40, 960 × 640 |
| GB | Original / Entera | 240, 0, 800 × 720 |
| GBA y GB | Pantalla completa | 80, 40, 1120 × 640 |
| GBA y GB | 16:9 | toda la pantalla |

## Menú: temas, fondo y portadas

- **Tema y fondo del menú:** las imágenes para el menú van en `backgrounds\`. En *Ajustes → Apariencia*: tema, imagen de fondo, **Visibilidad de la imagen** (cuánto se ve tu imagen), **Transparencia de los paneles** y **Tono de resaltado** (el color de la selección sobre cualquier tema).
- **Biblioteca** (la pantalla de inicio): primero eliges la consola, cada una con su cartucho (GBA, GBC, Game Boy/Pocket y los .zip/.7z aparte), separadas por la extensión del archivo. Dentro ves sólo los ROMs con su portada, sin carpetas: **Carrusel** (estilo *cover flow*: una portada de frente y las demás giradas a los lados, con reflejo y deslizamiento) o **Estante** (dos repisas); **START** cambia entre las dos.
- **Carpeta de juegos:** por defecto lee la carpeta **`Roms`** de cada unidad (por ejemplo `Hdd1:\Roms\` o `Usb0:\Roms\`) y sus subcarpetas. Para usar otra, en el explorador (BACK) pulsa **Y** sobre ella. Los archivos de BIOS no salen como juegos.
- **BACK** abre el menú de siempre (explorador de archivos, Recientes, Favoritos y Ajustes); **B** en la lista de unidades vuelve a la Biblioteca. En *Interfaz e idioma* → **Pantalla de inicio** puedes arrancar en el explorador en vez de la Biblioteca.
- **Efecto animado del fondo** (*Apariencia*): **Burbujas** que suben, **Agua** (tu imagen de fondo se ve a través de agua en movimiento, como en los menús de [snes360-enhanced](https://github.com/frankischilling/snes360-enhanced); se nota sobre una imagen), **Burbujas y agua**, o **Ninguno**. Sólo con el tema **Frutiger Aero**; también encima de tu propia imagen de fondo. Según lo que elijas aparecen **Cantidad de burbujas** y **Ondas del agua**.
- **Música del menú** (*Audio*): pon archivos **`.wav`** (PCM de 16 bits; Audacity los exporta así) en la carpeta `music\` junto a `default.xex`. Suenan sólo en el menú principal, **en orden** o **aleatoria**, con su propio volumen; nunca durante un juego.
- **Vista previa de la lista** (*Interfaz e idioma*): *Portada o última partida*, *Sólo portada* o *Sólo última partida* (la imagen del último momento que jugaste).
- **Portadas automáticas:** con la consola conectada a internet, en *Opciones del juego* (Y sobre un juego) → **Portada** → *Descargar portada* (ese juego) o *Descargar portadas de esta carpeta* (todos los que no tienen). Se bajan de [thumbnails.libretro.com](https://thumbnails.libretro.com) (las mismas de RetroArch) para GBA, GB y GBC. Se encuentran mejor si la ROM tiene su **nombre oficial** (No-Intro), por ejemplo `Pokemon - Emerald Version (USA, Europe).gba`; nombres cortos como `Pokemon Emerald` también suelen funcionar, pero hacks o nombres en español no. Si ese servidor dejara de funcionar, se puede poner otro en `vba360.ini` con `cover_server=`.
- **Portadas a mano:** pon `covers\<nombre de la ROM>.png` (o `.jpg`), por ejemplo `covers\Pokemon Esmeralda.png`. Unos 400×400 px bastan. Las descargas nunca reemplazan una portada que ya tenga el juego, salvo con *Descargar portada* de ese juego.

## Super Game Boy

En *Ajustes → Video → Super Game Boy (marcos)*: **Apagado**, **Juegos de Game Boy** o **También compatibles con Color** (Pokémon Amarillo, Oro/Plata, Zelda DX... se juegan con marco en vez de a color). También por juego: *Opciones del juego → Personalizada → Modo de juego*. Se aplica al abrir el juego; el marco lo manda el propio juego al arrancar.

## Si algo falla

- **Partida dañada:** al abrir un juego se guarda una copia del `.sav` en `saves\backups\` (las 3 últimas; `-1` es la más nueva). Cópiala encima del `.sav`.
- **Un juego se cuelga al cargar o al abrir:** *Ajustes → Guardado → Borrar todos los estados guardados* quita estados viejos o dañados.
- **Algo se ve o se comporta raro:** *Ajustes → Interfaz e idioma → Restablecer toda la configuración* vuelve todo a lo de fábrica (partidas, estados y trucos se quedan).
- **Para reportar un error:** manda `vba360.log` y `vba360-anterior.log` (junto a `default.xex`).
- **BIOS (opcional):** `bios\gba_bios.bin`, `bios\gb_bios.bin` o `bios\gbc_bios.bin`, y activa *Usar BIOS original* (Emulación).

Carpetas que crea junto a `default.xex`: `saves\`, `states\`, `cheats\`, `screenshots\`, `thumbs\`, `config\`, `borders\`, `backgrounds\`, `covers\`, `music\` y `bios\`. La configuración sigue en `vba360.ini` (no cambió de nombre para conservar tus ajustes).

# Qué emula y qué no

**Sí:**
- Game Boy Advance, incluidos **Classic NES Series / Famicom Mini**.
- Game Boy y Game Boy Color (también juegos sólo de Color), con reloj y vibración.
- Super Game Boy (marcos y colores de cada juego).
- Sensores: sol (Boktai), inclinación (Yoshi Topsy-Turvy, Koro Koro Puzzle, **Kirby Tilt 'n' Tumble**), giroscopio y vibración (WarioWare: Twisted!, Drill Dozer).

**No (todavía):**
- **Cable link** y **adaptador inalámbrico** (multijugador e intercambios).
- **e-Reader**, **Game Boy Camera**, **Game Boy Printer** e **infrarrojo** del Game Boy Color.
- El cartucho **TAMA5** de Bandai (Tamagotchi).
- Super Game Boy completo: falta el marco genérico, la animación y los sonidos de arranque, y el sonido extra de unos pocos juegos.

# Compilar

1. Visual Studio 2010 con el Xbox 360 SDK (probado con 21256.3).
2. Abre `vba360-enhanced/vbam.sln`, elige **Release | Xbox 360** y usa **Rebuild Solution** (compila también `libSDLx360`).
3. El resultado es `default.xex`.

Más detalles técnicos en el [README original](docs/README-original.md).

# Notas y créditos

- **XeFork360 (antes vba360-enhanced-ai):** [**ZenithUzu** (ZenithARC)](https://github.com/ZenithUzu): idea, pruebas en consola, compilación y capturas.
- **Programación con IA:** **Claude** ([Anthropic](https://www.anthropic.com)): cambios de código y documentación.
- **Fork vba360-enhanced:** [**frankischilling**](https://github.com/frankischilling), que arregló la compilación del port; base de este proyecto.
- **Port a Xbox 360:** los autores de **VBA360** y **libSDLx360**, con apoyo de **Team Avalaunch** y **Team XeLove**.
- **Emulador:** Forgotten (VisualBoyAdvance), el **equipo de VBA-M** y el equipo de **VBA-GX**.
- **Núcleo de Game Boy / Color:** [Gambatte](https://github.com/libretro/gambatte-libretro), de **Sindre Aamås** y el equipo de libretro (GPL v2). También sus paletas de Game Boy.
- **Super Game Boy y MBC7 (Kirby Tilt 'n' Tumble):** adaptados de [SameBoy](https://github.com/LIJI32/SameBoy), de **Lior Halphon** (MIT).
- **Mejoras del núcleo de GBA y arreglos para Classic NES Series:** [VBA-Next](https://github.com/libretro/vba-next) (libretro, **Squarepusher / twinaphex**, GPL v2).
- **Sensores de cartucho:** de la [VisualBoyAdvance-M actual](https://github.com/visualboyadvance-m/visualboyadvance-m) (GPL v2+).
- **Corrección de color de GBA:** shader *gba-color* de **Pokefan531**.
- **Efectos de pantalla:** *zfast_crt* de **Greg Hogan** (GPL v2+) y *lcd3x* de **Gigaherz** (dominio público), de [libretro/glsl-shaders](https://github.com/libretro/glsl-shaders).
- **Pruebas:** [gba-tests](https://github.com/jsmolka/gba-tests) de **jsmolka**, *cpu_instrs* de **Blargg** y [dmg-acid2 / cgb-acid2](https://github.com/mattcurrie/dmg-acid2) de **Matt Currie**.
- **Sonido de Game Boy y archivos comprimidos:** Gb_Apu, Blip_Buffer y File_Extractor de **Shay Green (blargg)**.
- **Íconos de los botones:** imágenes del control de Xbox 360 del paquete [Xbox 360 Controller Button Images](https://github.com/SimonDarksideJ/XNAGameStudio/wiki/Xbox-360-Controller-Button-Images) de **Microsoft** (XNA Game Studio), licencia Ms-PL ([texto](vba360-enhanced/src/ui/LICENSE-ButtonImages.txt)).
- **Portadas:** [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails) del equipo y la comunidad de **libretro**, servidas desde [thumbnails.libretro.com](https://thumbnails.libretro.com).
- **Efecto de agua:** idea de [snes360-enhanced](https://github.com/frankischilling/snes360-enhanced) de **frankischilling** (el agua que ondula el fondo), reescrita para este menú.
- **Cartuchos de la Biblioteca:** renders de **[ABeezy13](https://forums.launchbox-app.com/profile/77427-abeezy13/)** (foros de LaunchBox): Pokémon Esmeralda (GBA), cartucho amarillo con la etiqueta de Pokémon Amarillo (GBC) y Tetris (Game Boy).
- **Estilo de este README y de las notas de versión:** inspirado en [Pokemon-Terminal](https://github.com/LazoVelko/Pokemon-Terminal) y en las [releases de PokeRogue](https://github.com/pagefaultgames/pokerogue/releases).
- **Fuente:** [Inter](https://rsms.me/inter/), de **Rasmus Andersson** (SIL OFL). **Librerías:** zlib, libpng y SDL.

## Licencias

El emulador se distribuye bajo **GPL v2** ([LICENSE](LICENSE)). Tiene que ser la v2 porque el núcleo **Gambatte** y **VBA-Next** son "sólo GPL v2"; VisualBoyAdvance-M es "GPL v2 o posterior", así que también encaja. Las piezas con otra licencia son compatibles y conservan la suya:

| Pieza | Licencia |
|---|---|
| VisualBoyAdvance-M, VBA-Next, Gambatte, efectos *zfast_crt* | GPL v2 (o posterior en VBA-M y zfast_crt) |
| Super Game Boy y MBC7 (de SameBoy) | MIT |
| *lcd3x* | Dominio público |
| Fuente Inter | SIL Open Font License |
| zlib, libpng, SDL 1.2 | zlib, libpng, LGPL |
| Íconos de los botones (XNA) | Ms-PL, en su propio archivo (`UiGlyphs.cpp`) con su [texto de licencia](vba360-enhanced/src/ui/LICENSE-ButtonImages.txt) |
| Efecto de agua | Código propio; sólo la idea viene de snes360-enhanced (no se copió su código ni su textura) |
| Portadas | No vienen con el emulador: cada consola las descarga de libretro |
| Cartuchos de la Biblioteca (ABeezy13) | Sin licencia publicada: se usan con crédito; para un release público hay que pedirle permiso |

La lista completa de créditos del proyecto original está en el [README original](docs/README-original.md).
