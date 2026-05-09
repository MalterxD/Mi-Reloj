#ifndef CLOCK_H
#define CLOCK_H

#include <time.h>

typedef struct {
    int use_24h;
    int digit_color;
    int show_seconds_big;
    int autocolor;
    int show_battery;
    int use_nerdfonts;
    char distro_name[128];
} ClockState;

void load_config(ClockState *state);
void draw_clock_centered(int rows, int cols, int hh, int mm, int ss, const struct tm *t, int is_pm, ClockState *state);
int enable_raw_mode(void);
void restore_terminal(void);

#endif
