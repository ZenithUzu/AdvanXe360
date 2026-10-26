// AdvanXe360 entry point: sets up Direct3D and runs the menu UI. When the player
// picks a game, gba_main() (sdl/SDL.cpp) runs it until they quit.
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "Main.h"
#include "GeneralFunctions.h"
#include "Storage.h"
#include "XboxHost.h"
#include "XboxMusic.h"
#include "XboxCheevos.h"
#define XBOX_LOG_NO_REDIRECT
#include "XboxLog.h"

#include "../ui/UiApp.h"
#include "../ui/UiCanvas.h"

extern "C" LPDIRECT3D9 D3D;
extern "C" LPDIRECT3DDEVICE9 D3D_Device;

IDirect3DDevice9 *pDevice = 0;
D3DPRESENT_PARAMETERS d3dpp;

// Still referenced by parts of the core that were adapted for XContent saves.
GameStorage xStorage;

static HRESULT InitD3D(IDirect3DDevice9 **ppDevice, D3DPRESENT_PARAMETERS *pd3dPP)
{
  IDirect3D9 *pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  D3D = pD3D;

  ZeroMemory(pd3dPP, sizeof(D3DPRESENT_PARAMETERS));
  pd3dPP->BackBufferWidth = 1280;
  pd3dPP->BackBufferHeight = 720;
  pd3dPP->BackBufferFormat = D3DFMT_A8R8G8B8;
  pd3dPP->BackBufferCount = 1;
  pd3dPP->MultiSampleType = D3DMULTISAMPLE_NONE;
  pd3dPP->SwapEffect = D3DSWAPEFFECT_DISCARD;
  pd3dPP->PresentationInterval = D3DPRESENT_INTERVAL_ONE;

  return pD3D->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                            pd3dPP, ppDevice);
}

// Waits until no button is held, so a press doesn't leak into the next screen.
static void waitForRelease(UiCanvas &canvas, UiApp &ui)
{
  for (int i = 0; i < 120 && xboxReadPads(true) != 0; i++) {
    ui.draw(canvas);
    xboxUiPresent(canvas);
  }
}

VOID __cdecl main()
{
  MapInternalDrives();
  XSetFileCacheSize(0x10000);
  XMountUtilityDriveEx(XMOUNTUTILITYDRIVE_FORMAT0, 8192, 0);

  if (FAILED(InitD3D(&pDevice, &d3dpp))) {
    OutputDebugString("AdvanXe360: failed to create the Direct3D device\n");
    return;
  }
  D3D_Device = (LPDIRECT3DDEVICE9)pDevice;

  xboxCreateFolders();

  // Diagnostic log (core messages, load steps) next to default.xex.
  std::string logPath = uiJoinPath(xboxEmulatorDir(), "vba360.log");
  xboxLogOpen(logPath.c_str());

  UiApp ui(xboxHost(), g_cfg);
  g_cfg.load(ui.configPath());
  g_ui = &ui;
  cheevosStart();   // RetroAchievements: logs in with the saved token
  ui.loadLists();
  ui.openHome();

  static UiCanvas canvas;
  canvas.create(1280, 720);
  xboxSetMenuCanvas(&canvas);
  xboxUiRendererInit();
  waitForRelease(canvas, ui);

  for (;;) {
    xboxMusicUpdate(g_cfg.menuMusic, uiMusicVolumePercent[g_cfg.musicVolume], g_cfg.musicSong);
    ui.update(xboxReadPads(true));
    bool redraw = ui.needsRedraw();
    if (redraw)
      ui.draw(canvas);
    xboxUiPresent(canvas, redraw);

    std::string rom;
    if (ui.takeRequest(rom) != UI_REQ_LAUNCH)
      continue;

    // gba_main wants the folder (with its trailing '\') and the file name.
    std::string dir = uiParentDir(rom);
    std::string file = uiFileName(rom);
    static char dirBuf[MAX_PATH], fileBuf[MAX_PATH];
    strncpy(dirBuf, dir.c_str(), MAX_PATH - 1);
    strncpy(fileBuf, file.c_str(), MAX_PATH - 1);
    dirBuf[MAX_PATH - 1] = fileBuf[MAX_PATH - 1] = 0;

    xboxMusicStop();   // the menu music never plays over a game
    gba_main(dirBuf, fileBuf);

    ui.openHome(rom);
    if (xboxGameLoadFailed) {
      std::string msg = ui.tr("No se pudo abrir el juego", "The game could not be opened");
      std::string why = xboxLoadMessage;
      // Drop the folder part of any path in the message; keep it short.
      size_t slash = why.find_last_of("\\/");
      if (slash != std::string::npos)
        why = why.substr(0, why.find_last_of(' ', slash) + 1) + why.substr(slash + 1);
      if (why.size() > 70)
        why = why.substr(0, 67) + "...";
      if (!why.empty())
        msg += ": " + why;
      ui.toast(msg, 360);
    }
    waitForRelease(canvas, ui);
  }
}
