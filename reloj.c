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
static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_need_clear = 0;
static volatile sig_atomic_t g_show_seconds_big = 0;

static int g_digit_color = 252;
static int g_use_24h = 1;
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

static void draw_clock_centered(int term_rows, int term_cols, int hh, int mm, int ss,
                                const struct tm *t, int is_pm)
{
    int digit_w = 5, digit_h = 7, px_w = 2, gap_px = 2;
    int want_seconds = g_show_seconds_big;

    int n_parts = want_seconds ? 8 : 5;
    int time_w = ((want_seconds ? 6 : 4) * digit_w + (want_seconds ? 2 : 1)) * px_w + (n_parts - 1) * (gap_px * 2);

    if (time_w > term_cols)
    {
        move_cursor(term_rows / 2, (term_cols - 8) / 2);
        printf("%02d:%02d:%02d", hh, mm, ss);
        return;
    }

    int top = (term_rows - digit_h) / 2 + 1;
    int left = (term_cols - time_w) / 2 + 1;

    int seq[8] = {hh / 10, hh % 10, -1, mm / 10, mm % 10, -1, ss / 10, ss % 10};
    int limit = want_seconds ? 8 : 5;

    for (int line = 0; line < digit_h; line++)
    {
        move_cursor(top + line, left);
        for (int p = 0; p < limit; p++)
        {
            for (int x = 0; x < digit_w; x++)
            {
                int on = (seq[p] >= 0) ? BIG_MASK[seq[p]][line][x] : (x == 2 && COLON_MASK[line]);
                if (on)
                    printf("\033[48;5;%dm  \033[0m", g_digit_color);
                else
                    printf("  ");
            }
            if (p < limit - 1)
                printf("  ");
        }
    }

    static const char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"};
    static const char *days[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    char date_str[64];
    snprintf(date_str, sizeof(date_str), "%s, %s %d %04d",
             days[t->tm_wday], months[t->tm_mon], t->tm_mday, t->tm_year + 1900);
    int date_len = (int)strlen(date_str);

    char distro[64];
    get_distro_short(distro, sizeof(distro));
    int dlen = (int)strlen(distro);

    int row = top + digit_h + 1;

    if (!want_seconds)
    {
        move_cursor(row, left + (time_w / 2) - 3);
        printf("\033[38;5;245mSS: %02d\033[0m", ss);
        row++;
    }

    if (date_len > 0)
    {
        move_cursor(row, (term_cols - date_len) / 2 + 1);
        printf("\033[38;5;245m%s\033[0m", date_str);
        row++;
    }

    if (dlen > 0)
    {
        move_cursor(row, (term_cols - dlen) / 2 + 1);
        printf("\033[38;5;240m%s\033[0m", distro);
    }
    if (!g_use_24h)
    {
        const char *ampm = is_pm ? "PM" : "AM";
        move_cursor(row + 1, (term_cols - 2) / 2 + 1);
        printf("\033[38;5;245m%s\033[0m", ampm);
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
    }
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf(
                "reloj - reloj de terminal\n\n"
                "Uso:\n"
                "  reloj [opciones]\n\n"
                "Opciones:\n"
                "  -h, --help        Muestra esta ayuda\n"
                "  -12               Usa formato de 12 horas\n"
                "  -24               Usa formato de 24 horas (por defecto)\n\n"
                "Controles (en ejecución):\n"
                "  q / ESC           Salir\n"
                "  s                 Alternar segundos\n"
                "  t                 Alternar 12h/24h\n");
            return 0;
        }

        if (strcmp(argv[i], "-12") == 0)
            g_use_24h = 0;

        if (strcmp(argv[i], "-24") == 0)
            g_use_24h = 1;
    }

    load_config();
    struct sigaction sa = {.sa_handler = on_signal};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sw = {.sa_handler = on_winch};
    sigaction(SIGWINCH, &sw, NULL);

    if (isatty(STDIN_FILENO))
        enable_raw_mode();
    atexit(restore_terminal);

    printf("\033[2J\033[H\033[?25l");

    int last_r = 0, last_c = 0;

    while (!g_stop)
    {
        time_t ahora = time(NULL);
        struct tm *t = localtime(&ahora);
        int hour = t->tm_hour;
        int is_pm = 0;

        if (!g_use_24h)
        {
            is_pm = (hour >= 12);
            hour = hour % 12;
            if (hour == 0)
                hour = 12;
        }

        int r, c;
        get_terminal_size(&r, &c);

        if (g_need_clear || r != last_r || c != last_c)
        {
            printf("\033[2J");
            g_need_clear = 0;
            last_r = r;
            last_c = c;
        }
        printf("\033[H");

        draw_clock_centered(r, c, hour, t->tm_min, t->tm_sec, t, is_pm);
        fflush(stdout);

        handle_input();
        sleep(1);
    }

    return 0;
}