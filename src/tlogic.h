/* Keypad Timer - pure timer logic (no Win32), unit-testable. MIT (c) zmlworks */
#ifndef TLOGIC_H
#define TLOGIC_H

#define TL_MAX_SEC (99 * 3600 + 59 * 60 + 59) /* 99:59:59 */
#define TL_FADE_DB 36.0 /* alarm starts this many dB below max */

typedef enum { TL_IDLE, TL_RUN, TL_PAUSE, TL_ALARM } TLState;

typedef struct {
    TLState st;
    int disp;            /* IDLE: seconds shown */
    long long end_ms;    /* RUN: absolute end time (ms) */
    long long rem_ms;    /* PAUSE: remaining ms */
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
/* ms until the shown second changes (RUN only), else -1 */
long long tl_next_change_ms(const Timer *t, long long now);
/* Alarm gain 0..1 at t seconds after the alarm started (before volume). */
double tl_alarm_gain(double t, double fade_sec);
/* Percent setting 0..100 -> linear amplitude 0..1 */
double tl_volume_amp(int percent);

/* ---- generated sounds (no sound files) ---- */
#define TL_SR 22050
#define TL_TONE_HZ 2000.0
#define TL_PEAK 0.95  /* fraction of full scale at 100 % volume */
/* kitchen-timer pattern: 4 short beeps then a rest, period 1 s; returns 0..1 */
double tl_pattern_env(double t);
double tl_tone(long long n);
/* alarm sample n (16-bit) for volume %, fade-in seconds and minutes at max */
short tl_alarm_sample(long long n, int vol, int fade_sec, int max_min);
/* button click / preview sample n of a clip of len samples (pattern=1: alarm beeps at max) */
short tl_shot_sample(long n, long len, int vol, int pattern);

#endif
