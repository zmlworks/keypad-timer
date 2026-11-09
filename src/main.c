/*
 * Keypad Timer (キーパッドタイマー) - a tiny always-on-top kitchen-style countdown timer.
 * Win32 + GDI+ (flat API), no runtime needed.  MIT License, Copyright (c) 2026 zmlworks
 * Mouse only: keyboard input is ignored on purpose (mis-operation guard).
 */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <gdiplus.h>
#include <math.h>
#include <stdio.h>
#include "tlogic.h"

#define APPNAME L"キーパッドタイマー"
#define W_LOG 240   /* logical size @96 dpi */
#define H_LOG 420
#define ARGB(a, r, g, b) ((ARGB)(((DWORD)(a) << 24) | ((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b)))

/* ---------- buttons ---------- */
enum { B_ADD5 = 10, B_ADD1, B_ADD30, B_BS, B_RESET, B_START, B_PIN, B_CLOSE, B_COUNT }; /* 0..9 = digits */
typedef struct { float x, y, w, h; } RF;
static RF gBtn[B_COUNT];

static void Layout(void)
{
    const float m = 12, gap = 8, cw = (W_LOG - 2 * m - 2 * gap) / 3, rh = 38, y = 180;
    int i;
    for (i = 0; i < 3; i++) gBtn[B_ADD5 + i] = (RF){ m + i * (cw + gap), 136, cw, 32 };
    for (i = 1; i <= 9; i++)                     /* 7 8 9 / 4 5 6 / 1 2 3 */
        gBtn[i] = (RF){ m + (i - 1) % 3 * (cw + gap), y + (2 - (i - 1) / 3) * (rh + gap), cw, rh };
    gBtn[B_BS]    = (RF){ m, y + 3 * (rh + gap), cw, rh };   /* BS | 0 | リセット */
    gBtn[0]       = (RF){ m + cw + gap, y + 3 * (rh + gap), cw, rh };
    gBtn[B_RESET] = (RF){ m + 2 * (cw + gap), y + 3 * (rh + gap), cw, rh };
    gBtn[B_START] = (RF){ m, y + 4 * (rh + gap), W_LOG - 2 * m, 44 };
    gBtn[B_PIN]   = (RF){ W_LOG - 56, 6, 20, 20 };
    gBtn[B_CLOSE] = (RF){ W_LOG - 32, 6, 20, 20 };
}

/* ---------- state ---------- */
static HWND gWnd;
static Timer gT;
static float gScale = 1;
static int gHover = -1, gPress = -1, gTop = 0, gBlinkOn = 1, gRinging;
static GpFontFamily *gFam;
enum { TM_TICK = 1, TM_ALARM, TM_CLICKCLOSE };

static long long Now(void) { return (long long)GetTickCount64(); }

/* ---------- sound (synthesized, waveOut) ---------- */
#define ABUF (TL_SR / 10)
#define CLICK (TL_SR * 45 / 1000)
static HWAVEOUT gAWo, gCWo;
static WAVEHDR gAH[4], gCH;
static short gABuf[4][ABUF], gCBuf[CLICK];
static long long gASamp;
static const WAVEFORMATEX gFmt = { WAVE_FORMAT_PCM, 1, TL_SR, TL_SR * 2, 2, 16, 0 };

static void AlarmFill(WAVEHDR *h)
{
    short *b = (short *)h->lpData;
    int n;
    for (n = 0; n < ABUF; n++) b[n] = tl_alarm_sample(gASamp + n);
    gASamp += ABUF;
}
static void AlarmStart(void)
{
    int i;
    if (waveOutOpen(&gAWo, WAVE_MAPPER, &gFmt, (DWORD_PTR)gWnd, 0, CALLBACK_WINDOW)) { gAWo = NULL; return; }
    gASamp = 0;
    for (i = 0; i < 4; i++) {
        gAH[i] = (WAVEHDR){ .lpData = (LPSTR)gABuf[i], .dwBufferLength = sizeof gABuf[i] };
        AlarmFill(&gAH[i]);
        waveOutPrepareHeader(gAWo, &gAH[i], sizeof gAH[i]);
        waveOutWrite(gAWo, &gAH[i], sizeof gAH[i]);
    }
}
static void AlarmStop(void)
{
    HWAVEOUT wo = gAWo;
    int i;
    if (!wo) return;
    gAWo = NULL;   /* MM_WOM_DONE from the reset below is ignored */
    waveOutReset(wo);
    for (i = 0; i < 4; i++) waveOutUnprepareHeader(wo, &gAH[i], sizeof gAH[i]);
    waveOutClose(wo);
}
static void Click(void)   /* same loudness as the alarm at full, so a muted PC is noticed */
{
    if (!gCWo && waveOutOpen(&gCWo, WAVE_MAPPER, &gFmt, 0, 0, CALLBACK_NULL)) { gCWo = NULL; return; }
    waveOutReset(gCWo);
    waveOutUnprepareHeader(gCWo, &gCH, sizeof gCH);
    gCH = (WAVEHDR){ .lpData = (LPSTR)gCBuf, .dwBufferLength = sizeof gCBuf };
    waveOutPrepareHeader(gCWo, &gCH, sizeof gCH);
    waveOutWrite(gCWo, &gCH, sizeof gCH);
    SetTimer(gWnd, TM_CLICKCLOSE, 3000, NULL);
}
static void ClickClose(void)
{
    if (!gCWo) return;
    waveOutReset(gCWo);
    waveOutUnprepareHeader(gCWo, &gCH, sizeof gCH);
    waveOutClose(gCWo);
    gCWo = NULL;
}

/* ---------- drawing (flat) ---------- */
static GpPath *RRect(RF r, float rad)
{
    GpPath *p;
    float d = rad * 2;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathArc(p, r.x, r.y, d, d, 180, 90);
    GdipAddPathArc(p, r.x + r.w - d, r.y, d, d, 270, 90);
    GdipAddPathArc(p, r.x + r.w - d, r.y + r.h - d, d, d, 0, 90);
    GdipAddPathArc(p, r.x, r.y + r.h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}
static void FillRR(GpGraphics *g, RF r, float rad, GpBrush *b)
{
    GpPath *p = RRect(r, rad);
    GdipFillPath(g, b, p);
    GdipDeletePath(p);
}
static void FillRRc(GpGraphics *g, RF r, float rad, ARGB c)
{
    GpSolidFill *b;
    GdipCreateSolidFill(c, &b);
    FillRR(g, r, rad, (GpBrush *)b);
    GdipDeleteBrush((GpBrush *)b);
}
static float Text(GpGraphics *g, const wchar_t *s, RF r, float size, ARGB c, int measure)
{
    GpFont *f;
    GpStringFormat *sf;
    GpSolidFill *b;
    GpRectF lay = { r.x, r.y, r.w, r.h }, box = { 0, 0, 0, 0 };
    GdipCreateFont(gFam, size, FontStyleBold, UnitWorld, &f);
    GdipStringFormatGetGenericTypographic(&sf);
    GdipSetStringFormatAlign(sf, StringAlignmentCenter);
    GdipSetStringFormatLineAlign(sf, StringAlignmentCenter);
    if (measure) GdipMeasureString(g, s, -1, f, &lay, sf, &box, NULL, NULL);
    else {
        GdipCreateSolidFill(c, &b);
        GdipDrawString(g, s, -1, f, &lay, sf, (GpBrush *)b);
        GdipDeleteBrush((GpBrush *)b);
    }
    GdipDeleteStringFormat(sf);
    GdipDeleteFont(f);
    return box.Width;
}
static void InitGraphics(void)
{
    static const wchar_t *names[] = { L"Meiryo UI", L"Yu Gothic UI", L"Meiryo", L"Noto Sans CJK JP", L"Segoe UI" };
    int i;
    for (i = 0; i < 5 && GdipCreateFontFamilyFromName(names[i], NULL, &gFam) != Ok; i++) {}
    if (i == 5) GdipGetGenericFontFamilySansSerif(&gFam);
}

/* 7-segment digits, slanted; unlit segments stay faintly visible */
static const unsigned char SEG[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
#define SLANT 0.10f
static void Poly(GpGraphics *g, GpBrush *b, const float *uv, int n, float x, float y, float h)
{
    GpPointF p[6];
    int i;
    for (i = 0; i < n; i++) p[i] = (GpPointF){ x + uv[2 * i] + (h - uv[2 * i + 1]) * SLANT, y + uv[2 * i + 1] };
    GdipFillPolygon(g, b, p, n, FillModeAlternate);
}
static void Seg(GpGraphics *g, GpBrush *b, int horiz, float a0, float a1, float c, float t, float x, float y, float h)
{
    float q[12] = { a0, c, a0 + t / 2, c - t / 2, a1 - t / 2, c - t / 2, a1, c, a1 - t / 2, c + t / 2, a0 + t / 2, c + t / 2 };
    int i;
    if (!horiz) for (i = 0; i < 12; i += 2) { float u = q[i]; q[i] = q[i + 1]; q[i + 1] = u; }
    Poly(g, b, q, 6, x, y, h);
}
static void Digit(GpGraphics *g, GpBrush *on, GpBrush *off, int d, float x, float y, float w, float h, float t)
{
    const float k = 1, m = h / 2, l = t / 2, r = w - t / 2;
    unsigned s = d < 0 ? 0 : SEG[d];
    Seg(g, s & 0x01 ? on : off, 1, l + k, r - k, l, t, x, y, h);        /* a */
    Seg(g, s & 0x02 ? on : off, 0, l + k, m - k, r, t, x, y, h);        /* b */
    Seg(g, s & 0x04 ? on : off, 0, m + k, h - l - k, r, t, x, y, h);    /* c */
    Seg(g, s & 0x08 ? on : off, 1, l + k, r - k, h - l, t, x, y, h);    /* d */
    Seg(g, s & 0x10 ? on : off, 0, m + k, h - l - k, l, t, x, y, h);    /* e */
    Seg(g, s & 0x20 ? on : off, 0, l + k, m - k, l, t, x, y, h);        /* f */
    Seg(g, s & 0x40 ? on : off, 1, l + k, r - k, m, t, x, y, h);        /* g */
}
static void DrawLcd(GpGraphics *g, long long now)
{
    static const wchar_t *cap[3] = { L"H", L"M", L"S" };
    const float dw = 24.5f, dh = 52, th = 7, gap = 4.4f, colw = 10.5f, y = 45;
    int s = tl_display(&gT, now), dig[6], i, lit = !(gT.st == TL_ALARM && !gBlinkOn), colon = lit && tl_colon_on(&gT, now);
    float x = 21, cs = 6.2f;
    GpSolidFill *on, *off;
    FillRRc(g, (RF){ 12, 32, W_LOG - 24, 94 }, 10, ARGB(255, 34, 30, 32));     /* bezel */
    FillRRc(g, (RF){ 18, 38, W_LOG - 36, 82 }, 6, ARGB(255, 176, 188, 156));   /* panel */
    GdipCreateSolidFill(ARGB(255, 24, 28, 22), &on);
    GdipCreateSolidFill(ARGB(46, 30, 40, 24), &off);
    dig[0] = s / 36000 ? s / 36000 : -1; dig[1] = s / 3600 % 10;   /* H H : M M : S S */
    dig[2] = s / 600 % 6; dig[3] = s / 60 % 10;
    dig[4] = s % 60 / 10; dig[5] = s % 10;
    for (i = 0; i < 3; i++)   /* H / M / S under each digit pair */
        Text(g, cap[i], (RF){ x + i * (2 * (dw + gap) + colw), y + dh, 2 * dw + gap, 14 }, 8, ARGB(150, 40, 50, 34), 0);
    for (i = 0; i < 6; i++) {
        Digit(g, (GpBrush *)on, (GpBrush *)off, lit ? dig[i] : -1, x, y, dw, dh, th);
        x += dw + gap;
        if (i == 1 || i == 3) {
            float c = x + colw / 2 - gap / 2 - 0.5f, v;
            for (v = 0.30f; v < 0.8f; v += 0.40f) {   /* colon: two slanted squares */
                float q[8] = { c - cs / 2, dh * v - cs / 2, c + cs / 2, dh * v - cs / 2, c + cs / 2, dh * v + cs / 2, c - cs / 2, dh * v + cs / 2 };
                Poly(g, (GpBrush *)(colon ? on : off), q, 4, 0, y, dh);   /* blinks while running */
            }
            x += colw;
        }
    }
    GdipDeleteBrush((GpBrush *)on);
    GdipDeleteBrush((GpBrush *)off);
}

static void DrawSysBtn(GpGraphics *g, int id)
{
    RF r = gBtn[id];
    float cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    int on = id == B_PIN && gTop;
    ARGB ink = ARGB(255, 255, 236, 230);
    GpPen *pen;
    GpSolidFill *b;
    GdipCreateSolidFill(gPress == id ? ARGB(255, 110, 8, 22) : on ? ARGB(255, 90, 22, 28)
                        : gHover == id ? ARGB(255, 226, 92, 100) : ARGB(255, 150, 14, 32), &b);
    GdipFillEllipse(g, (GpBrush *)b, r.x, r.y, r.w, r.h);
    GdipDeleteBrush((GpBrush *)b);
    GdipCreatePen1(ink, 1.7f, UnitWorld, &pen);
    GdipSetPenLineJoin(pen, LineJoinRound);
    if (id == B_CLOSE) {
        GdipDrawLine(g, pen, cx - 4, cy - 4, cx + 4, cy + 4);
        GdipDrawLine(g, pen, cx + 4, cy - 4, cx - 4, cy + 4);
    } else {   /* push pin like StickyBB: off = tilted outline, on = upright and filled */
        static const float shape[8][2] = { { -3.5f, -7 }, { 3.5f, -7 }, { 2.5f, -5.5f }, { 2.5f, -1 },
                                           { 5, 1.5f }, { -5, 1.5f }, { -2.5f, -1 }, { -2.5f, -5.5f } };
        const float u = 0.72f, c = on ? 1 : 0.7071f, s = on ? 0 : 0.7071f;
        GpPointF pt[8];
        int k;
        for (k = 0; k < 8; k++) pt[k] = (GpPointF){ cx + u * (shape[k][0] * c - shape[k][1] * s), cy + u * (shape[k][0] * s + shape[k][1] * c) };
        if (on) {
            GdipCreateSolidFill(ink, &b);
            GdipFillPolygon(g, (GpBrush *)b, pt, 8, FillModeAlternate);
            GdipDeleteBrush((GpBrush *)b);
        }
        GdipSetPenWidth(pen, 1.2f);
        GdipDrawPolygon(g, pen, pt, 8);
        GdipSetPenWidth(pen, 1.7f);
        GdipDrawLine(g, pen, cx - 1.5f * u * s, cy + 1.5f * u * c, cx - 7 * u * s, cy + 7 * u * c);
    }
    GdipDeletePen(pen);
}

static void DrawKey(GpGraphics *g, int id)
{
    static const wchar_t *lab[] = { L"5", L"1", L"30", L"BS", L"リセット", L"スタート / ストップ" };
    RF r = gBtn[id];
    int cream = id == 2 || id == 4 || id == 6 || id == 8;
    ARGB fill = id >= B_ADD5 && id <= B_RESET ? ARGB(255, 140, 12, 30) : cream ? ARGB(255, 251, 238, 200)
              : id == 5 ? ARGB(255, 204, 34, 52) : ARGB(255, 226, 52, 66);
    ARGB ink = cream ? ARGB(255, 150, 14, 32) : ARGB(255, 255, 248, 240);
    wchar_t d[2] = { (wchar_t)(L'0' + id), 0 };
    const wchar_t *s = id < 10 ? d : lab[id - B_ADD5];
    FillRRc(g, r, 8, fill);
    if (gPress == id) FillRRc(g, r, 8, ARGB(70, 60, 0, 10));
    else if (gHover == id) FillRRc(g, r, 8, ARGB(40, 255, 255, 255));
    if (id >= B_ADD5 && id <= B_ADD30) {   /* big number + small unit */
        const wchar_t *unit = id == B_ADD30 ? L"秒" : L"分";
        float wn = Text(g, s, r, 19, 0, 1), wu = Text(g, unit, r, 10.5f, 0, 1), x0 = r.x + (r.w - wn - wu - 1.5f) / 2;
        Text(g, s, (RF){ x0, r.y, wn, r.h }, 19, ink, 0);
        Text(g, unit, (RF){ x0 + wn + 1.5f, r.y + 3.5f, wu, r.h }, 10.5f, ink, 0);
    } else
        Text(g, s, r, id < 10 ? 20 : 15, ink, 0);
}

/* only the case has some depth: a soft vertical shade and a bevelled rim */
static void DrawCase(GpGraphics *g)
{
    static const REAL pos[3] = { 0, 0.5f, 1 };
    const ARGB rim[3] = { ARGB(120, 255, 255, 255), ARGB(0, 255, 255, 255), ARGB(110, 40, 0, 8) };
    GpRectF rf = { 0, -1, W_LOG, H_LOG + 2 };
    GpLineGradient *b;
    GpPen *pen;
    GpPath *p = RRect((RF){ 1.5f, 1.5f, W_LOG - 3, H_LOG - 3 }, 16.5f);
    GdipCreateLineBrushFromRect(&rf, ARGB(255, 200, 28, 48), ARGB(255, 164, 14, 34), LinearGradientModeVertical, WrapModeTileFlipXY, &b);
    FillRR(g, (RF){ 0, 0, W_LOG, H_LOG }, 18, (GpBrush *)b);
    GdipSetLinePresetBlend(b, rim, pos, 3);
    GdipCreatePen2((GpBrush *)b, 3, UnitWorld, &pen);
    GdipDrawPath(g, pen, p);
    GdipDeletePen(pen);
    GdipDeletePath(p);
    GdipDeleteBrush((GpBrush *)b);
}

static void DrawAll(GpGraphics *g, long long now)
{
    int i;
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipSetTextRenderingHint(g, TextRenderingHintAntiAlias);
    DrawCase(g);
    Text(g, L"KEYPAD TIMER", (RF){ 14, 6, 110, 20 }, 10.5f, ARGB(220, 255, 228, 222), 0);
    DrawLcd(g, now);
    for (i = 0; i < B_COUNT; i++) {
        if (i >= B_PIN) DrawSysBtn(g, i);
        else DrawKey(g, i);
    }
}

/* Render to a premultiplied 32bpp DIB (rounded corners stay transparent). */
static HBITMAP RenderDib(long long now, float scale, int *pw, int *ph, void **bits)
{
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32 } };
    int w = (int)ceilf(W_LOG * scale), h = (int)ceilf(H_LOG * scale);
    HBITMAP hb;
    GpBitmap *bm;
    GpGraphics *g;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    if (!(hb = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, bits, NULL, 0))) return NULL;
    GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, (BYTE *)*bits, &bm);
    GdipGetImageGraphicsContext((GpImage *)bm, &g);
    GdipScaleWorldTransform(g, scale, scale, MatrixOrderPrepend);
    DrawAll(g, now);
    GdipDeleteGraphics(g);
    GdipDisposeImage((GpImage *)bm);
    *pw = w; *ph = h;
    return hb;
}

static void Render(void)
{
    long long now = Now();
    int w = 0, h = 0, s = tl_display(&gT, now);
    void *bits;
    HBITMAP hb = RenderDib(now, gScale, &w, &h, &bits);
    HDC sdc, mdc;
    HGDIOBJ old;
    POINT src = { 0, 0 };
    SIZE sz = { w, h };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    wchar_t title[64];
    if (!hb) return;
    sdc = GetDC(NULL);
    mdc = CreateCompatibleDC(sdc);
    old = SelectObject(mdc, hb);
    UpdateLayeredWindow(gWnd, sdc, NULL, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
    SelectObject(mdc, old);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
    DeleteObject(hb);
    swprintf(title, 64, L"%d:%02d:%02d - " APPNAME, s / 3600, s / 60 % 60, s % 60);
    SetWindowTextW(gWnd, title);
}

/* ---------- behaviour ---------- */
static void Schedule(void)
{
    long long d = tl_next_change_ms(&gT, Now());
    KillTimer(gWnd, TM_TICK);
    if (d >= 0) SetTimer(gWnd, TM_TICK, (UINT)d + 2, NULL);
    if (gT.st == TL_ALARM) SetTimer(gWnd, TM_ALARM, 500, NULL);
    else KillTimer(gWnd, TM_ALARM);
}
static void SyncAlarm(void)
{
    FLASHWINFO fi = { sizeof fi, gWnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
    if (gT.st == TL_ALARM && !gRinging) {
        gRinging = gBlinkOn = 1;
        if (IsIconic(gWnd)) ShowWindow(gWnd, SW_SHOWNOACTIVATE);
        AlarmStart();
        FlashWindowEx(&fi);
    } else if (gT.st != TL_ALARM && gRinging) {
        gRinging = 0;
        AlarmStop();
        fi.dwFlags = FLASHW_STOP;
        FlashWindowEx(&fi);
    }
    Schedule();
}
static void Press(int id)
{
    long long now = Now();
    if (id == B_CLOSE) { PostMessageW(gWnd, WM_CLOSE, 0, 0); return; }
    if (id == B_PIN) {
        gTop = !gTop;
        SetWindowPos(gWnd, gTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return;
    }
    if (id <= 9) tl_digit(&gT, id, now);
    else if (id == B_ADD5) tl_add(&gT, 300, now);
    else if (id == B_ADD1) tl_add(&gT, 60, now);
    else if (id == B_ADD30) tl_add(&gT, 30, now);
    else if (id == B_BS) tl_backspace(&gT, now);
    else if (id == B_RESET) tl_reset(&gT, now);
    else if (id == B_START) tl_startstop(&gT, now);
    SyncAlarm();   /* silence a stopped alarm before the click */
    Click();
}
static int HitTest(int cx, int cy)
{
    float x = cx / gScale, y = cy / gScale;
    int i;
    for (i = 0; i < B_COUNT; i++)
        if (x >= gBtn[i].x && x < gBtn[i].x + gBtn[i].w && y >= gBtn[i].y && y < gBtn[i].y + gBtn[i].h) return i;
    return -1;
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    int id;
    switch (msg) {
    case WM_NCHITTEST: {   /* buttons are client area; everything else drags the window */
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(h, &pt);
        return HitTest(pt.x, pt.y) >= 0 ? HTCLIENT : HTCAPTION;
    }
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONUP:
        return 0;
    case WM_LBUTTONDOWN:   /* keys act on press, window buttons on release */
        if ((gPress = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp))) < 0) return 0;
        SetCapture(h);
        if (gPress < B_PIN) Press(gPress);
        Render();
        return 0;
    case WM_LBUTTONUP:
        id = gPress;
        gPress = -1;
        ReleaseCapture();
        if (id >= B_PIN && id == HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp))) Press(id);
        Render();
        return 0;
    case WM_MOUSEMOVE:
        if ((id = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp))) != gHover) {
            TRACKMOUSEEVENT te = { sizeof te, TME_LEAVE, h, 0 };
            TrackMouseEvent(&te);
            gHover = id;
            Render();
        }
        return 0;
    case WM_MOUSELEAVE:
    case WM_NCMOUSEMOVE:
        if (gHover >= 0) { gHover = -1; Render(); }
        break;
    case WM_TIMER:
        if (wp == TM_TICK) {
            tl_tick(&gT, Now());
        } else if (wp == TM_ALARM) {
            gBlinkOn = !gBlinkOn;
            if (Now() - gT.alarm_ms >= TL_RING_SEC * 1000LL) tl_stop_alarm(&gT);  /* back to the measured time */
        } else {
            KillTimer(h, TM_CLICKCLOSE);
            ClickClose();
            return 0;
        }
        SyncAlarm();
        Render();
        return 0;
    case MM_WOM_DONE:
        if ((HWAVEOUT)wp == gAWo) {
            AlarmFill((WAVEHDR *)lp);
            waveOutWrite(gAWo, (WAVEHDR *)lp, sizeof(WAVEHDR));
        }
        return 0;
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
    case WM_DESTROY:
        AlarmStop();
        ClickClose();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

#ifdef SHOTS
/* Developer build: KeypadTimer-shots.exe --shot out.png <state> renders a state to PNG. */
static int Shot(const wchar_t *file, const wchar_t *what)
{
    static const CLSID png = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    long long t0 = 1000000;
    int w, h, i;
    void *bits;
    HBITMAP hb;
    GpBitmap *bm;
    if (!wcscmp(what, L"pinned")) gTop = 1;
    if (!wcscmp(what, L"stopwatch")) { tl_startstop(&gT, t0); t0 += 83400; }   /* 0:01:23 */
    if (!wcscmp(what, L"entered") || !wcscmp(what, L"bs")) for (i = 0; i < 3; i++) tl_digit(&gT, L"130"[i] - L'0', 0);
    if (!wcscmp(what, L"bs")) { tl_backspace(&gT, 0); gPress = B_BS; }
    if (!wcscmp(what, L"running") || !wcscmp(what, L"paused") || !wcscmp(what, L"alarm")) {
        tl_digit(&gT, 2, 0); tl_digit(&gT, 5, 0);
        tl_startstop(&gT, t0);
        t0 += 746000;   /* 12:34 left */
        if (!wcscmp(what, L"paused")) tl_startstop(&gT, t0);
        if (!wcscmp(what, L"alarm")) tl_tick(&gT, t0 += 754000);
    }
    hb = RenderDib(t0, 2, &w, &h, &bits);
    GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, (BYTE *)bits, &bm);
    GdipSaveImageToFile((GpImage *)bm, file, &png, NULL);
    GdipDisposeImage((GpImage *)bm);
    DeleteObject(hb);
    return 0;
}
#endif

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show)
{
    WNDCLASSEXW wc = { sizeof wc };
    GdiplusStartupInput gin = { 1, NULL, FALSE, FALSE };
    ULONG_PTR tok;
    RECT wa;
    MSG m;
    int n;
    (void)hp; (void)cmd;
    Layout();
    GdiplusStartup(&tok, &gin, NULL);
    InitGraphics();
    tl_init(&gT, 0);
#ifdef SHOTS
    if (__argc >= 4 && !wcscmp(__wargv[1], L"--shot")) return Shot(__wargv[2], __wargv[3]);
#endif
    for (n = 0; n < CLICK; n++) gCBuf[n] = tl_click_sample(n, CLICK);

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"zmlworks.KeypadTimer";
    RegisterClassExW(&wc);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    gWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_APPWINDOW | WS_EX_NOACTIVATE,
                           wc.lpszClassName, APPNAME, WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
                           0, 0, W_LOG, H_LOG, NULL, NULL, hi, NULL);
    if (!gWnd) return 1;
    gScale = GetDpiForWindow(gWnd) / 96.0f;   /* top-right of the work area */
    SetWindowPos(gWnd, NULL, wa.right - (int)ceilf(W_LOG * gScale) - 24, wa.top + 24,
                 (int)ceilf(W_LOG * gScale), (int)ceilf(H_LOG * gScale), SWP_NOZORDER | SWP_NOACTIVATE);
    Render();
    ShowWindow(gWnd, show == SW_SHOWMINIMIZED || show == SW_MINIMIZE ? show : SW_SHOWNOACTIVATE);
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (m.message >= WM_KEYFIRST && m.message <= WM_KEYLAST && m.hwnd == gWnd) continue;  /* no keyboard */
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    GdiplusShutdown(tok);
    return 0;
}
