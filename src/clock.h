#ifndef CLOCK_H
#define CLOCK_H

#include <time.h>
#include <stdbool.h>

typedef enum { DATE_SMALL, DATE_LARGE, DATE_HIDDEN } DateSize;
typedef enum { DISTRO_DIM, DISTRO_AUTO } DistroColor;

typedef struct {
    bool use_24h;
    int digit_color;
    bool show_seconds_big;
    bool autocolor;
    bool show_battery;
    bool use_nerdfonts;
    char distro_name[128];
    bool screensaver;
    int x, y, vx, vy;
    DateSize date_mode;
    bool clean_mode;
    DistroColor distro_color;
} ClockState;

void load_config(ClockState *state);
void draw_clock_centered(int rows, int cols, int hh, int mm, int ss, const struct tm *t, int is_pm, ClockState *state);
int enable_raw_mode(void);
void restore_terminal(void);

#endif
