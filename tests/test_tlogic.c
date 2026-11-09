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
    EQ(tl_next_change_ms(&t, 1000), 500); EQ(tl_next_change_ms(&t, 1250), 250); EQ(tl_next_change_ms(&t, 1600), 400);
    EQ(tl_colon_on(&t, 1000), 1); EQ(tl_colon_on(&t, 1499), 1); EQ(tl_colon_on(&t, 1500), 0); EQ(tl_colon_on(&t, 2000), 1);
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
    /* stopwatch: START with 0:00:00 shown counts up */
    tl_init(&t, 180);  tl_reset(&t, 0); EQ(t.disp, 0);
    EQ(tl_startstop(&t, 1000), 1); EQ(t.st, TL_RUN); EQ(t.sw, 1);
    EQ(tl_display(&t, 1000), 0); EQ(tl_display(&t, 1999), 0); EQ(tl_display(&t, 2000), 1); EQ(tl_display(&t, 61000 + 999), 60);
    EQ(tl_colon_on(&t, 1000), 1); EQ(tl_colon_on(&t, 1500), 0); EQ(tl_next_change_ms(&t, 1200), 300);
    EQ(tl_tick(&t, 999999999), 0); EQ(t.st, TL_RUN);                     /* never rings */
    keys(&t, "5"); tl_add(&t, 60, 5000); tl_backspace(&t, 5000); EQ(tl_display(&t, 5000), 4); /* ignored */
    tl_startstop(&t, 5500); EQ(t.st, TL_PAUSE); EQ(tl_display(&t, 99999), 4); EQ(tl_colon_on(&t, 5800), 1);
    tl_add(&t, 300, 0); EQ(tl_display(&t, 0), 4);
    tl_startstop(&t, 10000); EQ(tl_display(&t, 10500), 5);               /* resumes from 4.5 s */
    tl_reset(&t, 11000); EQ(t.st, TL_IDLE); EQ(t.disp, 0); EQ(t.sw, 0);
    tl_reset(&t, 0); EQ(t.disp, 0);                                       /* stays 0 */
    keys(&t, "3"); EQ(t.disp, 180); tl_startstop(&t, 0); EQ(t.sw, 0);    /* normal timer again */
    tl_init(&t, 0); tl_startstop(&t, 0);
    EQ(tl_display(&t, 400000000LL), TL_MAX_SEC); EQ(tl_next_change_ms(&t, 400000000LL), -1); /* capped */
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
    /* backspace */
    tl_init(&t, 0); keys(&t, "130"); EQ(t.disp, 5400);
    tl_backspace(&t, 0); EQ(t.disp, 13 * 60);
    tl_backspace(&t, 0); EQ(t.disp, 60);
    tl_backspace(&t, 0); EQ(t.disp, 0);
    EQ(tl_backspace(&t, 0), 0); EQ(t.disp, 0);
    keys(&t, "5"); EQ(t.disp, 300);                        /* fresh after emptied */
    tl_init(&t, 0); keys(&t, "13"); tl_backspace(&t, 0); keys(&t, "5"); EQ(t.disp, 15 * 60); /* append after BS */
    tl_init(&t, 5400); tl_backspace(&t, 0); EQ(t.disp, 13 * 60); /* recalled 1:30:00 -> 13 */
    keys(&t, "0"); EQ(t.disp, 5400);                        /* 13 -> 130 again */
    tl_init(&t, 0); keys(&t, "5"); tl_add(&t, 30, 0); tl_backspace(&t, 0); EQ(t.disp, 0); /* 5:30 -> drops secs, 5 -> 0 */
    tl_init(&t, 0); keys(&t, "1"); tl_startstop(&t, 0);
    EQ(tl_backspace(&t, 1000), 0); EQ(t.st, TL_RUN);       /* ignored while running */
    tl_startstop(&t, 2000); EQ(tl_backspace(&t, 0), 0); EQ(t.st, TL_PAUSE);
    tl_startstop(&t, 2000); tl_tick(&t, 70000); EQ(t.st, TL_ALARM);
    tl_backspace(&t, 0); EQ(t.st, TL_IDLE); EQ(t.disp, 60); /* stops the alarm only */
    /* fade curve */
    n++; if (fabs(tl_alarm_gain(0, 30) - pow(10, -TL_FADE_DB / 20)) > 1e-9) { fails++; puts("FAIL gain0"); }
    n++; if (fabs(tl_alarm_gain(30, 30) - 1) > 1e-9 || fabs(tl_alarm_gain(500, 30) - 1) > 1e-9) { fails++; puts("FAIL gainmax"); }
    n++; if (fabs(tl_alarm_gain(15, 30) - pow(10, -TL_FADE_DB / 40)) > 1e-9) { fails++; puts("FAIL gainmid"); }
    n++; { double p = 0; int ok = 1; for (double s = 0; s <= 30; s += 0.5) { double g = tl_alarm_gain(s, 30); if (g < p) ok = 0; p = g; } if (!ok) { fails++; puts("FAIL monotonic"); } }
    n++; if (tl_alarm_gain(0, 0) != 1.0) { fails++; puts("FAIL nofade"); }
    /* sounds: click peak == alarm peak at full; quiet start; stops 2 min after ringing starts */
    {
        int pk_click = 0, pk_a0 = 0, pk_full = 0, pk_last = 0, nz_end = 0, v;
        long i, len = TL_SR * 45 / 1000;
        for (i = 0; i < len; i++) if ((v = abs(tl_click_sample(i, len))) > pk_click) pk_click = v;
        for (i = 0; i < TL_SR; i++) {
            if ((v = abs(tl_alarm_sample(i))) > pk_a0) pk_a0 = v;
            if ((v = abs(tl_alarm_sample(40LL * TL_SR + i))) > pk_full) pk_full = v;
            if ((v = abs(tl_alarm_sample(119LL * TL_SR + i))) > pk_last) pk_last = v;
            nz_end |= tl_alarm_sample(120LL * TL_SR + i) != 0;
        }
        printf("peaks: click %d, alarm@0s %d, @40s %d, @119s %d\n", pk_click, pk_a0, pk_full, pk_last);
        n++; if (abs(pk_click - pk_full) > pk_full / 50) { fails++; puts("FAIL click != alarm full"); }
        n++; if (abs(pk_full - (int)(TL_AMP * 32767)) > 400) { fails++; puts("FAIL level != 30 %"); }
        n++; if (pk_a0 * 40 > pk_full) { fails++; puts("FAIL fade start too loud"); }
        n++; if (pk_last < pk_full * 0.98) { fails++; puts("FAIL not full before end"); }
        n++; if (nz_end) { fails++; puts("FAIL alarm did not stop at 2:00"); }
    }
    printf("%d checks, %d failures\n", n, fails);
    return fails != 0;
}
