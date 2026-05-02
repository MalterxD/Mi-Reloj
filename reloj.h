#ifndef RELOJ_H
#define RELOJ_H

#include <time.h>

#define MODO_RELOJ 0
#define MODO_TEMPORIZADOR 1
#define MODO_SET_TIMER 2

typedef struct {
    int mode;
    int use_24h;
    int digit_color;
    int show_seconds_big;
    int timer_running;
    long timer_seconds;
} ClockState;


void load_config(ClockState *state);
void draw_clock_centered(int rows, int cols, int hh, int mm, int ss, const struct tm *t, int is_pm, ClockState *state);
int enable_raw_mode(void);
void restore_terminal(void);

#endif
