#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "clock.h"
#include "battery.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <pwd.h>
#include <poll.h>
#include <ctype.h>
#include <stdbool.h>
#include <libgen.h>

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_need_clear = 0;
static struct termios g_old_termios;
static int g_termios_saved = 0;

static void on_signal(int signo) { (void)signo; g_stop = 1; }
static void on_winch(int signo) { (void)signo; g_need_clear = 1; }

static void ansi_show_cursor(void) { fputs("\033[?25h", stdout); }
static void ansi_hide_cursor(void) { fputs("\033[?25l", stdout); }
static void ansi_reset_style(void) { fputs("\033[0m", stdout); }

void restore_terminal(void) {
    ansi_reset_style();
    ansi_show_cursor();
    fputs("\033[?1049l", stdout);
    fflush(stdout);
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios);
        g_termios_saved = 0;
    }
}

int enable_raw_mode(void) {
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &g_old_termios) != 0) return -1;
    g_termios_saved = 1;
    t = g_old_termios;
    t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
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
    {{1, 1, 1, 1, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}},
    {{0, 0, 1, 0, 0}, {0, 1, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 1, 1, 1, 0}},
    {{1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {1, 1, 1, 1, 1}, {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {1, 1, 1, 1, 1}},
    {{1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {1, 1, 1, 1, 1}},
    {{1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}},
    {{1, 1, 1, 1, 1}, {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {1, 1, 1, 1, 1}},
    {{1, 1, 1, 1, 1}, {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {1, 1, 1, 1, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}},
    {{1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 1, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}},
    {{1, 1, 1, 1, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}},
    {{1, 1, 1, 1, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {1, 1, 1, 1, 1}}
};

static const unsigned char COLON_MASK[7] = {0, 0, 1, 0, 1, 0, 0};

static void move_cursor(int r, int c) { printf("\033[%d;%dH", r, c); }

static int color_name_to_ansi256(const char *name) {
    static const struct { const char *name; int code; } table[] = {
          {"red", 196}, {"green", 46}, {"blue", 21}, {"yellow", 226},
          {"magenta", 201}, {"cyan", 51}, {"orange", 208}, {"white", 231},
          {"gray", 240}, {"pink", 213},
          {NULL, -1}
    };
    for (int i = 0; table[i].name != NULL; i++)
        if (strcasecmp(name, table[i].name) == 0) return table[i].code;
    return -1;
}

static char* trim_spaces(char *str) {
    while (*str == ' ' || *str == '\t') str++;
    char *end = str + strlen(str) - 1;
    while (end > str && (isspace(*end))) { *end = '\0'; end--; }
    return str;
}

void load_config(ClockState *state) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return;
    char path[256]; snprintf(path, sizeof(path), "%s/.clockrc", home);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = '\0'; char *key = trim_spaces(line), *val = trim_spaces(eq + 1);
        if (strcasecmp(key, "autocolor") == 0) state->autocolor = (strcasecmp(val, "true") == 0);
        else if (strcasecmp(key, "nerdfonts") == 0) state->use_nerdfonts = (strcasecmp(val, "true") == 0);
        else if (strcasecmp(key, "color") == 0) { 
            if (isdigit(val[0])) {
                int code = atoi(val);
                if (code >= 0 && code <= 255) state->digit_color = code;
            } else {
                int code = color_name_to_ansi256(val);
                if (code >= 0) state->digit_color = code;
            }
        }
        else if (strcasecmp(key, "format") == 0) state->use_24h = (strcasecmp(val, "24h") == 0);
        else if (strcasecmp(key, "show_battery") == 0) state->show_battery = (strcasecmp(val, "true") == 0);
        else if (strcasecmp(key, "seconds") == 0) state->show_seconds_big = (strcasecmp(val, "true") == 0);
    }
    fclose(f);
}

static int get_color_by_distro(const char *id) {
    if (!id) return 252;
    if (strcasecmp(id, "fedora") == 0) return 39;
    if (strcasecmp(id, "arch") == 0) return 33;
    if (strcasecmp(id, "ubuntu") == 0) return 202;
    if (strcasecmp(id, "debian") == 0) return 160;
    if (strcasecmp(id, "linuxmint") == 0) return 119;
    if (strcasecmp(id, "kali") == 0) return 69;
    if (strcasecmp(id, "pop") == 0) return 45;
    if (strcasecmp(id, "manjaro") == 0) return 120;
    if (strcasecmp(id, "void") == 0) return 107;
    if (strstr(id, "opensuse") != NULL) return 113; 
    return 252;
}

static void get_distro_info(char *name_buf, size_t name_len, char *id_buf, size_t id_len) {
    name_buf[0] = '\0'; id_buf[0] = '\0';
    FILE *f = fopen("/etc/os-release", "r"); if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (name_buf[0] != '\0' && id_buf[0] != '\0') break;
        if (strncmp(line, "NAME=", 5) == 0 && name_buf[0] == '\0') {
            char *val = line + 5; if (*val == '"') val++;
            size_t l = strlen(val); 
            while (l > 0 && (val[l-1] == '\n' || val[l-1] == '"' || val[l-1] == '\r')) val[--l] = '\0';
            snprintf(name_buf, name_len, "%s", val);
        } else if (strncmp(line, "ID=", 3) == 0 && id_buf[0] == '\0') {
            char *val = line + 3; if (*val == '"') val++;
            size_t l = strlen(val); 
            while (l > 0 && (val[l-1] == '\n' || val[l-1] == '"' || val[l-1] == '\r')) val[--l] = '\0';
            snprintf(id_buf, id_len, "%s", val);
        }
    }
    fclose(f);
}

static void draw_small_clock(int rows, int cols, int hh, int mm, int ss, int is_pm, ClockState *state) {
    char buf[64]; int len;
    if (!state->use_24h) len = snprintf(buf, sizeof(buf), "%02d:%02d:%02d %s", hh, mm, ss, is_pm ? "PM" : "AM");
    else len = snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
    move_cursor(rows / 2 + 1, (cols - len) / 2 + 1);
    printf("\033[1;38;5;%dm%s\033[0m", state->digit_color, buf);
}

static void draw_big_clock(int rows, int cols, int hh, int mm, int ss, int limit, int time_w, int px_w, int gap_px, ClockState *state) {
    int top, left;
    if (state->screensaver) {
        top = state->y;
        left = state->x;
    } else {
        top = (rows - 7) / 2 + 1;
        left = (cols - time_w) / 2 + 1;
    }

    int seq[8] = {hh / 10, hh % 10, -1, mm / 10, mm % 10, -1, ss / 10, ss % 10};
    char line_buf[1024];

    for (int line = 0; line < 7; line++) {
        int pos = 0;
        if (state->screensaver) {
            pos += snprintf(line_buf + pos, sizeof(line_buf) - pos, "\033[%d;%dH", top + line, left);
        } else {
            pos += snprintf(line_buf + pos, sizeof(line_buf) - pos, "\033[%d;%dH\033[2K", top + line, left);
        }
        for (int p = 0; p < limit; p++) {
            for (int x = 0; x < 5; x++) {
                int on = (seq[p] >= 0) ? BIG_MASK[seq[p]][line][x] : (x == 2 && COLON_MASK[line]);
                if (on) {
                    pos += snprintf(line_buf + pos, sizeof(line_buf) - pos, "\033[48;5;%dm", state->digit_color);
                    for (int w = 0; w < px_w; w++) line_buf[pos++] = ' ';
                    pos += snprintf(line_buf + pos, sizeof(line_buf) - pos, "\033[0m");
                } else {
                    for (int w = 0; w < px_w; w++) line_buf[pos++] = ' ';
                }
            }
            if (p < limit - 1) for (int g = 0; g < gap_px; g++) line_buf[pos++] = ' ';
        }
        line_buf[pos] = '\0';
        fputs(line_buf, stdout);
    }
}

void draw_clock_centered(int term_rows, int term_cols, int hh, int mm, int ss, const struct tm *t, int is_pm, ClockState *state) {
    int want_seconds = state->show_seconds_big; 
    int n_parts = want_seconds ? 8 : 5, colons = want_seconds ? 2 : 1, digits = want_seconds ? 6 : 4;
    int px_w, gap_px, time_w, fit = 0;
    int configs[3][2] = {{2, 1}, {1, 1}, {1, 0}};

    for (int i = 0; i < 3; i++) {
        px_w = configs[i][0]; gap_px = configs[i][1];
        time_w = (digits * 5 + colons * 5) * px_w + (n_parts - 1) * gap_px;
        if (time_w <= term_cols) { fit = 1; break; }
    }

    if (!fit) { draw_small_clock(term_rows, term_cols, hh, mm, ss, is_pm, state); return; }

    draw_big_clock(term_rows, term_cols, hh, mm, ss, n_parts, time_w, px_w, gap_px, state);

    if (state->screensaver) return;

    int row = ((term_rows - 7) / 2 + 1) + 8;
    static const char *months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    char date_str[64];
    snprintf(date_str, sizeof(date_str), "%s, %s %d %04d", days[t->tm_wday], months[t->tm_mon], t->tm_mday, t->tm_year + 1900);

    if (row <= term_rows) {
        move_cursor(row, 1); printf("\033[2K");
        move_cursor(row++, (term_cols - (int)strlen(date_str)) / 2 + 1);
        printf("\033[38;5;245m%s\033[0m", date_str);
    }

    if (state->distro_name[0] != '\0' && row <= term_rows) {
        move_cursor(row, 1); printf("\033[2K");
        move_cursor(row++, (term_cols - (int)strlen(state->distro_name)) / 2 + 1);
        printf("\033[38;5;240m%s\033[0m", state->distro_name);
    }

    if (state->show_battery && row <= term_rows) {
        BatteryInfo b = get_battery_info();
        const char *icon;
        int icon_width;
        
        if (state->use_nerdfonts) {
            icon_width = 2;
            if (strcmp(b.status, "Charging") == 0) icon = "󱐋";
            else if (b.percentage < 20) icon = "󰂎";
            else if (b.percentage < 30) icon = "󰁺";
            else if (b.percentage < 40) icon = "󰁻";
            else if (b.percentage < 50) icon = "󰁼";
            else if (b.percentage < 60) icon = "󰁽";
            else if (b.percentage < 70) icon = "󰁾";
            else if (b.percentage < 80) icon = "󰁿";
            else if (b.percentage < 90) icon = "󰂀";
            else icon = "󰁹";
        } else {
            icon_width = 3;
            if (strcmp(b.status, "Charging") == 0) icon = "CHG";
            else if (b.percentage < 20) icon = "LOW";
            else icon = "BAT";
        }

        int bar_w = 10, filled = (b.percentage * bar_w) / 100;
        int color = (b.percentage > 50) ? 120 : (b.percentage > 20) ? 226 : 196;

        char bar[32]; int p = 0;
        bar[p++] = '[';
        for (int i = 0; i < bar_w; i++) bar[p++] = (i < filled) ? '|' : '-';
        bar[p++] = ']'; bar[p] = '\0';

        int perc_w = (b.percentage >= 100) ? 3 : (b.percentage >= 10) ? 2 : 1;
        int vis_w = icon_width + 1 + (bar_w + 2) + 1 + perc_w + 1;

        move_cursor(row, 1); printf("\033[2K");
        move_cursor(row++, (term_cols - vis_w) / 2 + 1);
        printf("\033[38;5;%dm%s %s %d%%\033[0m", color, icon, bar, b.percentage);
    }

    if (!state->use_24h && row <= term_rows) {
        move_cursor(row, 1); printf("\033[2K");
        move_cursor(row++, (term_cols - 2) / 2 + 1);
        printf("\033[38;5;245m%s\033[0m", is_pm ? "PM" : "AM");
    }
    while (row <= term_rows) { move_cursor(row++, 1); printf("\033[2K"); }
}

static void print_help(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -12                   Use 12-hour format\n");
    printf("  -s [on|off]           Show seconds\n");
    printf("  -sc                   Enable Screensaver mode\n");
    printf("  -b [on|off]           Show battery status\n");
    printf("  -C <color>            Set digit color (name or 0-255)\n");
    printf("\nInteractive keys:\n");
    printf("  q, ESC                Quit\n");
    printf("  s                     Toggle seconds\n");
    printf("  t                     Toggle 12/24h format\n");
    printf("  b                     Toggle battery\n");
}

int main(int argc, char *argv[]) {
    const char *prog = basename(argv[0]);
    ClockState state = { 
        .use_24h = true, 
        .digit_color = 252, 
        .show_seconds_big = false, 
        .autocolor = true, 
        .show_battery = false, 
        .use_nerdfonts = false,
        .screensaver = false,
        .x = 1, .y = 1, .vx = 1, .vy = 1
    };    
    char d_id[64] = {0};
    
    get_distro_info(state.distro_name, sizeof(state.distro_name), d_id, sizeof(d_id));
    load_config(&state);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(prog);
            return 0;
        }
        else if (strcmp(argv[i], "-12") == 0) state.use_24h = false;
        else if (strcmp(argv[i], "-sc") == 0) {
            state.screensaver = true;
            state.vx = 1; state.vy = 1;
            state.show_battery = false;
            state.distro_name[0] = '\0';
        }
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--battery") == 0) {
            if (i + 1 < argc && strcmp(argv[i + 1], "off") == 0) {
                state.show_battery = false; i++;
            } else {
                if (i + 1 < argc && strcmp(argv[i + 1], "on") == 0) i++;
                state.show_battery = true;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seconds") == 0) {
            if (i + 1 < argc && strcmp(argv[i + 1], "off") == 0) {
                state.show_seconds_big = false; i++;
            } else {
                if (i + 1 < argc && strcmp(argv[i + 1], "on") == 0) i++;
                state.show_seconds_big = true;
            }
        } else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
            const char *arg = argv[++i]; state.autocolor = false;
            if (isdigit(arg[0])) { int c = atoi(arg); if (c >= 0 && c <= 255) state.digit_color = c; }
            else { int c = color_name_to_ansi256(arg); if (c >= 0) state.digit_color = c; }
        }
    }

    if (state.autocolor) { int c = get_color_by_distro(d_id); if (c != 252) state.digit_color = c; }
    
    struct sigaction sa = { .sa_handler = on_signal }; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    struct sigaction sa_win = { .sa_handler = on_winch }; sigaction(SIGWINCH, &sa_win, NULL);
    
    if (enable_raw_mode() != 0) return 1;
    atexit(restore_terminal);
    fputs("\033[?1049h", stdout); ansi_hide_cursor();

    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    time_t last_tick = 0;
    int force = 1;

    while (!g_stop) {
        int poll_timeout = state.screensaver ? 80 : 50;
        if (poll(&pfd, 1, poll_timeout) > 0) {
            if (state.screensaver) { g_stop = 1; break; }
            unsigned char ch; if (read(STDIN_FILENO, &ch, 1) == 1) {
                if (ch == 'q' || ch == 'Q') g_stop = 1;
                else if (ch == 27) {
                    struct pollfd p2 = {STDIN_FILENO, POLLIN, 0};
                    if (poll(&p2, 1, 0) == 0) g_stop = 1;
                    else {
                        unsigned char garbage[16];
                        read(STDIN_FILENO, garbage, sizeof(garbage));
                    }
                }
                else if (ch == 's' || ch == 'S') { state.show_seconds_big = !state.show_seconds_big; g_need_clear = 1; force = 1; }
                else if (ch == 't' || ch == 'T') { state.use_24h = !state.use_24h; g_need_clear = 1; force = 1; }
                else if (ch == 'b' || ch == 'B') { state.show_battery = !state.show_battery; g_need_clear = 1; force = 1; }
            }
        }

        int r, c; get_terminal_size(&r, &c);
        time_t now = time(NULL);

        if (state.screensaver) {
            int n_parts = state.show_seconds_big ? 8 : 5;
            int colons = state.show_seconds_big ? 2 : 1;
            int digits = state.show_seconds_big ? 6 : 4;
            int px_w = 1, gap_px = 0, time_w = 0;
            int configs[3][2] = {{2, 1}, {1, 1}, {1, 0}};
            for (int i = 0; i < 3; i++) {
                px_w = configs[i][0]; gap_px = configs[i][1];
                time_w = (digits * 5 + colons * 5) * px_w + (n_parts - 1) * gap_px;
                if (time_w <= c) break;
            }
            (void)gap_px;

            state.x += state.vx;
            state.y += state.vy;

            if (state.x <= 1) { state.x = 1; state.vx = 1; }
            if (state.x + time_w > c) { state.x = c - time_w; state.vx = -1; }
            if (state.y <= 1) { state.y = 1; state.vy = 1; }
            if (state.y + 7 > r) { state.y = r - 7; state.vy = -1; }
            
            struct tm *t = localtime(&now);
            int dh = t->tm_hour, dm = t->tm_min, ds = t->tm_sec, pm = (dh >= 12);
            if (!state.use_24h) { dh %= 12; if (dh == 0) dh = 12; }
            
            fputs("\033[2J", stdout);
            fputs("\033[H", stdout);
            draw_clock_centered(r, c, dh, dm, ds, t, pm, &state);
            fflush(stdout);
            last_tick = now; force = 0; g_need_clear = 0;
        } else if (now != last_tick || force || g_need_clear) {
            struct tm *t = localtime(&now);
            int dh = t->tm_hour, dm = t->tm_min, ds = t->tm_sec, pm = (dh >= 12);
            if (!state.use_24h) { dh %= 12; if (dh == 0) dh = 12; }
            if (g_need_clear) { fputs("\033[2J", stdout); g_need_clear = 0; }
            fputs("\033[H", stdout);
            draw_clock_centered(r, c, dh, dm, ds, t, pm, &state);
            fflush(stdout);
            last_tick = now; force = 0;
        }
    }
    return 0;
}
