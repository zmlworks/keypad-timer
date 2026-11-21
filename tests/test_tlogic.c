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
    /* entry stays un-normalized on the display until start (90 -> 0:90:00, then 1:30:00) */
    {
        int d[6];
        tl_init(&t, 0); keys(&t, "90");
        EQ(t.disp, 5400);   /* duration is already 90 min, but the fields stay 0:90:00 */
        tl_digits(&t, 0, d);
        EQ(d[0], -1); EQ(d[1], 0); EQ(d[2], 9); EQ(d[3], 0); EQ(d[4], 0); EQ(d[5], 0);
        EQ(tl_startstop(&t, 0), 1);
        EQ(t.st, TL_RUN); EQ(t.remembered, 5400); EQ(tl_display(&t, 0), 5400);
        tl_digits(&t, 0, d);
        EQ(d[0], -1); EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 0); EQ(d[4], 0); EQ(d[5], 0);
        /* after alarm / stop, restore the normalized duration, not 0:90:00 */
        EQ(tl_tick(&t, 5400 * 1000LL), 1); EQ(t.st, TL_ALARM);
        EQ(tl_startstop(&t, 5400 * 1000LL + 5000), 1); EQ(t.st, TL_IDLE); EQ(t.disp, 5400);
        tl_digits(&t, 0, d);
        EQ(d[0], -1); EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 0); EQ(d[4], 0); EQ(d[5], 0);
        /* typing 90 again when remembered is already 1:30:00: reset restores 1:30:00, not 0 */
        keys(&t, "90");
        tl_digits(&t, 0, d); EQ(d[2], 9); EQ(d[3], 0);
        tl_reset(&t, 0); EQ(t.disp, 5400);
        tl_digits(&t, 0, d);
        EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 0);
        tl_reset(&t, 0); EQ(t.disp, 0);   /* second reset still clears */
    }
    /* 160 stays 1:60:00 until start, then 2:00:00 */
    {
        int d[6];
        tl_init(&t, 0); keys(&t, "160");
        EQ(t.disp, 7200);
        tl_digits(&t, 0, d);
        EQ(d[0], -1); EQ(d[1], 1); EQ(d[2], 6); EQ(d[3], 0); EQ(d[4], 0); EQ(d[5], 0);
        tl_startstop(&t, 0); EQ(t.remembered, 7200); EQ(tl_display(&t, 0), 7200);
        tl_digits(&t, 0, d);
        EQ(d[1], 2); EQ(d[2], 0); EQ(d[3], 0);
    }
    /* 99 stays 0:99:00 until start -> 1:39:00 */
    {
        int d[6];
        tl_init(&t, 0); keys(&t, "99");
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 9); EQ(d[3], 9);
        tl_startstop(&t, 0); EQ(t.remembered, 99 * 60);
        tl_digits(&t, 0, d);
        EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 9);
    }
    /* add 1 min / 30 s to an un-normalized entry; seconds carry at 60 immediately, minutes wait for start */
    {
        int d[6], i;
        tl_init(&t, 0); keys(&t, "90"); tl_add(&t, 60, 0);   /* 0:91:00 */
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 9); EQ(d[3], 1); EQ(d[4], 0); EQ(d[5], 0);
        EQ(t.disp, 91 * 60);
        tl_startstop(&t, 0); EQ(t.remembered, 91 * 60);
        tl_digits(&t, 0, d);
        EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 1);

        tl_init(&t, 0); keys(&t, "90"); tl_add(&t, 30, 0);   /* 0:90:30 */
        tl_digits(&t, 0, d);
        EQ(d[2], 9); EQ(d[3], 0); EQ(d[4], 3); EQ(d[5], 0);
        tl_startstop(&t, 0); EQ(t.remembered, 90 * 60 + 30);
        tl_digits(&t, 0, d);
        EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 0); EQ(d[4], 3); EQ(d[5], 0);

        tl_init(&t, 0);
        for (i = 0; i < 5; i++) tl_add(&t, 10, 0);          /* 0:00:50 */
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 0); EQ(d[3], 0); EQ(d[4], 5); EQ(d[5], 0);
        EQ(t.disp, 50);
        tl_add(&t, 10, 0);                                   /* carry at 60 immediately: 0:01:00 */
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 0); EQ(d[3], 1); EQ(d[4], 0); EQ(d[5], 0);
        EQ(t.disp, 60);
        tl_startstop(&t, 0); EQ(t.remembered, 60); EQ(tl_display(&t, 0), 60);
        tl_digits(&t, 0, d);
        EQ(d[3], 1); EQ(d[4], 0); EQ(d[5], 0);

        tl_init(&t, 0);
        tl_add(&t, 30, 0); tl_add(&t, 30, 0);               /* 30秒 twice -> 0:01:00, not 0:00:60 */
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 0); EQ(d[3], 1); EQ(d[4], 0); EQ(d[5], 0);
        EQ(t.disp, 60);

        tl_init(&t, 0);
        for (i = 0; i < 59; i++) tl_add(&t, 60, 0);
        tl_add(&t, 30, 0); tl_add(&t, 30, 0);               /* 0:59:00 + 30 + 30 -> 0:60:00 until start */
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 6); EQ(d[3], 0); EQ(d[4], 0); EQ(d[5], 0);
        EQ(t.disp, 3600);
        tl_startstop(&t, 0); EQ(t.remembered, 3600);
        tl_digits(&t, 0, d);
        EQ(d[1], 1); EQ(d[2], 0); EQ(d[3], 0);

        /* reset while running after a 90 start restores 1:30:00 */
        tl_init(&t, 0); keys(&t, "90"); tl_startstop(&t, 0);
        tl_reset(&t, 1000); EQ(t.st, TL_IDLE); EQ(t.disp, 5400);
        tl_digits(&t, 0, d);
        EQ(d[1], 1); EQ(d[2], 3); EQ(d[3], 0);

        /* backspace on 0:90:00 deletes the last typed digit: 90 -> 9 */
        tl_init(&t, 0); keys(&t, "90"); tl_backspace(&t, 0);
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 0); EQ(d[3], 9); EQ(t.disp, 9 * 60);
    }
    /* start / alarm / remembered */
    tl_init(&t, 0); keys(&t, "3");
    EQ(tl_startstop(&t, 1000), 1); EQ(t.st, TL_RUN); EQ(t.remembered, 180);
    EQ(tl_display(&t, 1000), 180); EQ(tl_display(&t, 1001), 180); EQ(tl_display(&t, 2000), 179);
    EQ(tl_next_change_ms(&t, 1000), 500); EQ(tl_next_change_ms(&t, 1250), 250); EQ(tl_next_change_ms(&t, 1600), 400);
    EQ(tl_colon_on(&t, 1000), 1); EQ(tl_colon_on(&t, 1499), 1); EQ(tl_colon_on(&t, 1500), 0); EQ(tl_colon_on(&t, 2000), 1);
    EQ(tl_tick(&t, 180999), 0); EQ(tl_display(&t, 180999), 1);
    EQ(tl_tick(&t, 181000), 1); EQ(t.st, TL_ALARM); EQ(tl_display(&t, 181000), 0);
    EQ(tl_ringing(&t, 181000), 1); EQ(tl_accent(&t, 181000), 0);          /* 0:00:00, no accent yet */
    /* blink 1 s cycle (500 ms lit / 500 ms off) while the count keeps advancing */
    EQ(tl_alarm_lit(&t, 181000), 1); EQ(tl_alarm_lit(&t, 181499), 1); EQ(tl_alarm_lit(&t, 181500), 0);
    EQ(tl_display(&t, 181500), 0);                                       /* off phase still holds 0, not frozen later */
    EQ(tl_display(&t, 181999), 0); EQ(tl_display(&t, 182000), 1); EQ(tl_accent(&t, 182000), 2);
    EQ(tl_alarm_lit(&t, 182000), 1); EQ(tl_alarm_lit(&t, 182499), 1); EQ(tl_alarm_lit(&t, 182500), 0);
    EQ(tl_display(&t, 182500), 1); EQ(tl_display(&t, 183000), 2);         /* next second is visible on the lit half */
    EQ(tl_next_change_ms(&t, 182000), 500); EQ(tl_next_change_ms(&t, 182200), 300);
    EQ(tl_colon_on(&t, 181000), 1); EQ(tl_colon_on(&t, 240999), 1);       /* solid colon while ringing */
    EQ(tl_ringing(&t, 181000 + 120000 - 1), 1); EQ(tl_ringing(&t, 181000 + 120000), 0);
    EQ(tl_alarm_lit(&t, 181000 + 120000), 1);                            /* always lit once the sound has stopped */
    EQ(tl_display(&t, 181000 + 120000), 120);                            /* still counting after the ring */
    EQ(tl_display(&t, 181000 + 185000), 185);                            /* and past 3 min, like a stopwatch */
    EQ(tl_colon_on(&t, 181000 + 120000), 1); EQ(tl_colon_on(&t, 181000 + 120500), 0);
    EQ(tl_next_change_ms(&t, 181000 + 120200), 300);
    EQ(tl_startstop(&t, 181000 + 185000), 1); EQ(t.st, TL_IDLE); EQ(t.disp, 180); /* stop restores the duration */
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
    keys(&t, "2"); EQ(t.disp, 150);                       /* fresh H:M, seconds kept: 0:02:30 */
    {
        int d[6];
        tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 0); EQ(d[3], 2); EQ(d[4], 3); EQ(d[5], 0);
        /* 0:05:20 then 1 -> 0:01:20, not 0:01:00 */
        tl_init(&t, 0); keys(&t, "5"); tl_add(&t, 10, 0); tl_add(&t, 10, 0);
        EQ(t.disp, 320); tl_digits(&t, 0, d);
        EQ(d[3], 5); EQ(d[4], 2); EQ(d[5], 0);
        keys(&t, "1"); EQ(t.disp, 80); tl_digits(&t, 0, d);
        EQ(d[1], 0); EQ(d[2], 0); EQ(d[3], 1); EQ(d[4], 2); EQ(d[5], 0);
        keys(&t, "3"); EQ(t.disp, 800); tl_digits(&t, 0, d); /* append H:M, still keep seconds: 0:13:20 */
        EQ(d[2], 1); EQ(d[3], 3); EQ(d[4], 2); EQ(d[5], 0);
        /* no seconds, ndig==0 (recalled time): digit still starts a fresh H:M, seconds stay 0 */
        tl_init(&t, 300); keys(&t, "2"); EQ(t.disp, 120);
        tl_digits(&t, 0, d); EQ(d[3], 2); EQ(d[4], 0); EQ(d[5], 0);
    }
    tl_init(&t, 0); tl_add(&t, 300, 0); tl_add(&t, 60, 0); tl_add(&t, 30, 0); EQ(t.disp, 390);
    tl_startstop(&t, 0); EQ(t.remembered, 390);
    tl_add(&t, 60, 100000); EQ(tl_display(&t, 100000), 390 - 100 + 60);
    EQ(t.remembered, 390);                                /* remembered = the started time */
    tl_startstop(&t, 100000); tl_add(&t, 30, 0); EQ(tl_display(&t, 0), 380);
    tl_init(&t, 0); tl_add(&t, 60, 0); tl_add(&t, 30, 0); tl_add(&t, 10, 0); EQ(t.disp, 100);
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
    tl_init(&t, 0); keys(&t, "5"); tl_add(&t, 30, 0); tl_backspace(&t, 0); EQ(t.disp, 300); /* 0:05:30 -> 0:05:00 */
    tl_backspace(&t, 0); EQ(t.disp, 0);                    /* then H/M digits */
    /* seconds BS: >30 subtract 30, <=30 clear to 0 */
    {
        int d[6], i;
        tl_init(&t, 0);
        for (i = 0; i < 5; i++) tl_add(&t, 10, 0);          /* 0:00:50 */
        tl_backspace(&t, 0); tl_digits(&t, 0, d);
        EQ(d[4], 2); EQ(d[5], 0); EQ(t.disp, 20);           /* 50 -> 20 */
        tl_backspace(&t, 0); tl_digits(&t, 0, d);
        EQ(d[4], 0); EQ(d[5], 0); EQ(t.disp, 0);            /* 20 -> 0 */

        tl_init(&t, 0); tl_add(&t, 10, 0); tl_add(&t, 10, 0); tl_add(&t, 10, 0); tl_add(&t, 10, 0); /* 40 */
        tl_backspace(&t, 0); EQ(t.disp, 10);                /* 40 -> 10 */
        tl_backspace(&t, 0); EQ(t.disp, 0);                 /* 10 -> 0 */

        tl_init(&t, 0); tl_add(&t, 30, 0);
        tl_backspace(&t, 0); EQ(t.disp, 0);                 /* exactly 30 -> 0 */

        tl_init(&t, 0); keys(&t, "90"); tl_add(&t, 30, 0);  /* 0:90:30 */
        tl_backspace(&t, 0); tl_digits(&t, 0, d);
        EQ(d[2], 9); EQ(d[3], 0); EQ(d[4], 0); EQ(d[5], 0); EQ(t.disp, 90 * 60);
        tl_backspace(&t, 0); tl_digits(&t, 0, d);           /* seconds already 0: delete last H/M digit */
        EQ(d[2], 0); EQ(d[3], 9); EQ(t.disp, 9 * 60);
    }
    tl_init(&t, 0); keys(&t, "1"); tl_startstop(&t, 0);
    EQ(tl_backspace(&t, 1000), 0); EQ(t.st, TL_RUN);       /* ignored while running */
    tl_startstop(&t, 2000); EQ(tl_backspace(&t, 0), 0); EQ(t.st, TL_PAUSE);
    tl_startstop(&t, 2000); tl_tick(&t, 70000); EQ(t.st, TL_ALARM);
    tl_backspace(&t, 0); EQ(t.st, TL_IDLE); EQ(t.disp, 60); /* stops the alarm only */
    /* significant digits: 5 min is 0:05:00, only "5:00" is significant (index 3..) */
    EQ(tl_sig_from(0), 6);
    EQ(tl_sig_from(5), 5); EQ(tl_sig_from(10), 4); EQ(tl_sig_from(60), 3);
    EQ(tl_sig_from(300), 3); EQ(tl_sig_from(600), 2);
    EQ(tl_sig_from(3600), 1); EQ(tl_sig_from(36000), 0);
    /* setting: only non-zero digits (5 min -> only the 5; 1:30:00 -> 1 and 3) */
    EQ(tl_digit_nz(300, 0), 0); EQ(tl_digit_nz(300, 1), 0); EQ(tl_digit_nz(300, 2), 0);
    EQ(tl_digit_nz(300, 3), 1); EQ(tl_digit_nz(300, 4), 0); EQ(tl_digit_nz(300, 5), 0);
    EQ(tl_digit_nz(5400, 1), 1); EQ(tl_digit_nz(5400, 2), 1); EQ(tl_digit_nz(5400, 3), 0);
    EQ(tl_digit_nz(5400, 0), 0); EQ(tl_digit_nz(0, 5), 0);
    /* setting red: trailing 0 of seconds is also red when seconds are present */
    tl_init(&t, 0); keys(&t, "5");
    EQ(tl_setting_red(&t, 0), 0); EQ(tl_setting_red(&t, 1), 0); EQ(tl_setting_red(&t, 2), 0);
    EQ(tl_setting_red(&t, 3), 1); EQ(tl_setting_red(&t, 4), 0); EQ(tl_setting_red(&t, 5), 0);
    tl_init(&t, 0); keys(&t, "30");                          /* 0:30:00: tens set, ones 0 also red */
    EQ(tl_setting_red(&t, 1), 0); EQ(tl_setting_red(&t, 2), 1); EQ(tl_setting_red(&t, 3), 1);
    EQ(tl_setting_red(&t, 4), 0); EQ(tl_setting_red(&t, 5), 0);
    tl_init(&t, 0); keys(&t, "130");                         /* 1:30:00: 1, 3 and the minutes 0 */
    EQ(tl_setting_red(&t, 1), 1); EQ(tl_setting_red(&t, 2), 1); EQ(tl_setting_red(&t, 3), 1);
    EQ(tl_setting_red(&t, 4), 0); EQ(tl_setting_red(&t, 5), 0);
    tl_init(&t, 0); keys(&t, "100");                         /* 1:00:00: hours only, minutes zeros stay normal */
    EQ(tl_setting_red(&t, 1), 1); EQ(tl_setting_red(&t, 2), 0); EQ(tl_setting_red(&t, 3), 0);
    EQ(tl_setting_red(&t, 4), 0); EQ(tl_setting_red(&t, 5), 0);
    tl_init(&t, 0); keys(&t, "105");                         /* 1:05:00: tens of minutes not set, its 0 stays normal */
    EQ(tl_setting_red(&t, 1), 1); EQ(tl_setting_red(&t, 2), 0); EQ(tl_setting_red(&t, 3), 1);
    tl_init(&t, 0); tl_add(&t, 30, 0);                       /* 0:00:30: both 3 and 0 */
    EQ(tl_setting_red(&t, 3), 0); EQ(tl_setting_red(&t, 4), 1); EQ(tl_setting_red(&t, 5), 1);
    tl_init(&t, 0); keys(&t, "5"); tl_add(&t, 30, 0);         /* 0:05:30: 5, 3 and 0 */
    EQ(tl_setting_red(&t, 3), 1); EQ(tl_setting_red(&t, 4), 1); EQ(tl_setting_red(&t, 5), 1);
    tl_init(&t, 0); keys(&t, "130"); tl_add(&t, 10, 0); tl_add(&t, 10, 0); /* 1:30:20 */
    EQ(tl_setting_red(&t, 1), 1); EQ(tl_setting_red(&t, 2), 1); EQ(tl_setting_red(&t, 3), 1);
    EQ(tl_setting_red(&t, 4), 1); EQ(tl_setting_red(&t, 5), 1);
    tl_init(&t, 0); EQ(tl_setting_red(&t, 5), 0);             /* 0:00:00, nothing red */
    tl_init(&t, 0); EQ(tl_accent(&t, 0), 0);
    keys(&t, "5"); EQ(t.disp, 300); EQ(tl_accent(&t, 0), 1);
    tl_startstop(&t, 0); EQ(tl_accent(&t, 0), 0);            /* countdown: all normal ink */
    EQ(tl_setting_red(&t, 3), 0);
    tl_startstop(&t, 1000); EQ(t.st, TL_PAUSE); EQ(tl_accent(&t, 0), 0);
    tl_startstop(&t, 1000);
    tl_tick(&t, 300000); EQ(t.st, TL_ALARM);
    EQ(tl_accent(&t, 300000), 0); EQ(tl_accent(&t, 305000), 2);
    EQ(tl_silence(&t, 305000), 1); EQ(t.st, TL_ALARM); EQ(tl_ringing(&t, 305000), 0);
    EQ(tl_display(&t, 310000), 10); EQ(tl_accent(&t, 310000), 2);  /* silenced, still counting */
    EQ(tl_silence(&t, 310000), 0);                                 /* already quiet */
    EQ(tl_alarm_lit(&t, 310000), 1);
    tl_reset(&t, 310000); EQ(t.st, TL_IDLE); EQ(t.disp, 300); EQ(tl_accent(&t, 0), 1);
    /* silence after the 2 min auto-stop does nothing */
    tl_startstop(&t, 0); tl_tick(&t, 300000);
    EQ(tl_silence(&t, 300000 + 120000), 0); EQ(t.st, TL_ALARM); EQ(t.muted, 0);
    tl_init(&t, 0); tl_startstop(&t, 0); EQ(t.sw, 1);
    EQ(tl_accent(&t, 0), 0); EQ(tl_accent(&t, 2000), 2);     /* stopwatch is blue once past 0 */
    EQ(tl_sig_from(tl_display(&t, 83400)), 3);               /* 0:01:23, suffix from the 1 */
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
