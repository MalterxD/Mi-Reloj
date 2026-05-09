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
    char path[512]; snprintf(path, sizeof(path), "%s/.clockrc", home);
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
    char small_buf[48]; int len;
    if (!state->use_24h) len = snprintf(small_buf, sizeof(small_buf), "%02d:%02d:%02d %s", hh, mm, ss, is_pm ? "PM" : "AM");
    else len = snprintf(small_buf, sizeof(small_buf), "%02d:%02d:%02d", hh, mm, ss);
    move_cursor(rows / 2 + 1, (cols - len) / 2 + 1);
    printf("\033[1;38;5;%dm%s\033[0m", state->digit_color, small_buf);
}

static void draw_big_clock(int rows, int cols, int hh, int mm, int ss, int limit, int time_w, int px_w, int gap_px, ClockState *state) {
    int top = (rows - 7) / 2 + 1, left = (cols - time_w) / 2 + 1;
    int seq[8] = {hh / 10, hh % 10, -1, mm / 10, mm % 10, -1, ss / 10, ss % 10};
    for (int line = 0; line < 7; line++) {
        move_cursor(top + line, 1); printf("\033[2K"); move_cursor(top + line, left);
        for (int p = 0; p < limit; p++) {
            for (int x = 0; x < 5; x++) {
                int on = (seq[p] >= 0) ? BIG_MASK[seq[p]][line][x] : (x == 2 && COLON_MASK[line]);
                if (on) { printf("\033[48;5;%dm", state->digit_color); for (int w = 0; w < px_w; w++) putchar(' '); printf("\033[0m"); }
                else for (int w = 0; w < px_w; w++) putchar(' ');
            }
            if (p < limit - 1) for (int g = 0; g < gap_px; g++) putchar(' ');
        }
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
        int icon_visual_width;
        
        if (state->use_nerdfonts) {
            icon_visual_width = 2;
            if (strcmp(b.status, "Charging") == 0) icon = "󱐋";
            else if (b.percentage < 20) icon = "󰂎";
            else if (b.percentage < 40) icon = "󰁼";
            else if (b.percentage < 60) icon = "󰁾";
            else if (b.percentage < 85) icon = "󰂀";
            else icon = "󰁹";
        } else {
            icon_visual_width = 3;
            if (strcmp(b.status, "Charging") == 0) icon = "CHG";
            else if (b.percentage < 20) icon = "LOW";
            else icon = "BAT";
        }

        int bar_width = 10;
        int filled = (b.percentage * bar_width) / 100;
        int color = 196;
        if (b.percentage > 50) color = 120;
        else if (b.percentage > 20) color = 226;

        char bar_str[32];
        int pos = 0;
        bar_str[pos++] = '[';
        for (int i = 0; i < bar_width; i++) bar_str[pos++] = (i < filled) ? '|' : '-';
        bar_str[pos++] = ']';
        bar_str[pos] = '\0';

        int perc_digits = (b.percentage >= 100) ? 3 : (b.percentage >= 10) ? 2 : 1;
        int visual_width = icon_visual_width + 1 + (bar_width + 2) + 1 + perc_digits + 1;

        move_cursor(row, 1); printf("\033[2K");
        move_cursor(row++, (term_cols - visual_width) / 2 + 1);
        printf("\033[38;5;%dm%s %s %d%%\033[0m", color, icon, bar_str, b.percentage);
    }

    if (!state->use_24h && row <= term_rows) {
        move_cursor(row, 1); printf("\033[2K");
        move_cursor(row++, (term_cols - 2) / 2 + 1);
        printf("\033[38;5;245m%s\033[0m", is_pm ? "PM" : "AM");
    }
    
    while (row <= term_rows) { move_cursor(row++, 1); printf("\033[2K"); }
}

void print_help(const char *progname) {
    printf("Usage: %s [OPTIONS]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help               Show help\n");
    printf("  -12                      12-hour format\n");
    printf("  -s, --seconds [on|off]   Large seconds toggle\n");
    printf("  -b, --battery [on|off]   Battery bar toggle\n");
    printf("  -C <color>               Set color (0-255 or name: red, green, blue...)\n\n");
    printf("Interactive:\n");
    printf("  q, ESC    Exit\n");
    printf("  s         Toggle seconds\n");
    printf("  t         Toggle 12h/24h\n");
    printf("  b         Toggle battery\n");
}

static int parse_bool_arg(const char *arg) {
    if (!arg) return -1;
    if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "true") == 0) return 1;
    if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "false") == 0) return 0;
    return -1;
}

int main(int argc, char *argv[]) {
    ClockState state = { .use_24h = 1, .digit_color = 252, .show_seconds_big = 0, .autocolor = 1, .show_battery = 0, .use_nerdfonts = 0 };    
    char distro_id[64] = {0};
    
    get_distro_info(state.distro_name, sizeof(state.distro_name), distro_id, sizeof(distro_id));
    load_config(&state);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-12") == 0) {
            state.use_24h = 0;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--battery") == 0) {
            if (i + 1 < argc) {
                int val = parse_bool_arg(argv[i+1]);
                if (val != -1) { state.show_battery = val; i++; }
                else state.show_battery = 1;
            } else {
                state.show_battery = 1;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seconds") == 0) {
            if (i + 1 < argc) {
                int val = parse_bool_arg(argv[i+1]);
                if (val != -1) { state.show_seconds_big = val; i++; }
                else state.show_seconds_big = 1;
            } else {
                state.show_seconds_big = 1;
            }
        } else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
            const char *color_arg = argv[++i];
            state.autocolor = 0;
            if (isdigit(color_arg[0])) {
                int code = atoi(color_arg);
                if (code >= 0 && code <= 255) state.digit_color = code;
            } else {
                int code = color_name_to_ansi256(color_arg);
                if (code >= 0) state.digit_color = code;
            }
        }
    }

    if (state.autocolor) { int d_color = get_color_by_distro(distro_id); if (d_color != 252) state.digit_color = d_color; }
    
    struct sigaction sa = { .sa_handler = on_signal }; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    struct sigaction sa_win = { .sa_handler = on_winch }; sigaction(SIGWINCH, &sa_win, NULL);
    
    if (enable_raw_mode() != 0) return 1;
    atexit(restore_terminal);

    fputs("\033[?1049h", stdout); ansi_hide_cursor();
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    time_t last_tick = 0;
    int force_redraw = 1;

    while (!g_stop) {
        if (poll(&pfd, 1, 50) > 0) {
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
                else if (ch == 's' || ch == 'S') { state.show_seconds_big = !state.show_seconds_big; g_need_clear = 1; force_redraw = 1; }
                else if (ch == 't' || ch == 'T') { state.use_24h = !state.use_24h; g_need_clear = 1; force_redraw = 1; }
                else if (ch == 'b' || ch == 'B') { state.show_battery = !state.show_battery; g_need_clear = 1; force_redraw = 1; }
            }
        }
        int r, c; get_terminal_size(&r, &c);
        time_t now = time(NULL);
        if (now != last_tick || force_redraw || g_need_clear) {
            struct tm *t = localtime(&now);
            int dh = t->tm_hour, dm = t->tm_min, ds = t->tm_sec, pm = (dh >= 12);
            if (!state.use_24h) { dh %= 12; if (dh == 0) dh = 12; }
            
            if (g_need_clear) { printf("\033[2J"); g_need_clear = 0; }
            printf("\033[H"); 
            draw_clock_centered(r, c, dh, dm, ds, t, pm, &state);
            fflush(stdout);
            
            last_tick = now;
            force_redraw = 0;
        }
    }
    return 0;
}
