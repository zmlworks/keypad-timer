#include "../src/tlogic.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
static int fails, n;
#define EQ(a, b) do { long long _a = (a), _b = (b); n++; if (_a != _b) { fails++; \
    printf("FAIL %s:%d %s = %lld, want %lld\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)
static void keys(Timer *t, const char *s) { for (; *s; s++) tl_digit(t, *s - '0', 0); }

int main(void)
{
    Timer t;
    /* parsing */
    EQ(tl_entry_to_sec(5), 300);
    EQ(tl_entry_to_sec(30), 1800);
    EQ(tl_entry_to_sec(130), 5400);
    EQ(tl_entry_to_sec(90), 5400);
    EQ(tl_entry_to_sec(9999), TL_MAX_SEC);
    tl_init(&t, 0); keys(&t, "5"); EQ(t.disp, 300);
    tl_init(&t, 0); keys(&t, "30"); EQ(t.disp, 1800);
    tl_init(&t, 0); keys(&t, "130"); EQ(t.disp, 5400);
    tl_init(&t, 0); keys(&t, "012"); EQ(t.disp, 720);
    tl_init(&t, 0); keys(&t, "12345"); EQ(t.disp, 23 * 3600 + 45 * 60); /* oldest digit drops */
    /* start / alarm / remembered */
    tl_init(&t, 0); keys(&t, "3");
    EQ(tl_startstop(&t, 1000), 1); EQ(t.st, TL_RUN); EQ(t.remembered, 180);
    EQ(tl_display(&t, 1000), 180); EQ(tl_display(&t, 1001), 180); EQ(tl_display(&t, 2000), 179);
    EQ(tl_next_change_ms(&t, 1000), 1000); EQ(tl_next_change_ms(&t, 1250), 750);
    EQ(tl_tick(&t, 180999), 0); EQ(tl_display(&t, 180999), 1);
    EQ(tl_tick(&t, 181000), 1); EQ(t.st, TL_ALARM); EQ(tl_display(&t, 181000), 0);
    EQ(tl_startstop(&t, 182000), 1); EQ(t.st, TL_IDLE); EQ(t.disp, 180);
    /* one press restarts the same time */
    EQ(tl_startstop(&t, 200000), 1); EQ(t.st, TL_RUN); EQ(tl_display(&t, 200000), 180);
    /* reset while running -> remembered, again -> 0, again -> remembered */
    tl_reset(&t, 210000); EQ(t.st, TL_IDLE); EQ(t.disp, 180);
    tl_reset(&t, 0); EQ(t.disp, 0);
    tl_reset(&t, 0); EQ(t.disp, 180);
    /* digit after reset starts fresh */
    keys(&t, "1"); EQ(t.disp, 60); keys(&t, "0"); EQ(t.disp, 600);
    /* reset while typing returns to remembered */
    tl_reset(&t, 0); EQ(t.disp, 180);
    /* start with 0 shown is ignored */
    tl_init(&t, 0); EQ(tl_startstop(&t, 0), 0); EQ(t.st, TL_IDLE);
    /* pause/resume */
    tl_init(&t, 0); keys(&t, "1"); tl_startstop(&t, 0);
    tl_startstop(&t, 10500); EQ(t.st, TL_PAUSE); EQ(tl_display(&t, 99999), 50);
    keys(&t, "5"); EQ(tl_display(&t, 0), 50);              /* digits ignored while paused */
    tl_startstop(&t, 20000); EQ(t.st, TL_RUN); EQ(tl_display(&t, 20000), 50);
    tl_reset(&t, 21000); EQ(t.disp, 60);
    /* add buttons */
    tl_init(&t, 0); keys(&t, "5"); tl_add(&t, 30, 0); EQ(t.disp, 330);
    keys(&t, "2"); EQ(t.disp, 120);                       /* digit after add = fresh */
    tl_init(&t, 0); tl_add(&t, 300, 0); tl_add(&t, 60, 0); tl_add(&t, 30, 0); EQ(t.disp, 390);
    tl_startstop(&t, 0); EQ(t.remembered, 390);
    tl_add(&t, 60, 100000); EQ(tl_display(&t, 100000), 390 - 100 + 60);
    EQ(t.remembered, 390);                                /* remembered = the started time */
    tl_startstop(&t, 100000); tl_add(&t, 30, 0); EQ(tl_display(&t, 0), 380);
    tl_init(&t, 0); keys(&t, "9959"); tl_add(&t, 300, 0); EQ(t.disp, TL_MAX_SEC);
    /* any button during alarm only stops it */
    tl_init(&t, 0); keys(&t, "1"); tl_startstop(&t, 0); tl_tick(&t, 60000);
    EQ(t.st, TL_ALARM); tl_digit(&t, 7, 0); EQ(t.st, TL_IDLE); EQ(t.disp, 60);
    tl_startstop(&t, 0); tl_tick(&t, 60000); tl_add(&t, 60, 0); EQ(t.st, TL_IDLE); EQ(t.disp, 60);
    tl_startstop(&t, 0); tl_tick(&t, 60000); tl_reset(&t, 0); EQ(t.st, TL_IDLE); EQ(t.disp, 60);
    /* fade curve */
    n++; if (fabs(tl_alarm_gain(0, 30) - pow(10, -TL_FADE_DB / 20)) > 1e-9) { fails++; puts("FAIL gain0"); }
    n++; if (fabs(tl_alarm_gain(30, 30) - 1) > 1e-9 || fabs(tl_alarm_gain(500, 30) - 1) > 1e-9) { fails++; puts("FAIL gainmax"); }
    n++; if (fabs(tl_alarm_gain(15, 30) - pow(10, -TL_FADE_DB / 40)) > 1e-9) { fails++; puts("FAIL gainmid"); }
    n++; { double p = 0; int ok = 1; for (double s = 0; s <= 30; s += 0.5) { double g = tl_alarm_gain(s, 30); if (g < p) ok = 0; p = g; } if (!ok) { fails++; puts("FAIL monotonic"); } }
    n++; if (tl_alarm_gain(0, 0) != 1.0) { fails++; puts("FAIL nofade"); }
    n++; if (tl_volume_amp(0) != 0 || tl_volume_amp(100) != 1.0 || tl_volume_amp(50) != 0.25) { fails++; puts("FAIL vol"); }
    /* sounds: click peak == alarm peak at max volume; fade start is quiet; auto-stop */
    {
        int pk_click = 0, pk_a0 = 0, pk_amax = 0, pk_end = 0, nz_end = 0;
        long i, len = TL_SR * 45 / 1000;
        FILE *f;
        for (i = 0; i < len; i++) { int v = abs(tl_shot_sample(i, len, 60, 0)); if (v > pk_click) pk_click = v; }
        for (i = 0; i < TL_SR; i++) { int v = abs(tl_alarm_sample(i, 60, 30, 3)); if (v > pk_a0) pk_a0 = v; }
        for (i = 0; i < TL_SR; i++) { int v = abs(tl_alarm_sample(40LL * TL_SR + i, 60, 30, 3)); if (v > pk_amax) pk_amax = v; }
        for (i = 0; i < TL_SR; i++) { int v = abs(tl_alarm_sample(209LL * TL_SR + i, 60, 30, 3)); if (v > pk_end) pk_end = v; }
        for (i = 0; i < TL_SR; i++) nz_end |= tl_alarm_sample(210LL * TL_SR + i, 60, 30, 3) != 0;
        printf("peaks: click %d, alarm@0s %d, alarm@40s %d, alarm@209s %d\n", pk_click, pk_a0, pk_amax, pk_end);
        n++; if (abs(pk_click - pk_amax) > pk_amax / 50) { fails++; puts("FAIL click != alarm max"); }
        n++; if (pk_a0 * 40 > pk_amax) { fails++; puts("FAIL fade start too loud"); }
        n++; if (pk_end < pk_amax * 0.98) { fails++; puts("FAIL not at max before stop"); }
        n++; if (nz_end) { fails++; puts("FAIL alarm did not stop at fade+3min"); }
        n++; if (tl_shot_sample(100, len, 0, 0) != 0) { fails++; puts("FAIL vol0"); }
        /* write a preview WAV of the first 40 s for listening */
        if ((f = fopen("alarm-preview.wav", "wb"))) {
            unsigned ns = 40 * TL_SR, db = ns * 2, v32; unsigned short v16;
            fwrite("RIFF", 1, 4, f); v32 = 36 + db; fwrite(&v32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
            v32 = 16; fwrite(&v32, 4, 1, f); v16 = 1; fwrite(&v16, 2, 1, f); fwrite(&v16, 2, 1, f);
            v32 = TL_SR; fwrite(&v32, 4, 1, f); v32 = TL_SR * 2; fwrite(&v32, 4, 1, f);
            v16 = 2; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
            fwrite("data", 1, 4, f); fwrite(&db, 4, 1, f);
            for (i = 0; i < (long)ns; i++) { short s = tl_alarm_sample(i, 60, 30, 3); fwrite(&s, 2, 1, f); }
            fclose(f);
        }
    }
    printf("%d checks, %d failures\n", n, fails);
    return fails != 0;
}
