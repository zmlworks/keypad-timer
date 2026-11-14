/* Keypad Timer - pure timer logic. MIT (c) zmlworks */
#include "tlogic.h"
#include <math.h>

static int clampsec(long long s) { return s < 0 ? 0 : s > TL_MAX_SEC ? TL_MAX_SEC : (int)s; }

void tl_init(Timer *t, int remembered)
{
    t->st = TL_IDLE;
    t->remembered = clampsec(remembered);
    t->disp = t->remembered;
    t->sw = 0;
    t->end_ms = t->rem_ms = t->alarm_ms = 0;
    t->entry = 0;
    t->ndig = 0;
}

int tl_entry_to_sec(int entry)
{
    int h = entry / 100, m = entry % 100;
    return clampsec((long long)h * 3600 + (long long)m * 60);
}

/* Counter in ms: timer = ms left, stopwatch / overtime = ms elapsed (capped) */
static long long value_ms(const Timer *t, long long now)
{
    long long v;
    if (t->st == TL_ALARM) v = now - t->alarm_ms;
    else if (t->st == TL_PAUSE) v = t->rem_ms;
    else if (t->sw) v = now - t->end_ms;
    else v = t->end_ms - now;
    return v < 0 ? 0 : v > TL_MAX_SEC * 1000LL ? TL_MAX_SEC * 1000LL : v;
}

int tl_display(const Timer *t, long long now)
{
    long long v = value_ms(t, now);
    switch (t->st) {
    case TL_RUN:
    case TL_PAUSE: return (int)(t->sw ? v / 1000 : (v + 999) / 1000);
    case TL_ALARM: return (int)(v / 1000);   /* elapsed since the timer reached 0 */
    default:       return t->disp;
    }
}

int tl_ringing(const Timer *t, long long now)
{
    return t->st == TL_ALARM && now - t->alarm_ms < TL_RING_SEC * 1000LL;
}

int tl_sig_from(int sec)
{
    int s = clampsec(sec), d[6], i;
    if (s <= 0) return 6;
    d[0] = s / 36000; d[1] = s / 3600 % 10;
    d[2] = s / 600 % 6; d[3] = s / 60 % 10;
    d[4] = s % 60 / 10; d[5] = s % 10;
    for (i = 0; i < 6; i++) if (d[i] > 0) return i;
    return 6;
}

int tl_accent(const Timer *t, long long now)
{
    if (tl_display(t, now) <= 0) return 0;
    if (t->st == TL_ALARM || t->sw) return 2;   /* blue: stopwatch or overtime */
    return 1;                                   /* red: a timer is set or counting down */
}

int tl_stop_alarm(Timer *t)
{
    if (t->st != TL_ALARM) return 0;
    t->st = TL_IDLE;
    t->disp = t->remembered;
    t->ndig = 0;
    return 1;
}

int tl_digit(Timer *t, int d, long long now)
{
    (void)now;
    if (t->st == TL_ALARM) return tl_stop_alarm(t);
    if (t->st != TL_IDLE || d < 0 || d > 9) return 0;
    if (t->ndig == 0) t->entry = 0;          /* fresh input, no appending */
    t->entry = (t->entry * 10 + d) % 10000;  /* keep the last 4 digits */
    if (t->ndig < 4) t->ndig++;
    t->disp = tl_entry_to_sec(t->entry);
    return 1;
}

int tl_add(Timer *t, int sec, long long now)
{
    long long rem;
    if (t->sw && t->st != TL_IDLE) return 0;   /* nothing to add to a stopwatch */
    switch (t->st) {
    case TL_ALARM: return tl_stop_alarm(t);
    case TL_IDLE:
        t->disp = clampsec((long long)t->disp + sec);
        t->ndig = 0;  /* next digit starts fresh */
        return 1;
    case TL_PAUSE:
        rem = t->rem_ms + (long long)sec * 1000;
        t->rem_ms = rem > TL_MAX_SEC * 1000LL ? TL_MAX_SEC * 1000LL : rem;
        return 1;
    case TL_RUN:
        rem = t->end_ms - now + (long long)sec * 1000;
        if (rem > TL_MAX_SEC * 1000LL) rem = TL_MAX_SEC * 1000LL;
        t->end_ms = now + rem;
        return 1;
    }
    return 0;
}

int tl_startstop(Timer *t, long long now)
{
    switch (t->st) {
    case TL_IDLE:   /* a time shown starts the timer; 0:00:00 starts the stopwatch */
        t->sw = t->disp <= 0;
        t->remembered = t->disp;
        t->end_ms = t->sw ? now : now + (long long)t->disp * 1000;
        t->ndig = 0;
        t->st = TL_RUN;
        return 1;
    case TL_RUN:
        t->rem_ms = value_ms(t, now);
        t->st = TL_PAUSE;
        return 1;
    case TL_PAUSE:
        t->end_ms = t->sw ? now - t->rem_ms : now + t->rem_ms;
        t->st = TL_RUN;
        return 1;
    case TL_ALARM:
        return tl_stop_alarm(t);
    }
    return 0;
}

int tl_reset(Timer *t, long long now)
{
    (void)now;
    if (t->st != TL_IDLE) {          /* running, paused or ringing */
        t->st = TL_IDLE;
        t->disp = t->remembered;
    } else if (t->disp != t->remembered) {
        t->disp = t->remembered;     /* back to the last measured time */
    } else {
        t->disp = 0;                 /* second press: clear to 0 */
    }
    t->sw = 0;
    t->ndig = 0;
    return 1;
}

int tl_backspace(Timer *t, long long now)
{
    (void)now;
    if (t->st == TL_ALARM) return tl_stop_alarm(t);
    if (t->st != TL_IDLE) return 0;
    if (t->ndig == 0) {               /* shown time (recalled/added): edit it as H MM digits */
        int e = t->disp / 3600 * 100 + t->disp / 60 % 60, n = 0, k;
        if (e == 0 && t->disp == 0) return 0;
        for (k = e; k > 0; k /= 10) n++;
        t->entry = e;
        t->ndig = n;
    }
    t->entry /= 10;
    t->ndig--;
    if (t->entry == 0) t->ndig = 0;   /* nothing left: next digit starts fresh */
    t->disp = tl_entry_to_sec(t->entry);
    return 1;
}

int tl_tick(Timer *t, long long now)
{
    if (t->st == TL_RUN && !t->sw && now >= t->end_ms) {
        t->st = TL_ALARM;
        t->alarm_ms = now;
        return 1;
    }
    return 0;
}

int tl_colon_on(const Timer *t, long long now)
{
    long long v = value_ms(t, now);
    if (t->st == TL_ALARM) {
        if (tl_ringing(t, now)) return 1;          /* solid while the whole display blinks */
        return v % 1000 < 500;                     /* then blink like a stopwatch */
    }
    if (t->st != TL_RUN || (t->sw && v >= TL_MAX_SEC * 1000LL)) return 1;
    return t->sw ? v % 1000 < 500 : (v + 999) % 1000 >= 500;
}

long long tl_next_change_ms(const Timer *t, long long now)
{
    long long v = value_ms(t, now);
    if (t->st == TL_ALARM) {
        if (v >= TL_MAX_SEC * 1000LL) return -1;
        return 500 - v % 500;
    }
    if (t->st != TL_RUN || (t->sw && v >= TL_MAX_SEC * 1000LL)) return -1;
    if (t->sw) return 500 - v % 500;
    return v <= 0 ? 0 : (v - 1) % 500 + 1;
}

double tl_alarm_gain(double t, double fade_sec)
{
    if (fade_sec <= 0 || t >= fade_sec) return 1.0;
    if (t < 0) t = 0;
    return pow(10.0, -(1.0 - t / fade_sec) * TL_FADE_DB / 20.0);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static double tone(long long n)
{
    double t = (double)n / TL_SR;
    return 0.86 * sin(2 * M_PI * TL_TONE_HZ * t) + 0.14 * sin(4 * M_PI * TL_TONE_HZ * t);
}

/* 4 beeps of 70 ms (with 4 ms soft edges) in each second */
static double beeps(double t)
{
    double ph = fmod(t, 1.0);
    int k;
    for (k = 0; k < 4; k++) {
        double s = k * 0.12, e = s + 0.07;
        if (ph >= s && ph < e) return fmin(1.0, fmin(ph - s, e - ph) / 0.004);
    }
    return 0;
}

short tl_alarm_sample(long long n)
{
    double t = (double)n / TL_SR;
    if (t >= TL_RING_SEC) return 0;
    return (short)lrint(tone(n) * beeps(t) * tl_alarm_gain(t, TL_FADE_SEC) * TL_AMP * 32767);
}

short tl_click_sample(long n, long len)
{
    double env = fmin(1.0, fmin(n / (TL_SR * 0.003), (double)(len - n) / (TL_SR * 0.006)));
    return (short)lrint(tone(n) * env * TL_AMP * 32767);
}
