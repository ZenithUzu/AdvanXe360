// Presents the menu UI canvas (1280x720, 32-bit) on the Xbox 360 with a
// full-screen textured quad. The game itself is drawn by libSDLx360; after
// the menu closes XBOX_RestoreVideoState() puts its render state back.
#include <xtl.h>
#include <d3dx9.h>
#include <string.h>
#include <vector>

#include "XboxHost.h"
#include "../ui/UiCanvas.h"
#include "../ui/UiApp.h"
#include <map>

extern IDirect3DDevice9 *pDevice;

namespace {

struct UiVertex
{
  float x, y, z, w;
  float u, v;
};

// Two textures used in turn. The Xbox 360 doesn't wait for the GPU when the
// CPU locks a texture, so rewriting the one still being drawn showed torn,
// half-old letters; each one is reused only after its fence has passed.
IDirect3DTexture9 *g_texs[2] = { 0, 0 };
DWORD g_texFence[2] = { 0, 0 };
int g_texCur = 0;
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

// --- Cover flow boxes on the GPU ---------------------------------------------
// Each box is a quad with its picture; w = 1/height gives the perspective
// (the GPU divides by it, which also maps the picture without the bend two
// flat triangles would have). Each reflection is a second quad below it.

struct CardVertex
{
  float x, y, z, w;
  float u, v, fade, pad;   // fade: the quad's opacity at this corner
};

const char *kCardVS =
  "struct VS_IN  { float4 Pos : POSITION; float4 UV : TEXCOORD0; };\n"
  "struct VS_OUT { float4 Pos : POSITION; float4 UV : TEXCOORD0; };\n"
  "VS_OUT main(VS_IN In) { VS_OUT o; o.Pos = In.Pos; o.UV = In.UV; return o; }\n";
// c0.rgb: brightness. The picture's own alpha is ignored (boxes are solid).
const char *kCardPS =
  "sampler tex : register(s0);\n"
  "float4 c0 : register(c0);\n"
  "float4 main(float4 uv : TEXCOORD0) : COLOR {\n"
  "  float4 c = tex2D(tex, uv.xy); return float4(c.rgb * c0.rgb, uv.z); }\n";
IDirect3DVertexShader9 *g_cardVs = 0;
IDirect3DPixelShader9 *g_cardPs = 0;
IDirect3DVertexDeclaration9 *g_cardDecl = 0;

struct CardTex { IDirect3DTexture9 *tex; DWORD fence; unsigned used; };
std::map<unsigned, CardTex> g_cardTex;   // by UiCard::key
unsigned g_cardFrame = 0;
enum { MAX_CARD_TEXTURES = 40 };

bool cardInit()
{
  if (g_cardDecl)
    return g_cardVs && g_cardPs;
  ID3DXBuffer *code = 0;
  if (compile(kCardVS, "vs_2_0", &code)) {
    pDevice->CreateVertexShader((const DWORD *)code->GetBufferPointer(), &g_cardVs);
    code->Release();
  }
  if (compile(kCardPS, "ps_2_0", &code)) {
    pDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(), &g_cardPs);
    code->Release();
  }
  static const D3DVERTEXELEMENT9 decl[] = {
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
  };
  pDevice->CreateVertexDeclaration(decl, &g_cardDecl);
  return g_cardVs && g_cardPs && g_cardDecl;
}

// The GPU copy of a box picture, made the first time its key is seen.
IDirect3DTexture9 *cardTexture(const UiCard &c)
{
  std::map<unsigned, CardTex>::iterator it = g_cardTex.find(c.key);
  if (it != g_cardTex.end()) {
    it->second.used = g_cardFrame;
    return it->second.tex;
  }
  if (g_cardTex.size() >= MAX_CARD_TEXTURES) {   // drop the least recently drawn
    std::map<unsigned, CardTex>::iterator old = g_cardTex.begin();
    for (it = g_cardTex.begin(); it != g_cardTex.end(); ++it)
      if (it->second.used < old->second.used)
        old = it;
    if (old->second.fence)
      pDevice->BlockOnFence(old->second.fence);
    old->second.tex->Release();
    g_cardTex.erase(old);
  }
  IDirect3DTexture9 *tex = 0;
  if (c.w <= 0 || c.h <= 0 || !c.px ||
      FAILED(pDevice->CreateTexture(c.w, c.h, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL)) || !tex)
    return 0;
  D3DLOCKED_RECT lr;
  if (SUCCEEDED(tex->LockRect(0, &lr, NULL, 0))) {
    for (int y = 0; y < c.h; y++)
      memcpy((BYTE *)lr.pBits + y * lr.Pitch, c.px + y * c.pitch, c.w * 4);
    tex->UnlockRect(0);
  }
  CardTex ct = { tex, 0, g_cardFrame };
  g_cardTex[c.key] = ct;
  return tex;
}

void drawCards(const std::vector<UiCard> &cards)
{
  if (cards.empty() || !cardInit())
    return;
  g_cardFrame++;
  pDevice->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
  pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  pDevice->SetVertexDeclaration(g_cardDecl);
  pDevice->SetVertexShader(g_cardVs);
  pDevice->SetPixelShader(g_cardPs);
  pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  for (int i = (int)cards.size() - 1; i >= 0; i--) {   // farthest first
    const UiCard &c = cards[i];
    IDirect3DTexture9 *tex = cardTexture(c);
    if (!tex || c.hL <= 0 || c.hR <= 0)
      continue;
    float b = c.bright / 256.0f;
    float k[4] = { b, b, b, 1.0f };
    pDevice->SetPixelShaderConstantF(0, k, 1);
    pDevice->SetTexture(0, tex);
    // In vertical strips, each a plain quad (w = 1, as every other quad here):
    // close enough to the perspective, and no divide tricks with the
    // viewport transform off.
    enum { STRIPS = 8 };
    float a = 0.345f * b;
    for (int sIdx = 0; sIdx < STRIPS; sIdx++) {
      float f0 = (float)sIdx / STRIPS, f1 = (float)(sIdx + 1) / STRIPS;
      float x0 = c.dx + c.dw * f0 - 0.5f, x1 = c.dx + c.dw * f1 - 0.5f;
      float t0 = c.topL + (c.topR - c.topL) * f0 - 0.5f, t1 = c.topL + (c.topR - c.topL) * f1 - 0.5f;
      float h0 = c.hL + (c.hR - c.hL) * f0, h1 = c.hL + (c.hR - c.hL) * f1;
      CardVertex v[4] = {
        { x0, t0, 0, 1, f0, 0, 1, 0 },
        { x1, t1, 0, 1, f1, 0, 1, 0 },
        { x0, t0 + h0, 0, 1, f0, 1, 1, 0 },
        { x1, t1 + h1, 0, 1, f1, 1, 1, 0 }
      };
      pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(CardVertex));
      if (c.reflect > 0) {   // mirrored below, fading out (as UiCanvas::blitCard)
        float vl = 1.0f - (float)c.reflect / 100.0f;
        float r0 = h0 * c.reflect / 100.0f, r1 = h1 * c.reflect / 100.0f;
        CardVertex r[4] = {
          { x0, t0 + h0 + 1, 0, 1, f0, 1, a, 0 },
          { x1, t1 + h1 + 1, 0, 1, f1, 1, a, 0 },
          { x0, t0 + h0 + 1 + r0, 0, 1, f0, vl, 0, 0 },
          { x1, t1 + h1 + 1 + r1, 0, 1, f1, vl, 0, 0 }
        };
        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, r, sizeof(CardVertex));
      }
    }
  }
  DWORD fence = pDevice->InsertFence();
  for (std::map<unsigned, CardTex>::iterator it = g_cardTex.begin(); it != g_cardTex.end(); ++it)
    if (it->second.used == g_cardFrame)
      it->second.fence = fence;
  pDevice->SetTexture(0, NULL);
  pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
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
  if (g_vs)
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

void xboxUiPresent(const UiCanvas &canvas, bool changed, const std::vector<UiCard> *cards)
{
  if (!g_vs && !xboxUiRendererInit())
    return;
  int w = canvas.width(), h = canvas.height();
  if (!g_texs[0] || w != g_texW || h != g_texH) {
    pDevice->SetTexture(0, NULL);
    pDevice->BlockUntilIdle();
    for (int i = 0; i < 2; i++) {
      if (g_texs[i])
        g_texs[i]->Release();
      g_texs[i] = 0;
      g_texFence[i] = 0;
    }
    for (int i = 0; i < 2; i++)
      if (FAILED(pDevice->CreateTexture(w, h, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT,
                                        &g_texs[i], NULL)))
        return;
    g_texW = w;
    g_texH = h;
  }

  // Copy the canvas into the texture the GPU is done with (when the menu
  // didn't change, the last one is simply shown again).
  if (changed) {
    g_texCur ^= 1;
    IDirect3DTexture9 *up = g_texs[g_texCur];
    pDevice->SetTexture(0, NULL);
    if (g_texFence[g_texCur])
      pDevice->BlockOnFence(g_texFence[g_texCur]);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(up->LockRect(0, &lr, NULL, 0))) {
      const UiColor *src = canvas.pixels();
      for (int y = 0; y < h; y++)
        memcpy((BYTE *)lr.pBits + y * lr.Pitch, src + y * w, w * 4);
      up->UnlockRect(0);
    }
  }
  IDirect3DTexture9 *tex = g_texs[g_texCur];

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
  pDevice->SetTexture(0, tex);
  pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(UiVertex));
  g_texFence[g_texCur] = pDevice->InsertFence();
  if (cards)
    drawCards(*cards);
  pDevice->Present(NULL, NULL, NULL, NULL);
}

// Menu background: the image scaled to w x h, read back as 0xAARRGGBB pixels
// for the software menu canvas.
bool xboxLoadImagePixels(const char *path, int w, int h, std::vector<UiColor> &out, bool smooth)
{
  if (!pDevice || w <= 0 || h <= 0)
    return false;
  IDirect3DTexture9 *tex = 0;
  // Box filter for the pictures loaded once (backgrounds): shrinking a big
  // one with plain linear filtering skips pixels. Covers load while the list
  // is browsed, so they keep the faster linear one.
  if (FAILED(D3DXCreateTextureFromFileExA(pDevice, path, w, h, 1, 0, D3DFMT_LIN_A8R8G8B8,
                                          D3DPOOL_DEFAULT, smooth ? D3DX_FILTER_TRIANGLE : D3DX_FILTER_LINEAR, D3DX_DEFAULT,
                                          0, NULL, NULL, &tex)) || !tex)
    return false;
  bool ok = false;
  D3DLOCKED_RECT lr;
  if (SUCCEEDED(tex->LockRect(0, &lr, NULL, D3DLOCK_READONLY))) {
    out.resize((size_t)w * h);
    for (int y = 0; y < h; y++)
      memcpy(&out[(size_t)y * w], (const BYTE *)lr.pBits + y * lr.Pitch, w * 4);
    tex->UnlockRect(0);
    ok = true;
  }
  tex->Release();
  return ok;
}

bool xboxLoadImageFit(const char *path, int maxW, int maxH, std::vector<UiColor> &out, int &w, int &h)
{
  D3DXIMAGE_INFO info;
  if (!pDevice || FAILED(D3DXGetImageInfoFromFileA(path, &info)) || !info.Width || !info.Height)
    return false;
  w = (int)info.Width;
  h = (int)info.Height;
  if (w > maxW) { h = h * maxW / w; w = maxW; }
  if (h > maxH) { w = w * maxH / h; h = maxH; }
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  return xboxLoadImagePixels(path, w, h, out);
}
