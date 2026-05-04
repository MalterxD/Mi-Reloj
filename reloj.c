#define _POSIX_C_SOURCE 200810L
#define _DEFAULT_SOURCE

#include "clock.h"
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

#define SEC_PER_HOUR 3600
#define SEC_PER_MIN  60

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_need_clear = 0;
static struct termios g_old_termios;
static int g_termios_saved = 0;

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static void on_winch(int signo)
{
    (void)signo;
    g_need_clear = 1;
}

static void ansi_show_cursor(void) { fputs("\033[?25h", stdout); }
static void ansi_hide_cursor(void) { fputs("\033[?25l", stdout); }
static void ansi_reset_style(void) { fputs("\033[0m", stdout); }

void restore_terminal(void)
{
    ansi_reset_style();
    ansi_show_cursor();
    fputs("\033[?1049l", stdout);
    fflush(stdout);

    if (g_termios_saved)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios);
    }
}

int enable_raw_mode(void)
{
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &g_old_termios) != 0)
        return -1;
    g_termios_saved = 1;
    t = g_old_termios;

    t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;

    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

static void get_terminal_size(int *rows, int *cols)
{
    struct winsize ws;
    *rows = 24;
    *cols = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
    {
        if (ws.ws_row)
            *rows = (int)ws.ws_row;
        if (ws.ws_col)
            *cols = (int)ws.ws_col;
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
    {{1, 1, 1, 1, 1}, {1, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {1, 1, 1, 1, 1}}};

static const unsigned char COLON_MASK[7] = {0, 0, 1, 0, 1, 0, 0};

static void move_cursor(int r, int c) { printf("\033[%d;%dH", r, c); }

static int color_name_to_ansi256(const char *name)
{
    static const struct
    {
        const char *name;
        int code;
    } table[] = {
        {"white", 252},
        {"red", 196},
        {"green", 118},
        {"yellow", 226},
        {"blue", 39},
        {"magenta", 201},
        {"cyan", 51},
        {"orange", 214},
        {"pink", 213},
        {"gray", 240},
        {NULL, -1}};
    for (int i = 0; table[i].name != NULL; i++)
    {
        if (strcasecmp(name, table[i].name) == 0)
            return table[i].code;
    }
    return -1;
}

static char* trim_spaces(char *str) {
    while (*str == ' ' || *str == '\t') str++;
    return str;
}

void load_config(ClockState *state)
{
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return;

    char path[512];
    snprintf(path, sizeof(path), "%s/.clockrc", home);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_spaces(line);
        char *val = trim_spaces(eq + 1);
        
        char *nl = strchr(val, '\n'); 
        if (nl) *nl = '\0';

        if (strcasecmp(key, "autocolor") == 0) {
            state->autocolor = (strcasecmp(val, "true") == 0);
        }
        else if (strcasecmp(key, "color") == 0) {
            int code = color_name_to_ansi256(val);
            if (code >= 0) state->digit_color = code;
        }
        else if (strcasecmp(key, "format") == 0) {
            state->use_24h = (strcasecmp(val, "24h") == 0);
        }
    }
    fclose(f);
}

static void get_distro_short(char *buf, size_t len)
{
    buf[0] = '\0';
    FILE *f = fopen("/etc/os-release", "r");
    if (!f)
        return;

    char id[64] = {0};
    char name[128] = {0};
    char line[256];

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "ID=", 3) == 0)
        {
            char *val = line + 3;
            if (*val == '"')
                val++;
            size_t l = strlen(val);
            while (l > 0 && (val[l - 1] == '\n' || val[l - 1] == '"' || val[l - 1] == '\r'))
                val[--l] = '\0';
            snprintf(id, sizeof(id), "%s", val);
        }
        else if (strncmp(line, "NAME=", 5) == 0)
        {
            char *val = line + 5;
            if (*val == '"')
                val++;
            size_t l = strlen(val);
            while (l > 0 && (val[l - 1] == '\n' || val[l - 1] == '"' || val[l - 1] == '\r'))
                val[--l] = '\0';
            snprintf(name, sizeof(name), "%s", val);
        }
    }
    fclose(f);

    static const struct
    {
        const char *id;
        const char *label;
    } known[] = {
        {"fedora", "Fedora"},
        {"arch", "Arch Linux"},
        {"debian", "Debian"},
        {"ubuntu", "Ubuntu"},
        {"opensuse-leap", "openSUSE Leap"},
        {"opensuse-tumbleweed", "openSUSE Tumbleweed"},
        {"gentoo", "Gentoo"},
        {"nixos", "NixOS"},
        {"manjaro", "Manjaro"},
        {"endeavouros", "EndeavourOS"},
        {"linuxmint", "Linux Mint"},
        {"pop", "Pop!_OS"},
        {"kali", "Kali Linux"},
        {"void", "Void Linux"},
        {"alpine", "Alpine Linux"},
        {"artix", "Artix Linux"},
        {"zorin", "Zorin OS"},
        {"slackware", "Slackware"},
        {"rocky", "Rocky Linux"},
        {"almalinux", "AlmaLinux"},
        {"garuda", "Garuda Linux"},
        {"nobara", "Nobara Linux"},
        {NULL, NULL}};

    for (int i = 0; known[i].id != NULL; i++)
    {
        if (strcasecmp(id, known[i].id) == 0)
        {
            snprintf(buf, len, "%s", known[i].label);
            return;
        }
    }

    if (name[0] != '\0')
        snprintf(buf, len, "%s", name);
}

static int get_color_by_distro(const char *distro)
{
    if (strcasecmp(distro, "Fedora") == 0) return 39;
    if (strcasecmp(distro, "Arch Linux") == 0) return 51;
    if (strcasecmp(distro, "Ubuntu") == 0) return 208;
    if (strcasecmp(distro, "Debian") == 0) return 161;
    if (strcasecmp(distro, "Linux Mint") == 0) return 120;
    if (strcasecmp(distro, "Kali Linux") == 0) return 244;
    
    return 252;
}

static void draw_small_clock(int rows, int cols, int hh, int mm, int ss, int is_pm, ClockState *state) {
    char small_buf[48];
    int len;

    if (state->mode == MODE_CLOCK && !state->use_24h) {
        len = snprintf(small_buf, sizeof(small_buf), " %02d:%02d:%02d %s ", hh, mm, ss, is_pm ? "PM" : "AM");
    } else {
        len = snprintf(small_buf, sizeof(small_buf), " %02d:%02d:%02d ", hh, mm, ss);
    }

    int start_col = (cols - len) / 2 + 1;
    int start_row = rows / 2 + 1;

    move_cursor(start_row, start_col);
    printf("\033[48;5;235m\033[1;38;5;%dm%s\033[0m", state->digit_color, small_buf);
    fflush(stdout);
}

static void draw_big_clock(int rows, int cols, int hh, int mm, int ss, int limit, int time_w, int px_w, int gap_px, ClockState *state) {
    int digit_w = 5, digit_h = 7;
    int top = (rows - digit_h) / 2 + 1;
    int left = (cols - time_w) / 2 + 1;
    int seq[8] = {hh / 10, hh % 10, -1, mm / 10, mm % 10, -1, ss / 10, ss % 10};

    if (state->mode == MODE_TIMER && state->timer_seconds == 0) printf("\033[5m");

    for (int line = 0; line < digit_h; line++) {
        move_cursor(top + line, left);
        for (int p = 0; p < limit; p++) {
            for (int x = 0; x < digit_w; x++) {
                int on = (seq[p] >= 0) ? BIG_MASK[seq[p]][line][x] : (x == 2 && COLON_MASK[line]);
                if (on) {
                    printf("\033[48;5;%dm", state->digit_color);
                    for (int w = 0; w < px_w; w++) putchar(' ');
                    printf("\033[0m");
                } else {
                    for (int w = 0; w < px_w; w++) putchar(' ');
                }
            }
            if (p < limit - 1) {
                for (int g = 0; g < gap_px; g++) putchar(' ');
            }
        }
    }
    ansi_reset_style();
}

void draw_clock_centered(int term_rows, int term_cols, int hh, int mm, int ss, const struct tm *t, int is_pm, ClockState *state) {
    int digit_w = 5;
    int want_seconds = (state->mode == MODE_TIMER) ? 1 : state->show_seconds_big; 
    int n_parts = want_seconds ? 8 : 5;
    int colons = want_seconds ? 2 : 1;
    int digits = want_seconds ? 6 : 4;

    int px_w, gap_px, time_w;
    int fit = 0;

    int configs[3][2] = {
        {2, 2}, 
        {1, 1}, 
        {1, 0}  
    };

    for (int i = 0; i < 3; i++) {
        px_w = configs[i][0];
        gap_px = configs[i][1];
        time_w = (digits * digit_w + colons * digit_w) * px_w + (n_parts - 1) * gap_px;

        if (time_w <= term_cols) {
            fit = 1;
            break;
        }
    }

    if (!fit) {
        draw_small_clock(term_rows, term_cols, hh, mm, ss, is_pm, state);
        return; 
    }

    draw_big_clock(term_rows, term_cols, hh, mm, ss, n_parts, time_w, px_w, gap_px, state);

    int row = ((term_rows - 7) / 2 + 1) + 8; 

    if (state->mode == MODE_CLOCK) {
        static const char *months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
        static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        char date_str[64], distro[64];
        
        snprintf(date_str, sizeof(date_str), "%s, %s %d %04d", days[t->tm_wday], months[t->tm_mon], t->tm_mday, t->tm_year + 1900);
        
        if (row < term_rows) {
            move_cursor(row++, (term_cols - (int)strlen(date_str)) / 2 + 1);
            printf("\033[38;5;245m%s\033[0m", date_str);
        }
        
        get_distro_short(distro, sizeof(distro));
        if (distro[0] != '\0' && row < term_rows) {
            move_cursor(row++, (term_cols - (int)strlen(distro)) / 2 + 1);
            printf("\033[38;5;240m%s\033[0m", distro);
        }

        if (!state->use_24h && row < term_rows) {
            move_cursor(row++, (term_cols - 2) / 2 + 1);
            printf("\033[38;5;245m%s\033[0m", is_pm ? "PM" : "AM");
        }
    } else {
        const char *status = (state->timer_seconds == 0) ? "DONE" : (state->timer_running ? "RUN" : "PAUSE");
        char msg[32];
        snprintf(msg, sizeof(msg), "[ %s ]", status);
        if (row < term_rows) {
            move_cursor(row++, (term_cols - (int)strlen(msg)) / 2 + 1);
            printf("\033[38;5;214m%s\033[0m", msg);
        }
    }
}

static int handle_input(ClockState *state)
{
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1)
    {
        if (ch == 27) { 
            struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
            if (poll(&pfd, 1, 20) <= 0) {
                g_stop = 1;
                return 0;
            }
            unsigned char next_chars[2];
            read(STDIN_FILENO, next_chars, sizeof(next_chars));
            return 0;
        }

        if (ch == 'q' || ch == 'Q') {
            g_stop = 1;
            return 1;
        }
        if (ch == 's' || ch == 'S') {
            state->show_seconds_big = !state->show_seconds_big;
            g_need_clear = 1;
            return 1;
        }
        if (ch == 't' || ch == 'T') {
            state->use_24h = !state->use_24h;
            g_need_clear = 1;
            return 1;
        }
        if (ch == 'v' || ch == 'V') {
            state->mode = (state->mode == MODE_CLOCK) ? MODE_SET_TIMER : MODE_CLOCK;
            g_need_clear = 1;
            return 1;
        }
        if (ch == ' ' && state->mode == MODE_TIMER) {
            state->timer_running = !state->timer_running;
            return 1;
        }
    }
    return 0;
}

static void draw_input_box(int r, int c, const char *title, const char *prompt) {
    int box_w = 40;
    int box_h = 7;
    int start_r = (r - box_h) / 2;
    int start_c = (c - box_w) / 2;

    for (int i = 0; i < box_h; i++) {
        move_cursor(start_r + i, start_c);
        if (i == 0) {
            printf("\033[1;36m┌"); 
            for(int j=0; j<box_w-2; j++) printf("─"); 
            printf("┐\033[0m");
        } else if (i == box_h - 1) {
            printf("\033[1;36m└"); 
            for(int j=0; j<box_w-2; j++) printf("─"); 
            printf("┘\033[0m");
        } else {
            printf("\033[1;36m│\033[0m\033[48;5;234m%*s\033[0m\033[1;36m│\033[0m", box_w - 2, "");
        }
    }

    move_cursor(start_r + 1, start_c + (box_w - (int)strlen(title)) / 2);
    printf("\033[1;37m\033[48;5;234m%s\033[0m", title);
    
    move_cursor(start_r + 3, start_c + (box_w - (int)strlen(prompt)) / 2);
    printf("\033[38;5;242m\033[48;5;234m%s\033[0m", prompt);

    move_cursor(start_r + 5, start_c + (box_w / 2) - 4);
    ansi_show_cursor();
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    ClockState state = {
        .mode = MODE_CLOCK,
        .use_24h = 1,
        .digit_color = 252,
        .show_seconds_big = 0,
        .timer_running = 0,
        .timer_seconds = 0,
        .autocolor = 1
    };

    get_distro_short(state.distro, sizeof(state.distro));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-12") == 0) state.use_24h = 0;
        if (strcmp(argv[i], "-24") == 0) state.use_24h = 1;
    }

    load_config(&state);
    
    if (state.autocolor) {
        int d_color = get_color_by_distro(state.distro);
        if (d_color != 252) state.digit_color = d_color;
    }

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sa_winch = {0};
    sa_winch.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa_winch, NULL);    

    enable_raw_mode();
    fputs("\033[?1049h", stdout);
    ansi_hide_cursor();

    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    time_t last_tick = 0;
    
    while (!g_stop)
    {
        int redraw_now = 0;

        if (poll(&pfd, 1, 10) > 0) {
            if (handle_input(&state)) {
                redraw_now = 1;
            }
        }

        int r, c;
        get_terminal_size(&r, &c);

if (state.mode == MODE_SET_TIMER) {
            printf("\033[2J");
            draw_input_box(r, c, "TIMER CONFIGURATION", "Enter time (HH:MM:SS)");
            
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios); 
            tcflush(STDIN_FILENO, TCIFLUSH);

            printf("\033[1;37m\033[48;5;234m"); 
            fflush(stdout);

            char buf[32];
            if (fgets(buf, sizeof(buf), stdin)) { 
                if (buf[0] == '\n' || buf[0] == '\0') {
                    state.mode = MODE_CLOCK;
                } else {
                    int h = 0, m = 0, s = 0;
                    int valid = 0;
                    if (sscanf(buf, "%d:%d:%d", &h, &m, &s) == 3) valid = 1;
                    else if (sscanf(buf, "%d:%d", &m, &s) == 2) valid = 1;
                    else if (sscanf(buf, "%d", &s) == 1) valid = 1;

                    if (valid) {
                        state.timer_seconds = (long)h * SEC_PER_HOUR + m * SEC_PER_MIN + s;
                        state.mode = (state.timer_seconds > 0) ? MODE_TIMER : MODE_CLOCK;
                        state.timer_running = (state.mode == MODE_TIMER);
                    } else {
                        state.mode = MODE_CLOCK;
                    }
                }
            } else {
                state.mode = MODE_CLOCK;
            }

            enable_raw_mode();
            ansi_hide_cursor();
            printf("\033[2J");
            
            last_tick = 0; 
            g_need_clear = 1;
            continue;
        }

        if (g_need_clear) {
            printf("\033[2J");
            g_need_clear = 0;
            redraw_now = 1;
        }

        time_t now = time(NULL);

        if (now != last_tick) {
            if (state.mode == MODE_TIMER && state.timer_running && state.timer_seconds > 0) {
                state.timer_seconds--;
            }
            redraw_now = 1;
        }

        if (redraw_now) {
            printf("\033[H");
            struct tm *t = localtime(&now);
            int display_h, display_m, display_s, is_pm = 0;

            if (state.mode == MODE_TIMER) {
                display_h = (int)(state.timer_seconds / SEC_PER_HOUR);
                display_m = (int)((state.timer_seconds % SEC_PER_HOUR) / SEC_PER_MIN);
                display_s = (int)(state.timer_seconds % SEC_PER_MIN);
            } else {
                display_h = t->tm_hour;
                display_m = t->tm_min;
                display_s = t->tm_sec;
                is_pm = (display_h >= 12);
                if (!state.use_24h) {
                    display_h = display_h % 12;
                    if (display_h == 0) display_h = 12;
                }
            }

            draw_clock_centered(r, c, display_h, display_m, display_s, t, is_pm, &state);
            fflush(stdout);
            last_tick = now;
        }
    }

    restore_terminal();
    return 0;
}