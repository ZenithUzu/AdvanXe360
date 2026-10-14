// Presents the menu UI canvas (1280x720, 32-bit) on the Xbox 360 with a
// full-screen textured quad. The game itself is drawn by libSDLx360; after
// the menu closes XBOX_RestoreVideoState() puts its render state back.
#include <xtl.h>
#include <d3dx9.h>
#include <string.h>

#include "XboxHost.h"
#include "../ui/UiCanvas.h"

extern IDirect3DDevice9 *pDevice;

namespace {

struct UiVertex
{
  float x, y, z, w;
  float u, v;
};

IDirect3DTexture9 *g_tex = 0;
IDirect3DVertexShader9 *g_vs = 0;
IDirect3DPixelShader9 *g_ps = 0;
IDirect3DVertexDeclaration9 *g_decl = 0;
int g_texW = 0, g_texH = 0;

const char *kVertexShader =
  "struct VS_IN  { float4 Pos : POSITION; float2 UV : TEXCOORD0; };\n"
  "struct VS_OUT { float4 Pos : POSITION; float2 UV : TEXCOORD0; };\n"
  "VS_OUT main(VS_IN In) { VS_OUT o; o.Pos = In.Pos; o.UV = In.UV; return o; }\n";

// Solid black, for the area around the game picture.
const char *kBlackShader =
  "float4 main(float2 uv : TEXCOORD0) : COLOR { return float4(0.0, 0.0, 0.0, 1.0); }\n";
IDirect3DPixelShader9 *g_psBlack = 0;

// Render states every full-screen quad needs, whatever ran before it (the
// game's effect shader, the menu, D3DX).
void setQuadStates()
{
  pDevice->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);   // red, green, blue, alpha
  pDevice->SetVertexDeclaration(g_decl);
  pDevice->SetVertexShader(g_vs);
}

const char *kPixelShader =
  "sampler tex : register(s0);\n"
  "float4 main(float2 uv : TEXCOORD0) : COLOR { return tex2D(tex, uv); }\n";

bool compile(const char *src, const char *profile, ID3DXBuffer **code)
{
  ID3DXBuffer *errors = 0;
  HRESULT hr = D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL, "main", profile, 0,
                                 code, &errors, NULL);
  if (errors)
    errors->Release();
  return SUCCEEDED(hr);
}

IDirect3DTexture9 *g_border = 0;
int g_borderW = 0, g_borderH = 0;   // size of the image file
int g_borderFit = 0;                // UiBorderFit

// Fills the screen behind the game picture: black, then the background
// image (the game is drawn on top, so only the edges of the image show).
// Called by libSDLx360 right before it draws each game frame; always
// installed, so the area around the picture never shows leftovers.
void drawGameBackground()
{
  pDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0xFF000000, 1.0f, 0);
  // Paint the whole screen black with a quad as well: a clear alone has
  // let the previous screen (the menu, garbage) show around the picture.
  if (g_vs && g_psBlack) {
    UiVertex b[4] = {
      { -0.5f,    -0.5f,   0, 1, 0, 0 },
      { 1279.5f,  -0.5f,   0, 1, 1, 0 },
      { -0.5f,    719.5f,  0, 1, 0, 1 },
      { 1279.5f,  719.5f,  0, 1, 1, 1 }
    };
    setQuadStates();
    pDevice->SetPixelShader(g_psBlack);
    pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, b, sizeof(UiVertex));
  }
  if (g_border && g_vs && g_borderW > 0 && g_borderH > 0) {
    const float sw = 1280.0f, sh = 720.0f;
    float w = sw, h = sh;
    if (g_borderFit != 2) {
      // 0: whole image, centered (black bars if the shapes differ)
      // 1: cover the screen, centered (the edges of the image are cut)
      float sx = sw / g_borderW, sy = sh / g_borderH;
      float scale = g_borderFit == 1 ? (sx > sy ? sx : sy) : (sx < sy ? sx : sy);
      w = g_borderW * scale;
      h = g_borderH * scale;
    }
    float x0 = (sw - w) * 0.5f - 0.5f, y0 = (sh - h) * 0.5f - 0.5f;
    UiVertex v[4] = {
      { x0,     y0,     0, 1, 0, 0 },
      { x0 + w, y0,     0, 1, 1, 0 },
      { x0,     y0 + h, 0, 1, 0, 1 },
      { x0 + w, y0 + h, 0, 1, 1, 1 }
    };
    setQuadStates();
    pDevice->SetPixelShader(g_ps);
    pDevice->SetTexture(0, g_border);
    pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(UiVertex));
  }
  // Give the game picture its texture, vertex buffer and filter back.
  XBOX_RestoreVideoState();
  xboxApplyGameSampler();
}

} // namespace

extern "C" void (*XBOX_BackgroundHook)(void);

bool xboxSetBorderImage(const char *path, int fit)
{
  g_borderFit = fit;
  if (pDevice && (g_vs || xboxUiRendererInit()))
    XBOX_BackgroundHook = drawGameBackground;   // black at least
  if (g_border) {
    pDevice->SetTexture(0, NULL);
    pDevice->BlockUntilIdle();
    g_border->Release();
    g_border = 0;
  }
  g_borderW = g_borderH = 0;
  if (!path || !*path || !pDevice || !g_vs)
    return false;
  // D3DX reads PNG, JPG, BMP, TGA, DDS and DIB.
  D3DXIMAGE_INFO info;
  if (FAILED(D3DXGetImageInfoFromFileA(path, &info)) || !info.Width || !info.Height)
    return false;
  if (FAILED(D3DXCreateTextureFromFileA(pDevice, path, &g_border))) {
    g_border = 0;
    return false;
  }
  g_borderW = (int)info.Width;
  g_borderH = (int)info.Height;
  return true;
}

void xboxSetBorderFit(int fit)
{
  g_borderFit = fit;
}

bool xboxUiRendererInit()
{
  if (g_tex)
    return true;
  if (!pDevice)
    return false;

  ID3DXBuffer *code = 0;
  if (!compile(kVertexShader, "vs_2_0", &code))
    return false;
  pDevice->CreateVertexShader((const DWORD *)code->GetBufferPointer(), &g_vs);
  code->Release();
  if (!compile(kPixelShader, "ps_2_0", &code))
    return false;
  pDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(), &g_ps);
  code->Release();
  if (compile(kBlackShader, "ps_2_0", &code)) {
    pDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(), &g_psBlack);
    code->Release();
  }

  static const D3DVERTEXELEMENT9 decl[] = {
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
  };
  pDevice->CreateVertexDeclaration(decl, &g_decl);
  return g_vs && g_ps && g_decl;
}

void xboxUiPresent(const UiCanvas &canvas)
{
  if (!g_vs && !xboxUiRendererInit())
    return;
  int w = canvas.width(), h = canvas.height();
  if (!g_tex || w != g_texW || h != g_texH) {
    if (g_tex) {
      pDevice->SetTexture(0, NULL);
      pDevice->BlockUntilIdle();
      g_tex->Release();
      g_tex = 0;
    }
    if (FAILED(pDevice->CreateTexture(w, h, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT,
                                      &g_tex, NULL)))
      return;
    g_texW = w;
    g_texH = h;
  }

  // Copy the canvas into the texture.
  pDevice->SetTexture(0, NULL);
  D3DLOCKED_RECT lr;
  if (SUCCEEDED(g_tex->LockRect(0, &lr, NULL, 0))) {
    const UiColor *src = canvas.pixels();
    for (int y = 0; y < h; y++)
      memcpy((BYTE *)lr.pBits + y * lr.Pitch, src + y * w, w * 4);
    g_tex->UnlockRect(0);
  }

  // Full-screen quad in pixel coordinates (viewport transform disabled),
  // the same way libSDLx360 draws the game.
  D3DVIEWPORT9 vp;
  pDevice->GetViewport(&vp);
  float sw = vp.Width ? (float)vp.Width : 1280.0f;
  float sh = vp.Height ? (float)vp.Height : 720.0f;
  UiVertex v[4] = {
    { -0.5f,      -0.5f,      0, 1, 0, 0 },
    { sw - 0.5f,  -0.5f,      0, 1, 1, 0 },
    { -0.5f,      sh - 0.5f,  0, 1, 0, 1 },
    { sw - 0.5f,  sh - 0.5f,  0, 1, 1, 1 }
  };
  pDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0xFF000000, 1.0f, 0);
  setQuadStates();
  pDevice->SetPixelShader(g_ps);
  pDevice->SetTexture(0, g_tex);
  pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(UiVertex));
  pDevice->Present(NULL, NULL, NULL, NULL);
}
