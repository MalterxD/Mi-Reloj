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
#include <strings.h>
#include <pwd.h>
#include <poll.h>

#define MODO_RELOJ 0
#define MODO_TEMPORIZADOR 1
#define MODO_SET_TIMER 2
static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_need_clear = 0;
static volatile sig_atomic_t g_show_seconds_big = 0;
static volatile sig_atomic_t g_mode = MODO_RELOJ;
static volatile sig_atomic_t g_timer_running = 0;

static int g_digit_color = 252;
static int g_use_24h = 1;
static struct termios g_old_termios;
static int g_termios_saved = 0;
static long g_timer_seconds = 0;

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

static void restore_terminal(void)
{
    if (g_termios_saved)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios);
    }
    ansi_reset_style();
    ansi_show_cursor();
    fflush(stdout);
}

static int enable_raw_mode(void)
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

static void load_config(void)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            home = pw->pw_dir;
    }
    if (!home)
        return;

    char path[512];
    snprintf(path, sizeof(path), "%s/.relojrc", home);

    FILE *f = fopen(path, "r");
    if (!f)
        return;

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        if (line[0] == '#' || line[0] == '\0')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        while (*key == ' ')
            key++;
        while (*val == ' ')
            val++;

        if (strcasecmp(key, "color") == 0)
        {
            int code = color_name_to_ansi256(val);
            if (code >= 0)
                g_digit_color = code;
        }
        else if (strcasecmp(key, "format") == 0)
        {
            if (strcasecmp(val, "12h") == 0)
                g_use_24h = 0;
            else if (strcasecmp(val, "24h") == 0)
                g_use_24h = 1;
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
    if (strcasecmp(distro, "Fedora") == 0) return 39;      // Azul brillante
    if (strcasecmp(distro, "Arch Linux") == 0) return 51;  // Cyan
    if (strcasecmp(distro, "Ubuntu") == 0) return 208;     // Naranja
    if (strcasecmp(distro, "Debian") == 0) return 161;     // Carmesí/Rojo
    if (strcasecmp(distro, "Linux Mint") == 0) return 120; // Verde lima
    if (strcasecmp(distro, "Kali Linux") == 0) return 244; // Gris oscuro/Azul
    
    return 252;
}
static void draw_clock_centered(int term_rows, int term_cols, int hh, int mm, int ss, const struct tm *t, int is_pm)
{
    int digit_w = 5, digit_h = 7, px_w = 2, gap_px = 2;
    int want_seconds = (g_mode == MODO_TEMPORIZADOR) ? 1 : g_show_seconds_big; 
    int n_parts = want_seconds ? 8 : 5;
    int time_w = ((want_seconds ? 6 : 4) * digit_w + (want_seconds ? 2 : 1)) * px_w + (n_parts - 1) * (gap_px * 2);

    if (time_w > term_cols) {
        move_cursor(term_rows / 2, (term_cols - 12) / 2);
        printf("%02d:%02d:%02d", hh, mm, ss);
        return;
    } 

    int top = (term_rows - digit_h) / 2 + 1;
    int left = (term_cols - time_w) / 2 + 1;
    int seq[8] = {hh / 10, hh % 10, -1, mm / 10, mm % 10, -1, ss / 10, ss % 10};
    int limit = want_seconds ? 8 : 5;

    if (g_mode == MODO_TEMPORIZADOR && g_timer_seconds == 0) printf("\033[5m");

    for (int line = 0; line < digit_h; line++) {
        move_cursor(top + line, left);
        for (int p = 0; p < limit; p++) {
            for (int x = 0; x < digit_w; x++) {
                int on = (seq[p] >= 0) ? BIG_MASK[seq[p]][line][x] : (x == 2 && COLON_MASK[line]);
                if (on) printf("\033[48;5;%dm  \033[0m", g_digit_color);
                else printf("  ");
            }
            if (p < limit - 1) printf("  ");
        }
    }
   
    ansi_reset_style();
    int row = top + digit_h + 1;

    if (g_mode == MODO_RELOJ) {
        static const char *months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
        static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        char date_str[64];
        snprintf(date_str, sizeof(date_str), "%s, %s %d %04d", days[t->tm_wday], months[t->tm_mon], t->tm_mday, t->tm_year + 1900);
        
        move_cursor(row++, (term_cols - (int)strlen(date_str)) / 2 + 1);
        printf("\033[38;5;245m%s\033[0m", date_str);
        
        char distro[64];
        get_distro_short(distro, sizeof(distro));
        if (distro[0] != '\0') {
            move_cursor(row++, (term_cols - (int)strlen(distro)) / 2 + 1);
            printf("\033[38;5;240m%s\033[0m", distro);
        }

        if (!g_use_24h) {
            move_cursor(row++, (term_cols - 2) / 2 + 1);
            printf("\033[38;5;245m%s\033[0m", is_pm ? "PM" : "AM");
        }
    } else {
        const char *status = g_timer_running ? "RUN" : "PAUSE";
        if (g_timer_seconds == 0) status = "DONE";
        
        char msg[32];
        snprintf(msg, sizeof(msg), "[ %s ]", status);
        move_cursor(row++, (term_cols - (int)strlen(msg)) / 2 + 1);
        printf("\033[38;5;214m%s\033[0m", msg);
    }
}

static void handle_input(void)
{
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1)
    {
        if (ch == 'q' || ch == 'Q' || ch == 27)
            g_stop = 1;
        if (ch == 's' || ch == 'S')
        {
            g_show_seconds_big = !g_show_seconds_big;
            g_need_clear = 1;
        }
        if (ch == 't' || ch == 'T')
        {
            g_use_24h = !g_use_24h;
            g_need_clear = 1;
        }
        if (ch == 'v' || ch == 'V')
        {
            g_mode = (g_mode == MODO_RELOJ) ? MODO_SET_TIMER : MODO_RELOJ;
            g_need_clear = 1;
        }
        if (ch == ' ' && g_mode == MODO_TEMPORIZADOR)
        {
            g_timer_running = !g_timer_running;
        }
    }
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Uso: clock [opciones]\n");
            printf("Opciones:\n");
            printf("  -12       Modo 12 horas\n");
            printf("  -24       Modo 24 horas\n");
            printf("  --help    Mostrar esta ayuda\n");
            return 0;
        }
        if (strcmp(argv[i], "-12") == 0) {
            g_use_24h = 0;
        }
        if (strcmp(argv[i], "-24") == 0) {
            g_use_24h = 1;
        }
    }

    load_config();
    
    char distro_name[64];
    get_distro_short(distro_name, sizeof(distro_name));
    int d_color = get_color_by_distro(distro_name);
    
    
    if (d_color != 252) {
        g_digit_color = d_color;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGWINCH, on_winch);

    enable_raw_mode();
    ansi_hide_cursor();
    printf("\033[2J");

    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    int last_r = 0, last_c = 0;
    time_t last_tick = time(NULL);

    while (!g_stop)
    {
        int r, c;
        get_terminal_size(&r, &c);

        if (g_need_clear || r != last_r || c != last_c)
        {
            printf("\033[2J");
            g_need_clear = 0;
            last_r = r;
            last_c = c;
        }

       if (g_mode == MODO_SET_TIMER)
        {
            ansi_show_cursor();
            move_cursor(r / 2, (c - 40) / 2);
            /* Pedimos el tiempo de forma más clara */
            printf("\033[1;33mTiempo (HH:MM:SS o MM:SS): \033[0m");
            fflush(stdout);
            
            restore_terminal();
            char buf[32];
            if (fgets(buf, sizeof(buf), stdin)) {
                int h = 0, m = 0, s = 0;
                if (sscanf(buf, "%d:%d:%d", &h, &m, &s) == 3) {
                    g_timer_seconds = (h * 3600) + (m * 60) + s;
                } else if (sscanf(buf, "%d:%d", &m, &s) == 2) {
                    g_timer_seconds = (m * 60) + s;
                } else {
                    g_timer_seconds = atol(buf) * 60;
                }
                
                g_timer_running = 1; 
                g_mode = MODO_TEMPORIZADOR;
            } else {
                g_mode = MODO_RELOJ;
            }
            enable_raw_mode(); 
            ansi_hide_cursor();
            g_need_clear = 1;
            continue;
        }        
        time_t ahora = time(NULL);
        if (ahora > last_tick) {
            if (g_mode == MODO_TEMPORIZADOR && g_timer_running && g_timer_seconds > 0) {
                g_timer_seconds--;
            }
            last_tick = ahora;
        }

        printf("\033[H");
        
        if (g_mode == MODO_RELOJ) {
            struct tm *t = localtime(&ahora);
            int hour = t->tm_hour;
            int is_pm = (hour >= 12);
            if (!g_use_24h) {
                hour = hour % 12;
                if (hour == 0) hour = 12;
            }
            draw_clock_centered(r, c, hour, t->tm_min, t->tm_sec, t, is_pm);
        } else {
            int th = g_timer_seconds / 3600;
            int tm = (g_timer_seconds % 3600) / 60;
            int ts = g_timer_seconds % 60;
            draw_clock_centered(r, c, th, tm, ts, NULL, 0);
        }
        
        fflush(stdout);

        if (poll(&pfd, 1, 100) > 0) {
            handle_input();
        }
    }

    ansi_show_cursor();
    restore_terminal();
    printf("\n");
    return 0;
}
