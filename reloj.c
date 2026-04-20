#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_need_clear = 0;
static volatile sig_atomic_t g_show_seconds_big = 0;

static struct termios g_old_termios;
static int g_termios_saved = 0;

static void on_signal(int signo) {
    (void)signo;
    g_stop = 1;
}

static void on_winch(int signo) {
    (void)signo;
    g_need_clear = 1;
}

static void ansi_show_cursor(void) { fputs("\033[?25h", stdout); }
static void ansi_reset_style(void) { fputs("\033[0m", stdout); }

static void restore_terminal(void) {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios);
    }
    ansi_reset_style();
    ansi_show_cursor();
    fflush(stdout);
}

static int enable_raw_mode(void) {
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &g_old_termios) != 0) return -1;
    g_termios_saved = 1;
    t = g_old_termios;

    t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;

    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

static void get_terminal_size(int *rows, int *cols) {
    struct winsize ws;
    *rows = 24; *cols = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_row) *rows = (int)ws.ws_row;
        if (ws.ws_col) *cols = (int)ws.ws_col;
    }
}

static const unsigned char BIG_MASK[10][7][5] = {
    {{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1}}, // 0
    {{0,0,1,0,0},{0,1,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,1,1,1,0}}, // 1
    {{1,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,1}}, // 2
    {{1,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,1}}, // 3
    {{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{0,0,0,0,1}}, // 4
    {{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,1}}, // 5
    {{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1}}, // 6
    {{1,1,1,1,1},{0,0,0,0,1},{0,0,0,1,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0}}, // 7
    {{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1}}, // 8
    {{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,1}}  // 9
};

static const unsigned char COLON_MASK[7] = {0, 0, 1, 0, 1, 0, 0};

static void move_cursor(int r, int c) { printf("\033[%d;%dH", r, c); }

static void draw_clock_centered(int term_rows, int term_cols, int hh, int mm, int ss) {
    int digit_w = 5, digit_h = 7, px_w = 2, gap_px = 2;
    int want_seconds = g_show_seconds_big;

    int n_parts = want_seconds ? 8 : 5;
    int time_w = ((want_seconds ? 6 : 4) * digit_w + (want_seconds ? 2 : 1)) * px_w + (n_parts - 1) * (gap_px * 2);

    if (time_w > term_cols) { // Fallback texto simple si no cabe
        move_cursor(term_rows / 2, (term_cols - 8) / 2);
        printf("%02d:%02d:%02d", hh, mm, ss);
        return;
    }

    int top = (term_rows - digit_h) / 2 + 1;
    int left = (term_cols - time_w) / 2 + 1;

    int seq[8] = {hh/10, hh%10, -1, mm/10, mm%10, -1, ss/10, ss%10};
    int limit = want_seconds ? 8 : 5;

    for (int line = 0; line < digit_h; line++) {
        move_cursor(top + line, left);
        for (int p = 0; p < limit; p++) {
            for (int x = 0; x < digit_w; x++) {
                int on = (seq[p] >= 0) ? BIG_MASK[seq[p]][line][x] : (x == 2 && COLON_MASK[line]);
                if (on) printf("\033[48;5;252m  \033[0m");
                else printf("  ");
            }
            if (p < limit - 1) printf("  ");
        }
    }
    
    if (!want_seconds) {
        move_cursor(top + digit_h + 1, left + (time_w / 2) - 3);
        printf("\033[38;5;245mSS: %02d\033[0m", ss);
    }
}

static void handle_input(void) {
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == 'q' || ch == 'Q' || ch == 27) g_stop = 1;
        if (ch == 's' || ch == 'S') { g_show_seconds_big = !g_show_seconds_big; g_need_clear = 1; }
    }
}

int main(void) {
    struct sigaction sa = {.sa_handler = on_signal};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sw = {.sa_handler = on_winch};
    sigaction(SIGWINCH, &sw, NULL);

    if (isatty(STDIN_FILENO)) enable_raw_mode();
    atexit(restore_terminal);

    printf("\033[2J\033[H\033[?25l");

    int last_r = 0, last_c = 0;

    while (!g_stop) {
        time_t ahora = time(NULL);
        struct tm *t = localtime(&ahora);

        int r, c;
        get_terminal_size(&r, &c);

        if (g_need_clear || r != last_r || c != last_c) {
            printf("\033[2J");
            g_need_clear = 0; last_r = r; last_c = c;
        }
        printf("\033[H");

        draw_clock_centered(r, c, t->tm_hour, t->tm_min, t->tm_sec);
        fflush(stdout);

        handle_input();
        sleep(1);
    }

    return 0;
}