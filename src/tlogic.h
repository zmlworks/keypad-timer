/* Keypad Timer - pure timer logic (no Win32), unit-testable. MIT (c) zmlworks */
#ifndef TLOGIC_H
#define TLOGIC_H

#define TL_MAX_SEC (99 * 3600 + 59 * 60 + 59) /* 99:59:59 */
#define TL_FADE_DB 36.0 /* alarm starts this many dB below full */

typedef enum { TL_IDLE, TL_RUN, TL_PAUSE, TL_ALARM } TLState;

typedef struct {
    TLState st;
    int disp;            /* IDLE: seconds shown */
    int sw;              /* 1 = stopwatch (counts up), started with 0:00:00 shown */
    long long end_ms;    /* RUN: timer end time, or stopwatch start time (ms) */
    long long rem_ms;    /* PAUSE: timer ms left, or stopwatch ms elapsed */
    long long alarm_ms;  /* ALARM: time the alarm started */
    int remembered;      /* the time of the last START from IDLE */
    int entry;           /* digits typed, as integer HMM (max 4 digits) */
    int ndig;            /* digits typed so far; 0 = next digit starts fresh */
} Timer;

void tl_init(Timer *t, int remembered);
int  tl_entry_to_sec(int entry);         /* "130" -> 1h30m, "90" -> 1h30m */
int  tl_display(const Timer *t, long long now); /* seconds to show */
/* Button actions. Each returns 1 if something changed, 0 if ignored. */
int  tl_digit(Timer *t, int d, long long now);
int  tl_add(Timer *t, int sec, long long now);
int  tl_startstop(Timer *t, long long now);
int  tl_reset(Timer *t, long long now);
int  tl_backspace(Timer *t, long long now); /* delete the last digit (idle only) */
int  tl_stop_alarm(Timer *t);
/* Returns 1 when a running timer has just reached 0 (enters ALARM). */
int  tl_tick(Timer *t, long long now);
/* RUN: 1 while the colon is lit (first half of each second); otherwise always 1.
   ALARM: lit while ringing; after the ring, blinks like a stopwatch. */
int  tl_colon_on(const Timer *t, long long now);
/* ms until the shown second or the colon changes (RUN or ALARM), else -1 */
long long tl_next_change_ms(const Timer *t, long long now);
/* 1 while ALARM and the synthesized ring is still playing (first TL_RING_SEC). */
int  tl_ringing(const Timer *t, long long now);
/* First significant digit in HH:MM:SS (0 = tens of hours .. 5 = ones of seconds; 6 = none). */
int  tl_sig_from(int sec);
/* Accent for significant digits: 0 = none, 1 = timer (red), 2 = stopwatch / overtime (blue). */
int  tl_accent(const Timer *t, long long now);
/* Alarm gain 0..1 at t seconds after the alarm started (fade-in). */
double tl_alarm_gain(double t, double fade_sec);

/* ---- generated sounds (no sound files) ---- */
#define TL_SR        22050
#define TL_TONE_HZ   2000.0
#define TL_AMP       (0.36 * 0.3)  /* 30 % of v0.1's default level */
#define TL_FADE_SEC  30            /* fades in over 30 s ... */
#define TL_RING_SEC  120           /* ... and stops 2 min after it starts ringing */
/* alarm sample n: 4 short beeps per second, fading in, silent after TL_RING_SEC */
short tl_alarm_sample(long long n);
/* button click sample n of a clip of len samples, same peak as the alarm at full */
short tl_click_sample(long n, long len);

#endif
