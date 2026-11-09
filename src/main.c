/*
 * Keypad Timer (キーパッドタイマー) - a tiny always-on-top kitchen-style countdown timer.
 * Win32 + GDI+ (flat API), no runtime needed.  MIT License, Copyright (c) 2026 zmlworks
 *
 * Keyboard input is intentionally ignored (mis-operation guard); everything is mouse-only.
 */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shlobj.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <math.h>
#include <stdio.h>
#include <wchar.h>
#include "tlogic.h"
#include "resource.h"

#define APPNAME   L"キーパッドタイマー"
#define CLASSNAME L"zmlworks.KeypadTimer"

/* ---------- layout (logical px @96dpi) ---------- */
#define W_LOG 240
#define H_LOG 420

enum { B_D0 = 0, /* 0..9 digits */ B_ADD5 = 10, B_ADD1, B_ADD30, B_RESET, B_START, B_SET, B_MIN, B_CLOSE, B_COUNT };
typedef struct { float x, y, w, h; } RF;
static RF gBtn[B_COUNT];

static void Layout(void)
{
    const float m = 12, gap = 8, cw = (W_LOG - 2 * m - 2 * gap) / 3.0f, rh = 38;
    float y;
    int i;
    for (i = 0; i < 3; i++) gBtn[B_ADD5 + i] = (RF){ m + i * (cw + gap), 136, cw, 32 };
    y = 180;
    for (i = 0; i < 9; i++) {           /* 7 8 9 / 4 5 6 / 1 2 3 */
        int row = 2 - i / 3, col = i % 3;
        gBtn[B_D0 + 1 + i] = (RF){ m + col * (cw + gap), y + row * (rh + gap), cw, rh };
    }
    gBtn[B_D0]    = (RF){ m, y + 3 * (rh + gap), 2 * cw + gap, rh };
    gBtn[B_RESET] = (RF){ m + 2 * (cw + gap), y + 3 * (rh + gap), cw, rh };
    gBtn[B_START] = (RF){ m, y + 4 * (rh + gap), W_LOG - 2 * m, 44 };
    gBtn[B_CLOSE] = (RF){ W_LOG - 32, 6, 20, 20 };
    gBtn[B_MIN]   = (RF){ W_LOG - 56, 6, 20, 20 };
    gBtn[B_SET]   = (RF){ W_LOG - 80, 6, 20, 20 };
}

/* ---------- state ---------- */
static HWND gWnd;
static Timer gT;
static float gScale = 1.0f;
static int gHover = -1, gPress = -1;
static ULONG_PTR gGdipToken;
static wchar_t gIni[MAX_PATH];
static struct { int vol, fade, hold, top, x, y; } gCfg = { 60, 30, 3, 1, INT_MIN, INT_MIN };
static int gBlinkOn = 1;
static int gLastTitle = -2;

#define TM_TICK   1
#define TM_ALARM  2
#define TM_CLICKCLOSE 3

static long long Now(void) { return (long long)GetTickCount64(); }

/* ---------- settings ---------- */
static void IniPath(void)
{
    wchar_t dir[MAX_PATH];
    wchar_t *p;
    GetModuleFileNameW(NULL, gIni, MAX_PATH);
    p = wcsrchr(gIni, L'\\');
    if (p) wcscpy(p + 1, L"keypad-timer.ini");
    if (GetFileAttributesW(gIni) != INVALID_FILE_ATTRIBUTES) return; /* portable mode */
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, dir))) {
        swprintf(gIni, MAX_PATH, L"%ls\\KeypadTimer", dir);
        CreateDirectoryW(gIni, NULL);
        wcscat(gIni, L"\\keypad-timer.ini");
    }
}
static int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void LoadCfg(int *last)
{
    gCfg.vol  = Clamp(GetPrivateProfileIntW(L"timer", L"volume", 60, gIni), 0, 100);
    gCfg.fade = Clamp(GetPrivateProfileIntW(L"timer", L"fade_seconds", 30, gIni), 0, 600);
    gCfg.hold = Clamp(GetPrivateProfileIntW(L"timer", L"max_minutes", 3, gIni), 1, 60);
    gCfg.top  = GetPrivateProfileIntW(L"timer", L"topmost", 1, gIni) != 0;
    gCfg.x    = (int)GetPrivateProfileIntW(L"window", L"x", INT_MIN, gIni);
    gCfg.y    = (int)GetPrivateProfileIntW(L"window", L"y", INT_MIN, gIni);
    *last     = Clamp(GetPrivateProfileIntW(L"timer", L"last_seconds", 0, gIni), 0, TL_MAX_SEC);
}
static void PutInt(const wchar_t *sec, const wchar_t *key, int v)
{
    wchar_t b[24];
    swprintf(b, 24, L"%d", v);
    WritePrivateProfileStringW(sec, key, b, gIni);
}
static void SaveCfg(void)
{
    RECT r;
    PutInt(L"timer", L"volume", gCfg.vol);
    PutInt(L"timer", L"fade_seconds", gCfg.fade);
    PutInt(L"timer", L"max_minutes", gCfg.hold);
    PutInt(L"timer", L"topmost", gCfg.top);
    PutInt(L"timer", L"last_seconds", gT.remembered);
    if (gWnd && !IsIconic(gWnd) && GetWindowRect(gWnd, &r)) {
        PutInt(L"window", L"x", r.left);
        PutInt(L"window", L"y", r.top);
    }
}

/* ---------- audio (generated in-app, waveOut) ---------- */
#define SR TL_SR
#define ABUF (SR / 10)
static HWAVEOUT gAWo, gCWo;
static WAVEHDR gAH[4], gCH;
static short gABuf[4][ABUF];
static short *gCBuf; static int gCLen, gCCap;
static long long gASamp;
static int gAlarmOn;

static WAVEFORMATEX Fmt(void)
{
    WAVEFORMATEX wf = { WAVE_FORMAT_PCM, 1, SR, SR * 2, 2, 16, 0 };
    return wf;
}
static void AlarmFill(int i)
{
    int n;
    for (n = 0; n < ABUF; n++) gABuf[i][n] = tl_alarm_sample(gASamp + n, gCfg.vol, gCfg.fade, gCfg.hold);
    gASamp += ABUF;
}
static void AlarmStart(void)
{
    WAVEFORMATEX wf = Fmt();
    int i;
    if (gAlarmOn) return;
    if (waveOutOpen(&gAWo, WAVE_MAPPER, &wf, (DWORD_PTR)gWnd, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR) { gAWo = NULL; return; }
    gAlarmOn = 1;
    gASamp = 0;
    for (i = 0; i < 4; i++) {
        ZeroMemory(&gAH[i], sizeof gAH[i]);
        gAH[i].lpData = (LPSTR)gABuf[i];
        gAH[i].dwBufferLength = sizeof gABuf[i];
        AlarmFill(i);
        waveOutPrepareHeader(gAWo, &gAH[i], sizeof gAH[i]);
        waveOutWrite(gAWo, &gAH[i], sizeof gAH[i]);
    }
}
static void AlarmStop(void)
{
    int i;
    if (!gAlarmOn) return;
    gAlarmOn = 0;
    waveOutReset(gAWo);
    for (i = 0; i < 4; i++) waveOutUnprepareHeader(gAWo, &gAH[i], sizeof gAH[i]);
    waveOutClose(gAWo);
    gAWo = NULL;
}
static void AlarmDone(HWAVEOUT wo, WAVEHDR *h)
{
    if (!gAlarmOn || wo != gAWo) return;
    AlarmFill((int)(h - gAH));
    waveOutWrite(gAWo, h, sizeof *h);
}
/* one-shot sounds (button click / preview) at the alarm's max volume */
static void PlayShot(double seconds, int pattern)
{
    WAVEFORMATEX wf = Fmt();
    int n, len = (int)(seconds * SR);
    if (gCfg.vol <= 0) return;
    if (len > gCCap) {
        short *nb = (short *)realloc(gCBuf, len * sizeof(short));
        if (!nb) return;
        gCBuf = nb; gCCap = len;
    }
    for (n = 0; n < len; n++) gCBuf[n] = tl_shot_sample(n, len, gCfg.vol, pattern);
    gCLen = len;
    if (!gCWo && waveOutOpen(&gCWo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { gCWo = NULL; return; }
    waveOutReset(gCWo);
    if (gCH.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(gCWo, &gCH, sizeof gCH);
    ZeroMemory(&gCH, sizeof gCH);
    gCH.lpData = (LPSTR)gCBuf;
    gCH.dwBufferLength = gCLen * sizeof(short);
    waveOutPrepareHeader(gCWo, &gCH, sizeof gCH);
    waveOutWrite(gCWo, &gCH, sizeof gCH);
    if (gWnd) SetTimer(gWnd, TM_CLICKCLOSE, (UINT)(seconds * 1000) + 3000, NULL);
}
static void ClickClose(void)
{
    if (!gCWo) return;
    waveOutReset(gCWo);
    if (gCH.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(gCWo, &gCH, sizeof gCH);
    waveOutClose(gCWo);
    gCWo = NULL;
}

/* ---------- drawing helpers (GDI+ flat API) ---------- */
#define ARGB(a, r, g, b) ((ARGB)(((DWORD)(a) << 24) | ((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b)))

static GpPath *RRect(float x, float y, float w, float h, float r)
{
    GpPath *p;
    float d = r * 2;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}
static void FillSolid(GpGraphics *g, GpPath *p, ARGB c)
{
    GpSolidFill *b;
    GdipCreateSolidFill(c, &b);
    GdipFillPath(g, (GpBrush *)b, p);
    GdipDeleteBrush((GpBrush *)b);
}
static void FillGrad(GpGraphics *g, GpPath *p, float x, float y, float w, float h, ARGB c1, ARGB c2)
{
    GpLineGradient *b;
    GpRectF r = { x, y - 0.5f, w, h + 1 };
    GdipCreateLineBrushFromRect(&r, c1, c2, LinearGradientModeVertical, WrapModeTileFlipXY, &b);
    GdipFillPath(g, (GpBrush *)b, p);
    GdipDeleteBrush((GpBrush *)b);
}
static void Stroke(GpGraphics *g, GpPath *p, ARGB c, float w)
{
    GpPen *pen;
    GdipCreatePen1(c, w, UnitWorld, &pen);
    GdipDrawPath(g, pen, p);
    GdipDeletePen(pen);
}
static GpFontFamily *gFam;
static void InitFont(void)
{
    static const wchar_t *names[] = { L"Meiryo UI", L"Yu Gothic UI", L"Meiryo", L"MS UI Gothic", L"Noto Sans CJK JP", L"Segoe UI", L"Tahoma" };
    int i;
    for (i = 0; i < (int)(sizeof names / sizeof *names); i++)
        if (GdipCreateFontFamilyFromName(names[i], NULL, &gFam) == Ok) return;
    GdipGetGenericFontFamilySansSerif(&gFam);
}
static void Text(GpGraphics *g, const wchar_t *s, float x, float y, float w, float h, float size, int bold, ARGB c, int align)
{
    GpFont *f;
    GpStringFormat *sf;
    GpSolidFill *b;
    GpRectF r = { x, y, w, h };
    if (!gFam || GdipCreateFont(gFam, size, bold ? FontStyleBold : FontStyleRegular, UnitWorld, &f) != Ok) return;
    GdipCreateStringFormat(StringFormatFlagsNoWrap, LANG_NEUTRAL, &sf);
    GdipSetStringFormatAlign(sf, align == 0 ? StringAlignmentNear : StringAlignmentCenter);
    GdipSetStringFormatLineAlign(sf, StringAlignmentCenter);
    GdipCreateSolidFill(c, &b);
    GdipDrawString(g, s, -1, f, &r, sf, (GpBrush *)b);
    GdipDeleteBrush((GpBrush *)b);
    GdipDeleteStringFormat(sf);
    GdipDeleteFont(f);
}

/* checkerboard texture: clear red with a faint pale-yellow cell */
static GpTexture *gChk[3];
static ARGB Mix(ARGB a, ARGB b, float t)
{
    int i;
    ARGB r = 0;
    for (i = 0; i < 32; i += 8) {
        int x = (a >> i) & 255, y = (b >> i) & 255;
        r |= (ARGB)(int)lrintf(x + (y - x) * t) << i;
    }
    return r;
}
static GpTexture *MakeChecker(ARGB red, float yellow)
{
    GpBitmap *bm;
    GpTexture *t;
    const int c = 6;
    int x, y;
    ARGB pale = Mix(red, ARGB(red >> 24, 255, 238, 160), yellow);
    GdipCreateBitmapFromScan0(c * 2, c * 2, 0, PixelFormat32bppARGB, NULL, &bm);
    for (y = 0; y < c * 2; y++)
        for (x = 0; x < c * 2; x++)
            GdipBitmapSetPixel(bm, x, y, ((x / c) ^ (y / c)) ? pale : red);
    GdipCreateTexture((GpImage *)bm, WrapModeTile, &t);
    GdipSetTextureWrapMode(t, WrapModeTile);
    GdipDisposeImage((GpImage *)bm);
    return t;
}
static void InitTextures(void)
{
    gChk[0] = MakeChecker(ARGB(232, 206, 26, 44), 0.15f);  /* keys */
    gChk[1] = MakeChecker(ARGB(238, 168, 10, 30), 0.13f);  /* reset / add (deeper red) */
    gChk[2] = MakeChecker(ARGB(242, 222, 34, 48), 0.17f);  /* start */
}

/* ---------- 7-segment LCD ---------- */
static const unsigned char SEG[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
#define SLANT 0.10f

static void Poly(GpGraphics *g, GpBrush *b, const float *uv, int n, float x, float y, float h)
{
    GpPointF p[6];
    int i;
    for (i = 0; i < n; i++) { p[i].X = x + uv[i * 2] + (h - uv[i * 2 + 1]) * SLANT; p[i].Y = y + uv[i * 2 + 1]; }
    GdipFillPolygon(g, b, p, n, FillModeAlternate);
}
static void HSeg(GpGraphics *g, GpBrush *b, float u0, float u1, float vc, float t, float x, float y, float h)
{
    float q[12] = { u0, vc, u0 + t / 2, vc - t / 2, u1 - t / 2, vc - t / 2, u1, vc, u1 - t / 2, vc + t / 2, u0 + t / 2, vc + t / 2 };
    Poly(g, b, q, 6, x, y, h);
}
static void VSeg(GpGraphics *g, GpBrush *b, float uc, float v0, float v1, float t, float x, float y, float h)
{
    float q[12] = { uc, v0, uc + t / 2, v0 + t / 2, uc + t / 2, v1 - t / 2, uc, v1, uc - t / 2, v1 - t / 2, uc - t / 2, v0 + t / 2 };
    Poly(g, b, q, 6, x, y, h);
}
static void Digit(GpGraphics *g, GpBrush *on, GpBrush *off, int d, float x, float y, float w, float h, float t)
{
    const float k = 0.9f, hm = h / 2;
    unsigned m = d < 0 ? 0 : SEG[d];
    GpBrush *b;
    b = (m & 0x01) ? on : off; HSeg(g, b, t / 2 + k, w - t / 2 - k, t / 2, t, x, y, h);
    b = (m & 0x02) ? on : off; VSeg(g, b, w - t / 2, t / 2 + k, hm - k, t, x, y, h);
    b = (m & 0x04) ? on : off; VSeg(g, b, w - t / 2, hm + k, h - t / 2 - k, t, x, y, h);
    b = (m & 0x08) ? on : off; HSeg(g, b, t / 2 + k, w - t / 2 - k, h - t / 2, t, x, y, h);
    b = (m & 0x10) ? on : off; VSeg(g, b, t / 2, hm + k, h - t / 2 - k, t, x, y, h);
    b = (m & 0x20) ? on : off; VSeg(g, b, t / 2, t / 2 + k, hm - k, t, x, y, h);
    b = (m & 0x40) ? on : off; HSeg(g, b, t / 2 + k, w - t / 2 - k, hm, t, x, y, h);
}
static void Colon(GpGraphics *g, GpBrush *b, float cx, float y, float h, float s)
{
    float vs[2] = { h * 0.30f, h * 0.70f };
    int i;
    for (i = 0; i < 2; i++) {
        float v = vs[i];
        float q[8] = { cx - s / 2, v - s / 2, cx + s / 2, v - s / 2, cx + s / 2, v + s / 2, cx - s / 2, v + s / 2 };
        Poly(g, b, q, 4, 0, y, h);
    }
}
/* indicators: 0 = run (triangle), 1 = pause (bars), 2 = alarm (bell) */
static void Indicator(GpGraphics *g, GpBrush *b, int kind, float x, float y)
{
    GpPath *p;
    if (kind == 0) {
        GpPointF q[3] = { { x, y }, { x + 9, y + 5 }, { x, y + 10 } };
        GdipFillPolygon(g, b, q, 3, FillModeAlternate);
    } else if (kind == 1) {
        GdipFillRectangle(g, b, x, y, 3.2f, 10);
        GdipFillRectangle(g, b, x + 5.6f, y, 3.2f, 10);
    } else {
        GdipCreatePath(FillModeWinding, &p);
        GdipAddPathArc(p, x + 1, y, 10, 12, 180, 180);
        GdipAddPathLine(p, x + 11, y + 6, x + 12, y + 9);
        GdipAddPathLine(p, x + 12, y + 9, x, y + 9);
        GdipAddPathLine(p, x, y + 9, x + 1, y + 6);
        GdipClosePathFigure(p);
        GdipAddPathEllipse(p, x + 4.2f, y + 9.4f, 3.6f, 2.6f);
        GdipFillPath(g, b, p);
        GdipDeletePath(p);
    }
}
static void DrawLcd(GpGraphics *g, long long now)
{
    const float bx = 12, by = 32, bw = W_LOG - 24, bh = 94;
    const float px = bx + 6, py = by + 6, pw = bw - 12, ph = bh - 12;
    GpPath *p;
    GpSolidFill *on, *off;
    int s = tl_display(&gT, now), dig[6], i, lit;
    float x, dw = 21, dh = 39, th = 4.6f, gapd = 5.0f, colw = 11, y = py + 19;
    /* bezel */
    p = RRect(bx, by, bw, bh, 11);
    FillGrad(g, p, bx, by, bw, bh, ARGB(255, 44, 40, 42), ARGB(255, 14, 12, 14));
    Stroke(g, p, ARGB(120, 255, 170, 175), 1.0f);
    GdipDeletePath(p);
    /* panel */
    p = RRect(px, py, pw, ph, 6);
    FillGrad(g, p, px, py, pw, ph, ARGB(255, 188, 198, 168), ARGB(255, 160, 172, 142));
    GdipDeletePath(p);
    p = RRect(px, py, pw, 10, 5);
    FillGrad(g, p, px, py, pw, 10, ARGB(60, 0, 0, 0), ARGB(0, 0, 0, 0));
    GdipDeletePath(p);
    GdipCreateSolidFill(ARGB(242, 24, 28, 22), &on);
    GdipCreateSolidFill(ARGB(30, 30, 40, 24), &off);
    /* indicators (right) and H / M / S captions over each digit group */
    Indicator(g, (GpBrush *)(gT.st == TL_RUN ? on : off), 0, px + pw - 52, py + 5);
    Indicator(g, (GpBrush *)(gT.st == TL_PAUSE ? on : off), 1, px + pw - 37, py + 5);
    Indicator(g, (GpBrush *)(gT.st == TL_ALARM && gBlinkOn ? on : off), 2, px + pw - 22, py + 4);
    {
        static const wchar_t *cap[3] = { L"H", L"M", L"S" };
        float gx = px + 9, grp = 2 * dw + gapd;
        for (i = 0; i < 3; i++) {
            float cx = gx + i * (grp + gapd + colw) + grp / 2 - 1.5f;
            Text(g, cap[i], cx - 15, y + dh + 0.5f, 30, 11, 7.5f, 1, ARGB(105, 40, 50, 34), 1);
        }
    }
    /* digits HH:MM:SS (blank leading hour) */
    dig[0] = s / 36000; dig[1] = s / 3600 % 10;
    dig[2] = s / 600 % 6; dig[3] = s / 60 % 10;
    dig[4] = s % 60 / 10; dig[5] = s % 10;
    if (dig[0] == 0) dig[0] = -1;
    lit = !(gT.st == TL_ALARM && !gBlinkOn);
    x = px + 9;
    for (i = 0; i < 6; i++) {
        Digit(g, (GpBrush *)on, (GpBrush *)off, lit ? dig[i] : -1, x, y, dw, dh, th);
        x += dw + gapd;
        if (i == 1 || i == 3) {
            Colon(g, (GpBrush *)(lit ? on : off), x + colw / 2 - gapd / 2 - 0.5f, y, dh, 4.6f);
            x += colw;
        }
    }
    GdipDeleteBrush((GpBrush *)on);
    GdipDeleteBrush((GpBrush *)off);
}

/* small round window buttons */
static void DrawSysBtn(GpGraphics *g, int id)
{
    RF r = gBtn[id];
    float cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    GpPath *p;
    GpPen *pen;
    ARGB ink = ARGB(235, 255, 238, 230);
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathEllipse(p, r.x + 1, r.y + 1, r.w - 2, r.h - 2);
    FillSolid(g, p, gPress == id ? ARGB(150, 70, 0, 10) : gHover == id ? ARGB(110, 255, 210, 200) : ARGB(60, 90, 0, 12));
    Stroke(g, p, ARGB(110, 255, 200, 200), 1);
    GdipDeletePath(p);
    GdipCreatePen1(ink, 1.7f, UnitWorld, &pen);
    GdipSetPenStartCap(pen, LineCapRound);
    GdipSetPenEndCap(pen, LineCapRound);
    if (id == B_CLOSE) {
        GdipDrawLine(g, pen, cx - 4, cy - 4, cx + 4, cy + 4);
        GdipDrawLine(g, pen, cx + 4, cy - 4, cx - 4, cy + 4);
    } else if (id == B_MIN) {
        GdipDrawLine(g, pen, cx - 4.5f, cy + 0.5f, cx + 4.5f, cy + 0.5f);
    } else {
        int k;
        GpPen *tp;
        GdipDrawEllipse(g, pen, cx - 3.4f, cy - 3.4f, 6.8f, 6.8f);
        GdipCreatePen1(ink, 2.4f, UnitWorld, &tp);
        for (k = 0; k < 6; k++) {
            float a = (float)(k * M_PI / 3);
            GdipDrawLine(g, tp, cx + cosf(a) * 4.4f, cy + sinf(a) * 4.4f, cx + cosf(a) * 6.4f, cy + sinf(a) * 6.4f);
        }
        GdipDeletePen(tp);
    }
    GdipDeletePen(pen);
}

static const wchar_t *Label(int id)
{
    static wchar_t d[2];
    switch (id) {
    case B_ADD5: return L"＋5分";
    case B_ADD1: return L"＋1分";
    case B_ADD30: return L"＋30秒";
    case B_RESET: return L"リセット";
    case B_START: return gT.st == TL_RUN ? L"ストップ" : gT.st == TL_PAUSE ? L"再開" : gT.st == TL_ALARM ? L"アラーム停止" : L"スタート";
    }
    d[0] = (wchar_t)(L'0' + id);
    d[1] = 0;
    return d;
}
static void DrawKey(GpGraphics *g, int id)
{
    RF r = gBtn[id];
    int pressed = gPress == id;
    float o = pressed ? 1.2f : 0;
    GpPath *p;
    GpTexture *tex = id == B_START ? gChk[2] : (id >= B_ADD5 && id <= B_RESET) ? gChk[1] : gChk[0];
    float fs = id < 10 ? 19 : id == B_START ? 16 : 13.5f;
    /* drop shadow */
    p = RRect(r.x + 0.5f, r.y + 2.2f, r.w - 1, r.h, 9);
    FillSolid(g, p, ARGB(pressed ? 30 : 70, 60, 0, 6));
    GdipDeletePath(p);
    p = RRect(r.x, r.y + o, r.w, r.h - o * 0.5f, 9);
    GdipResetTextureTransform(tex);
    GdipTranslateTextureTransform(tex, r.x, r.y, MatrixOrderPrepend);
    GdipFillPath(g, (GpBrush *)tex, p);
    /* glossy top, refraction glow at the bottom */
    FillGrad(g, p, r.x, r.y + o, r.w, r.h * 0.55f, ARGB(pressed ? 25 : 78, 255, 255, 255), ARGB(0, 255, 255, 255));
    {
        GpPath *q = RRect(r.x + 3, r.y + r.h * 0.55f + o, r.w - 6, r.h * 0.45f - 2.5f, 7);
        FillGrad(g, q, r.x, r.y + r.h * 0.55f + o, r.w, r.h * 0.45f, ARGB(0, 255, 120, 110), ARGB(pressed ? 20 : 55, 255, 150, 120));
        GdipDeletePath(q);
    }
    if (pressed) FillSolid(g, p, ARGB(55, 50, 0, 8));
    else if (gHover == id) FillSolid(g, p, ARGB(26, 255, 245, 220));
    Stroke(g, p, ARGB(150, 255, 196, 196), 1.0f);
    GdipDeletePath(p);
    Text(g, Label(id), r.x, r.y + o + 1.2f, r.w, r.h, fs, 1, ARGB(110, 70, 0, 8), 1);
    Text(g, Label(id), r.x, r.y + o, r.w, r.h, fs, 1, ARGB(250, 255, 250, 240), 1);
}

static void DrawAll(GpGraphics *g, long long now)
{
    GpPath *p;
    int i;
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipSetTextRenderingHint(g, TextRenderingHintAntiAlias);
    GdipSetCompositingQuality(g, CompositingQualityHighQuality);
    /* body: see-through crystal red */
    p = RRect(0.5f, 0.5f, W_LOG - 1, H_LOG - 1, 20);
    FillGrad(g, p, 0, 0, W_LOG, H_LOG, ARGB(214, 214, 30, 50), ARGB(228, 128, 4, 22));
    GdipSetClipPath(g, p, CombineModeReplace);
    {   /* diagonal light streaks like clear plastic */
        GpSolidFill *b;
        GpPointF s1[4] = { { 30, 0 }, { 120, 0 }, { -40, H_LOG }, { -130, H_LOG } };
        GpPointF s2[4] = { { 150, 0 }, { 172, 0 }, { 12, H_LOG }, { -10, H_LOG } };
        GdipCreateSolidFill(ARGB(16, 255, 230, 230), &b);
        GdipFillPolygon(g, (GpBrush *)b, s1, 4, FillModeAlternate);
        GdipFillPolygon(g, (GpBrush *)b, s2, 4, FillModeAlternate);
        GdipDeleteBrush((GpBrush *)b);
    }
    GdipResetClip(g);
    {
        GpPath *q = RRect(3, 3, W_LOG - 6, 120, 18);
        FillGrad(g, q, 3, 3, W_LOG - 6, 120, ARGB(70, 255, 255, 255), ARGB(0, 255, 255, 255));
        GdipDeletePath(q);
    }
    Stroke(g, p, ARGB(255, 92, 0, 12), 1.0f);
    GdipDeletePath(p);
    p = RRect(2, 2, W_LOG - 4, H_LOG - 4, 18.5f);
    Stroke(g, p, ARGB(120, 255, 190, 190), 1.2f);
    GdipDeletePath(p);
    Text(g, L"KEYPAD TIMER", 16, 6, 140, 20, 10.5f, 1, ARGB(120, 90, 0, 10), 0);
    Text(g, L"KEYPAD TIMER", 16, 5, 140, 20, 10.5f, 1, ARGB(215, 255, 232, 220), 0);
    DrawSysBtn(g, B_SET);
    DrawSysBtn(g, B_MIN);
    DrawSysBtn(g, B_CLOSE);
    DrawLcd(g, now);
    for (i = 0; i < B_SET; i++) DrawKey(g, i);
}

/* Render into a premultiplied 32bpp DIB; returns the bitmap (caller frees). */
static HBITMAP RenderDib(long long now, float scale, int *pw, int *ph, void **pbits)
{
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER) } };
    int w = (int)ceilf(W_LOG * scale), h = (int)ceilf(H_LOG * scale);
    void *bits;
    HBITMAP hb;
    GpBitmap *bm;
    GpGraphics *g;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    hb = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hb) return NULL;
    ZeroMemory(bits, (size_t)w * h * 4);
    GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, (BYTE *)bits, &bm);
    GdipGetImageGraphicsContext((GpImage *)bm, &g);
    GdipScaleWorldTransform(g, scale, scale, MatrixOrderPrepend);
    DrawAll(g, now);
    GdipDeleteGraphics(g);
    GdipDisposeImage((GpImage *)bm);
    *pw = w; *ph = h; *pbits = bits;
    return hb;
}

static void UpdateTitle(int s)
{
    wchar_t t[64];
    if (s == gLastTitle) return;
    gLastTitle = s;
    swprintf(t, 64, L"%d:%02d:%02d - " APPNAME, s / 3600, s / 60 % 60, s % 60);
    SetWindowTextW(gWnd, t);
}

static void Render(void)
{
    long long now = Now();
    int w = 0, h = 0;
    void *bits;
    HBITMAP hb = RenderDib(now, gScale, &w, &h, &bits);
    HDC sdc, mdc;
    HGDIOBJ old;
    POINT src = { 0, 0 };
    SIZE sz;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    RECT r;
    POINT pos;
    if (!hb) return;
    GetWindowRect(gWnd, &r);
    pos.x = r.left; pos.y = r.top;
    sz.cx = w; sz.cy = h;
    sdc = GetDC(NULL);
    mdc = CreateCompatibleDC(sdc);
    old = SelectObject(mdc, hb);
    UpdateLayeredWindow(gWnd, sdc, &pos, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
    SelectObject(mdc, old);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
    DeleteObject(hb);
    UpdateTitle(tl_display(&gT, now));
}

/* ---------- timer scheduling ---------- */
static void Schedule(void)
{
    long long d = tl_next_change_ms(&gT, Now());
    KillTimer(gWnd, TM_TICK);
    if (d >= 0) SetTimer(gWnd, TM_TICK, (UINT)(d + 2), NULL);
    if (gT.st == TL_ALARM) SetTimer(gWnd, TM_ALARM, 500, NULL);
    else KillTimer(gWnd, TM_ALARM);
}
static void EnterAlarm(void)
{
    FLASHWINFO fi = { sizeof fi, gWnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
    gBlinkOn = 1;
    if (IsIconic(gWnd)) ShowWindow(gWnd, SW_SHOWNOACTIVATE);
    AlarmStart();
    FlashWindowEx(&fi);
}
static void LeaveAlarmIfNeeded(TLState before)
{
    if (before == TL_ALARM && gT.st != TL_ALARM) {
        FLASHWINFO fi = { sizeof fi, gWnd, FLASHW_STOP, 0, 0 };
        AlarmStop();
        FlashWindowEx(&fi);
    }
}

static void Press(int id)
{
    long long now = Now();
    TLState before = gT.st;
    int changed = 0;
    if (id == B_CLOSE) { PostMessageW(gWnd, WM_CLOSE, 0, 0); return; }
    if (id == B_MIN) { ShowWindow(gWnd, SW_MINIMIZE); return; }
    if (id == B_SET) { PostMessageW(gWnd, WM_COMMAND, 1, 0); return; }
    if (before == TL_ALARM) AlarmStop();   /* silence first, then click */
    PlayShot(0.045, 0);
    if (id <= 9) changed = tl_digit(&gT, id, now);
    else if (id == B_ADD5) changed = tl_add(&gT, 300, now);
    else if (id == B_ADD1) changed = tl_add(&gT, 60, now);
    else if (id == B_ADD30) changed = tl_add(&gT, 30, now);
    else if (id == B_RESET) changed = tl_reset(&gT, now);
    else if (id == B_START) changed = tl_startstop(&gT, now);
    (void)changed;
    LeaveAlarmIfNeeded(before);
    Schedule();
}

static int HitTest(int cx, int cy)
{
    float x = cx / gScale, y = cy / gScale;
    int i;
    for (i = 0; i < B_COUNT; i++) {
        RF r = gBtn[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
    }
    return -1;
}

/* ---------- settings dialog ---------- */
static int gSaveVol;
static void SetVolText(HWND d, int v)
{
    wchar_t b[16];
    swprintf(b, 16, L"%d %%", v);
    SetDlgItemTextW(d, IDC_VOLTXT, b);
}
static INT_PTR CALLBACK SettingsProc(HWND d, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        gSaveVol = gCfg.vol;
        SendDlgItemMessageW(d, IDC_VOL, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendDlgItemMessageW(d, IDC_VOL, TBM_SETPAGESIZE, 0, 10);
        SendDlgItemMessageW(d, IDC_VOL, TBM_SETPOS, TRUE, gCfg.vol);
        SetVolText(d, gCfg.vol);
        SendDlgItemMessageW(d, IDC_FADESPIN, UDM_SETRANGE32, 0, 600);
        SendDlgItemMessageW(d, IDC_FADESPIN, UDM_SETPOS32, 0, gCfg.fade);
        SendDlgItemMessageW(d, IDC_HOLDSPIN, UDM_SETRANGE32, 1, 60);
        SendDlgItemMessageW(d, IDC_HOLDSPIN, UDM_SETPOS32, 0, gCfg.hold);
        CheckDlgButton(d, IDC_TOP, gCfg.top ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    case WM_HSCROLL:
        if ((HWND)lp == GetDlgItem(d, IDC_VOL)) {
            gCfg.vol = (int)SendDlgItemMessageW(d, IDC_VOL, TBM_GETPOS, 0, 0);
            SetVolText(d, gCfg.vol);
            if (LOWORD(wp) == TB_ENDTRACK || LOWORD(wp) == TB_THUMBPOSITION) PlayShot(0.045, 0);
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_TEST:
            PlayShot(1.0, 1);
            return TRUE;
        case IDOK: {
            BOOL ok1, ok2;
            int f = (int)GetDlgItemInt(d, IDC_FADE, &ok1, FALSE), h = (int)GetDlgItemInt(d, IDC_HOLD, &ok2, FALSE);
            gCfg.fade = ok1 ? Clamp(f, 0, 600) : gCfg.fade;
            gCfg.hold = ok2 ? Clamp(h, 1, 60) : gCfg.hold;
            gCfg.top = IsDlgButtonChecked(d, IDC_TOP) == BST_CHECKED;
            EndDialog(d, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            gCfg.vol = gSaveVol;
            EndDialog(d, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
static void ApplyTopmost(void)
{
    SetWindowPos(gWnd, gCfg.top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

/* ---------- window ---------- */
static UINT Dpi(HWND h)
{
    typedef UINT(WINAPI * FN)(HWND);
    static FN fn;
    static int tried;
    UINT d = 0;
    HDC dc;
    if (!tried) { tried = 1; fn = (FN)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"); }
    if (fn && h) d = fn(h);
    if (!d) { dc = GetDC(NULL); d = (UINT)GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(NULL, dc); }
    return d ? d : 96;
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(h, &pt);
        return HitTest(pt.x, pt.y) >= 0 ? HTCLIENT : HTCAPTION;
    }
    case WM_NCLBUTTONDBLCLK:
        return 0;
    case WM_LBUTTONDOWN: {
        int id = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (id < 0) return 0;
        gPress = id;
        SetCapture(h);
        if (id < B_SET) Press(id);
        Render();
        return 0;
    }
    case WM_LBUTTONUP: {
        int id = gPress, over = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        gPress = -1;
        ReleaseCapture();
        Render();
        if (id >= B_SET && id == over) Press(id);  /* window buttons act on release */
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (gPress >= 0) { gPress = -1; Render(); }
        return 0;
    case WM_MOUSEMOVE: {
        int id = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (id != gHover) {
            TRACKMOUSEEVENT te = { sizeof te, TME_LEAVE, h, 0 };
            gHover = id;
            TrackMouseEvent(&te);
            Render();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
    case WM_NCMOUSEMOVE:
        if (gHover >= 0) { gHover = -1; Render(); }
        break;
    case WM_CONTEXTMENU: {
        HMENU m;
        int cmd;
        if (lp == -1) return 0;  /* keyboard-invoked: ignore */
        m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, 1, L"設定...");
        AppendMenuW(m, MF_STRING | (gCfg.top ? MF_CHECKED : 0), 2, L"常に手前に表示");
        AppendMenuW(m, MF_STRING, 3, L"最小化");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m, MF_STRING, 4, L"終了");
        cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0, h, NULL);
        DestroyMenu(m);
        if (cmd == 2) { gCfg.top = !gCfg.top; ApplyTopmost(); SaveCfg(); }
        else if (cmd == 3) ShowWindow(h, SW_MINIMIZE);
        else if (cmd == 4) PostMessageW(h, WM_CLOSE, 0, 0);
        else if (cmd == 1) PostMessageW(h, WM_COMMAND, 1, 0);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) {
            if (DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SETTINGS), h, SettingsProc) == IDOK) {
                ApplyTopmost();
                SaveCfg();
            }
        }
        return 0;
    case WM_TIMER:
        if (wp == TM_TICK) {
            KillTimer(h, TM_TICK);
            if (tl_tick(&gT, Now())) EnterAlarm();
            Render();
            Schedule();
        } else if (wp == TM_ALARM) {
            if (gT.st != TL_ALARM) { KillTimer(h, TM_ALARM); return 0; }
            gBlinkOn = !gBlinkOn;
            if ((Now() - gT.alarm_ms) >= (gCfg.fade + gCfg.hold * 60LL) * 1000 + 300) {
                tl_stop_alarm(&gT);       /* auto-stop: back to the measured time */
                LeaveAlarmIfNeeded(TL_ALARM);
                Schedule();
            }
            Render();
        } else if (wp == TM_CLICKCLOSE) {
            KillTimer(h, TM_CLICKCLOSE);
            ClickClose();
        }
        return 0;
    case MM_WOM_DONE:
        AlarmDone((HWAVEOUT)wp, (WAVEHDR *)lp);
        return 0;
    /* no keyboard operation at all */
    case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_SYSCHAR:
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_DPICHANGED: {
        RECT *r = (RECT *)lp;
        gScale = HIWORD(wp) / 96.0f;
        SetWindowPos(h, NULL, r->left, r->top, (int)ceilf(W_LOG * gScale), (int)ceilf(H_LOG * gScale), SWP_NOZORDER | SWP_NOACTIVATE);
        Render();
        return 0;
    }
    case WM_CLOSE:
        if ((gT.st == TL_RUN || gT.st == TL_PAUSE) &&
            MessageBoxW(h, L"タイマーが動いています。終了しますか？", APPNAME, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 0;
        DestroyWindow(h);
        return 0;
    case WM_ENDSESSION:
        if (wp) SaveCfg();
        return 0;
    case WM_DESTROY:
        AlarmStop();
        ClickClose();
        SaveCfg();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void GdipInit(void)
{
    GdiplusStartupInput in = { 1, NULL, FALSE, FALSE };
    GdiplusStartup(&gGdipToken, &in, NULL);
    InitFont();
    InitTextures();
}

#ifdef SHOTS
/* Developer build: render states to PNG for screenshots (no window). */
static void Shot(const wchar_t *file, const wchar_t *what, float scale)
{
    static const CLSID png = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    int w, h, i;
    void *bits;
    HBITMAP hb;
    GpBitmap *bm;
    long long t0 = 1000000;
    tl_init(&gT, 0);
    if (!wcscmp(what, L"entered")) { tl_digit(&gT, 1, 0); tl_digit(&gT, 3, 0); tl_digit(&gT, 0, 0); }
    else if (!wcscmp(what, L"pressed")) { tl_digit(&gT, 5, 0); gPress = 5; }
    else if (!wcscmp(what, L"running") || !wcscmp(what, L"paused") || !wcscmp(what, L"alarm") || !wcscmp(what, L"reset")) {
        tl_digit(&gT, 2, 0); tl_digit(&gT, 5, 0);
        tl_startstop(&gT, t0);
        if (!wcscmp(what, L"paused")) tl_startstop(&gT, t0 + 746000);
        if (!wcscmp(what, L"alarm")) tl_tick(&gT, t0 + 1500000);
        if (!wcscmp(what, L"reset")) tl_reset(&gT, t0 + 5000);
        t0 += 746000;   /* 12:26 elapsed -> 12:34 left */
        gHover = !wcscmp(what, L"running") ? B_START : -1;
    }
    for (i = 0; i < 1; i++) {
        hb = RenderDib(t0, scale, &w, &h, &bits);
        GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, (BYTE *)bits, &bm);
        GdipSaveImageToFile((GpImage *)bm, file, &png, NULL);
        GdipDisposeImage((GpImage *)bm);
        DeleteObject(hb);
    }
}
#endif

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show)
{
    WNDCLASSEXW wc = { sizeof wc };
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_BAR_CLASSES | ICC_UPDOWN_CLASS };
    MSG m;
    int last = 0, w, h;
    RECT wa;
    (void)hp; (void)cmd;
    Layout();
    GdipInit();
#ifdef SHOTS
    {
        int argc;
        wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argc >= 4 && !wcscmp(argv[1], L"--shot")) {
            Shot(argv[2], argv[3], argc >= 5 ? (float)_wtof(argv[4]) : 2.0f);
            return 0;
        }
    }
#endif
    InitCommonControlsEx(&icc);
    IniPath();
    LoadCfg(&last);
    tl_init(&gT, last);

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, 0);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = CLASSNAME;
    RegisterClassExW(&wc);

    gScale = Dpi(NULL) / 96.0f;
    w = (int)ceilf(W_LOG * gScale);
    h = (int)ceilf(H_LOG * gScale);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (gCfg.x == INT_MIN || gCfg.y == INT_MIN) { gCfg.x = wa.right - w - 24; gCfg.y = wa.top + 24; }
    {   /* keep on a visible monitor */
        POINT c = { gCfg.x + w / 2, gCfg.y + 20 };
        if (!MonitorFromPoint(c, MONITOR_DEFAULTTONULL)) { gCfg.x = wa.right - w - 24; gCfg.y = wa.top + 24; }
    }
    gWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_APPWINDOW | WS_EX_NOACTIVATE | (gCfg.top ? WS_EX_TOPMOST : 0),
                           CLASSNAME, APPNAME, WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
                           gCfg.x, gCfg.y, w, h, NULL, NULL, hi, NULL);
    if (!gWnd) return 1;
    {
        float s = Dpi(gWnd) / 96.0f;
        if (s != gScale) { gScale = s; SetWindowPos(gWnd, NULL, 0, 0, (int)ceilf(W_LOG * s), (int)ceilf(H_LOG * s), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); }
    }
    Render();
    ShowWindow(gWnd, show == SW_SHOWMINIMIZED || show == SW_MINIMIZE ? show : SW_SHOWNOACTIVATE);
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN || m.message == WM_SYSKEYDOWN) {
            /* still let the settings dialog receive keys */
            HWND top = GetAncestor(m.hwnd, GA_ROOT);
            if (top == gWnd) continue;
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    GdiplusShutdown(gGdipToken);
    return 0;
}
