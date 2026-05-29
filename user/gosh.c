#include "runtime.h"
#include "syscall.h"

#define LINE_MAX    1024
#define ARG_MAX     64
#define PATH_MAX    4096
#define HIST_MAX    16
#define THEME_FILE  "/etc/shell-colors.jsonc"
#define MAX(x,y)    ((x)>(y)?(x):(y))

struct gosh_theme {
    int user, host, path, symbol, error, success, default_c, warning, title;
};

static struct gosh_theme g_thm;
static int g_last_exit;
static char g_cwd[PATH_MAX];
static char **g_envp;
static char g_history[HIST_MAX][LINE_MAX];
static int g_hist_count;
static int g_hist_pos;

static int read_byte(void) {
    unsigned char ch = 0;
    int n = sys_read(0, (char*)&ch, 1);
    if (n == 1) return (int)ch;
    return (n == 0) ? -2 : -1;
}

static void write_ch(char ch) { sys_write(1, &ch, 1); }

static void write_str(const char *s) { u_write_buffer(s, u_strlen(s)); }

static int parse_json_int(const char *buf, const char *key, int fallback) {
    const char *p = buf;
    while (*p) {
        const char *k = key;
        const char *scan = p;
        while (*k && *scan && *scan == *k) { k++; scan++; }
        if (!*k) {
            while (*scan && *scan != ':') scan++;
            if (*scan != ':') { p++; continue; }
            scan++;
            while (*scan && (*scan == ' ' || *scan == '\t')) scan++;
            if (*scan == '"') scan++;
            int val = 0;
            while (*scan && *scan >= '0' && *scan <= '9')
                val = val * 10 + (*scan++ - '0');
            return val;
        }
        p++;
    }
    return fallback;
}

static void load_theme(void) {
    g_thm.user = 96; g_thm.host = 92; g_thm.path = 94;
    g_thm.symbol = 97; g_thm.error = 91; g_thm.success = 92;
    g_thm.default_c = 97; g_thm.warning = 93; g_thm.title = 96;

    int fd = sys_open(THEME_FILE, 0, 0);
    if (fd < 0) return;
    char tbuf[2048];
    int total = 0;
    for (;;) {
        int n = sys_read(fd, tbuf + total, 1);
        if (n <= 0) break;
        total++;
        if (total >= (int)sizeof(tbuf) - 1) break;
    }
    sys_close(fd);
    tbuf[total] = '\0';
    g_thm.user = parse_json_int(tbuf, "prompt_user", g_thm.user);
    g_thm.host = parse_json_int(tbuf, "prompt_host", g_thm.host);
    g_thm.path = parse_json_int(tbuf, "prompt_path", g_thm.path);
    g_thm.symbol = parse_json_int(tbuf, "prompt_symbol", g_thm.symbol);
    g_thm.error = parse_json_int(tbuf, "error", g_thm.error);
    g_thm.success = parse_json_int(tbuf, "success", g_thm.success);
    g_thm.default_c = parse_json_int(tbuf, "default", g_thm.default_c);
    g_thm.warning = parse_json_int(tbuf, "warning", g_thm.warning);
    g_thm.title = parse_json_int(tbuf, "title", g_thm.title);
}

static const char *env_get(const char *name) {
    if (!g_envp) return 0;
    unsigned int nlen = u_strlen(name);
    for (int i = 0; g_envp[i]; i++)
        if (u_strncmp(g_envp[i], name, nlen) == 0 && g_envp[i][nlen] == '=')
            return g_envp[i] + nlen + 1;
    return 0;
}

static void write_buf(const char *buf, unsigned int len) { sys_write(1, buf, len); }

static unsigned int emit_sgr(char *buf, int code) {
    unsigned int pos = 0;
    buf[pos++] = 27; buf[pos++] = '[';
    if (code >= 90 && code <= 97) {
        buf[pos++] = '1'; buf[pos++] = ';';
        code -= 60;
    }
    if (code == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[8];
        unsigned int tpos = 0;
        int c = code;
        while (c) { tmp[tpos++] = (char)('0' + (c % 10)); c /= 10; }
        while (tpos) buf[pos++] = tmp[--tpos];
    }
    buf[pos++] = 'm';
    return pos;
}

static unsigned int emit_str(char *buf, const char *s) {
    unsigned int pos = 0;
    while (*s) buf[pos++] = *s++;
    return pos;
}

static void show_prompt(void) {
    const char *user = env_get("USER");
    if (!user) user = "root";
    const char *host = env_get("HOSTNAME");
    if (!host) host = "openhobby";

    if (!sys_getcwd(g_cwd, sizeof(g_cwd)))
        u_strcpy(g_cwd, "?");

    const char *cwd_short = g_cwd;
    const char *slash = 0;
    const char *p = g_cwd;
    while (*p) { if (*p == '/') slash = p; p++; }
    if (slash && slash != g_cwd) cwd_short = slash + 1;

    char prom[512];
    unsigned int pos = 0;

    if (g_last_exit != 0) {
        pos += emit_sgr(prom + pos, g_thm.error);
        char tmp[16]; unsigned int tp = 0; int ec = g_last_exit;
        if (ec == 0) { tmp[tp++] = '0'; }
        else { char rev[8]; unsigned int rp = 0;
            while (ec) { rev[rp++] = (char)('0' + (ec % 10)); ec /= 10; }
            while (rp) tmp[tp++] = rev[--rp]; }
        tmp[tp] = ' '; tmp[tp + 1] = '\0';
        pos += emit_str(prom + pos, tmp);
    }

    pos += emit_sgr(prom + pos, g_thm.user);
    pos += emit_str(prom + pos, user);
    pos += emit_sgr(prom + pos, g_thm.symbol);
    prom[pos++] = '@';
    pos += emit_sgr(prom + pos, g_thm.host);
    pos += emit_str(prom + pos, host);
    pos += emit_sgr(prom + pos, g_thm.symbol);
    prom[pos++] = ':';
    pos += emit_sgr(prom + pos, g_thm.path);
    pos += emit_str(prom + pos, cwd_short);
    pos += emit_sgr(prom + pos, g_thm.symbol);
    prom[pos++] = ' '; prom[pos++] = '#'; prom[pos++] = ' ';
    pos += emit_sgr(prom + pos, 0);

    write_buf(prom, pos);
}

static void hist_add(const char *line) {
    if (!*line) return;
    if (g_hist_count > 0 && u_strcmp(g_history[(g_hist_count-1) % HIST_MAX], line) == 0)
        return;
    u_strcpy(g_history[g_hist_count % HIST_MAX], line);
    g_hist_count++;
    g_hist_pos = g_hist_count;
}

static void hist_draw(const char *buf, int pos) {
    write_str("\r\e[K");
    show_prompt();
    for (int i = 0; i < pos; i++) write_ch(buf[i]);
}

static int read_line(char *buf, int max) {
    int pos = 0;
    buf[0] = '\0';
    for (;;) {
        int ch = read_byte();
        if (ch == -2) { sys_sched_yield(); continue; }
        if (ch < 0) return -1;
        if (ch == '\n' || ch == '\r') {
            write_ch('\n');
            buf[pos] = '\0';
            hist_add(buf);
            return pos;
        }
        if (ch == 127 || ch == 8) {
            if (pos > 0) { pos--; write_str("\b \b"); }
            continue;
        }
        if (ch == 27) {
            int n1 = read_byte();
            if (n1 == '[') {
                int n2 = read_byte();
                if (n2 == 'A') {
                    if (g_hist_pos > 0 && g_hist_count > 0) {
                        g_hist_pos--;
                        int idx = g_hist_pos % HIST_MAX;
                        u_strcpy(buf, g_history[idx]);
                        pos = u_strlen(buf);
                        hist_draw(buf, pos);
                    }
                } else if (n2 == 'B') {
                    if (g_hist_pos < g_hist_count - 1) {
                        g_hist_pos++;
                        int idx = g_hist_pos % HIST_MAX;
                        u_strcpy(buf, g_history[idx]);
                        pos = u_strlen(buf);
                        hist_draw(buf, pos);
                    } else if (g_hist_pos == g_hist_count - 1) {
                        g_hist_pos = g_hist_count;
                        buf[0] = '\0'; pos = 0;
                        hist_draw(buf, pos);
                    }
                }
            }
            continue;
        }
        if (pos >= max - 1) continue;
        if (ch < 32) continue;
        buf[pos++] = (char)ch;
        write_ch((char)ch);
    }
}

static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

static int parse_line(char *line, const char **argv) {
    int argc = 0;
    char *wp = line;
    char *rp = line;
    char quote = 0;

    while (*rp) {
        skip_ws((const char **)&rp);
        if (!*rp) break;
        if (argc >= ARG_MAX - 1) return -1;

        argv[argc++] = wp;
        quote = 0;

        while (*rp) {
            char ch = *rp++;
            if (quote) {
                if (ch == quote) { quote = 0; continue; }
                if (ch == '\\' && quote == '"' && (*rp == '"' || *rp == '\\'))
                    ch = *rp++;
                *wp++ = ch;
                continue;
            }
            if (ch == '"' || ch == '\'') { quote = ch; continue; }
            if (ch == ' ' || ch == '\t') break;
            if (ch == '\\' && *rp) ch = *rp++;
            *wp++ = ch;
        }
        if (quote) return -1;
        *wp++ = '\0';
    }

    argv[argc] = 0;
    return argc;
}

static int find_in_path(const char *cmd, char *resolved, unsigned int size) {
    if (!cmd || !*cmd) return -1;
    if (cmd[0] == '/') {
        unsigned int i = 0;
        while (cmd[i] && i + 1 < size) { resolved[i] = cmd[i]; i++; }
        resolved[i] = '\0';
        return 0;
    }

    const char *path = env_get("PATH");
    if (!path) path = "/bin:/usr/bin";

    char path_copy[PATH_MAX];
    unsigned int pi = 0;
    while (*path && pi + 1 < sizeof(path_copy)) path_copy[pi++] = *path++;
    path_copy[pi] = '\0';

    char *start = path_copy;
    while (*start) {
        char *end = start;
        while (*end && *end != ':') end++;
        char sep = *end;
        *end = '\0';

        unsigned int si = 0;
        while (start[si] && si + 1 < size) { resolved[si] = start[si]; si++; }
        if (si > 0 && resolved[si - 1] != '/') {
            if (si + 1 < size) resolved[si++] = '/';
        }
        unsigned int ci = 0;
        while (cmd[ci] && si + 1 < size) { resolved[si++] = cmd[ci++]; }
        resolved[si] = '\0';

        struct linux_stat64 st;
        if (sys_stat64(resolved, &st) == 0 && (st.st_mode & 0111))
            return 0;

        if (!sep) break;
        start = end + 1;
    }
    return -1;
}

static int builtin_cd(int argc, const char **argv) {
    (void)argc;
    const char *target = argv[1] ? argv[1] : "/";
    if (sys_chdir(target) < 0) {
        write_str("gosh: cd: "); write_str(target);
        write_str(": No such file or directory\n");
        return 1;
    }
    return 0;
}

static int builtin_exit(int argc, const char **argv) {
    int code = 0;
    if (argc > 1) { int ok = 0; code = (int)u_parse_uint(argv[1], &ok);
        if (!ok) code = 0; }
    g_last_exit = -1;
    return code;
}

static int builtin_help(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("GOSH! - OpenHobbyOS Shell\n");
    write_str("Built-in commands:\n");
    write_str("  cd <dir>     Change directory\n");
    write_str("  clear        Clear the screen\n");
    write_str("  echo <text>  Print text\n");
    write_str("  exit [code]  Exit the shell\n");
    write_str("  help         Display this help\n");
    write_str("  history      Show command history\n");
    write_str("  pwd          Print working directory\n");
    write_str("  export       Print environment variables\n");
    return 0;
}

static int builtin_clear(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("\e[H\e[J");
    return 0;
}

static int builtin_pwd(int argc, const char **argv) {
    (void)argc; (void)argv;
    if (sys_getcwd(g_cwd, sizeof(g_cwd))) {
        write_str(g_cwd); write_ch('\n');
    }
    return 0;
}

static int builtin_echo(int argc, const char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) write_ch(' ');
        write_str(argv[i]);
    }
    write_ch('\n');
    return 0;
}

static int builtin_export(int argc, const char **argv) {
    (void)argv;
    if (argc == 1) {
        for (int i = 0; g_envp && g_envp[i]; i++) {
            write_str(g_envp[i]); write_ch('\n');
        }
        return 0;
    }
    write_str("gosh: export: setting variables not supported\n");
    return 1;
}

static int builtin_history(int argc, const char **argv) {
    (void)argc; (void)argv;
    int start = MAX(0, g_hist_count - HIST_MAX);
    for (int i = start; i < g_hist_count; i++) {
        char num[12]; unsigned int np = 0; int n = i + 1;
        char rev[8]; unsigned int rp = 0;
        while (n) { rev[rp++] = (char)('0' + (n % 10)); n /= 10; }
        while (rp) num[np++] = rev[--rp];
        num[np] = '\0';
        write_str(num); write_str("  "); write_str(g_history[i % HIST_MAX]); write_ch('\n');
    }
    return 0;
}

struct builtin {
    const char *name;
    int (*func)(int argc, const char **argv);
};

static const struct builtin builtins[] = {
    {"cd",      builtin_cd},
    {"clear",   builtin_clear},
    {"echo",    builtin_echo},
    {"exit",    builtin_exit},
    {"export",  builtin_export},
    {"help",    builtin_help},
    {"history", builtin_history},
    {"pwd",     builtin_pwd},
    {0, 0}
};

static int run_builtin(const char *name, int argc, const char **argv) {
    for (const struct builtin *b = builtins; b->name; b++)
        if (u_strcmp(name, b->name) == 0) return b->func(argc, argv);
    return -1;
}

static int execute_external(const char *path, const char **argv) {
    int pid = sys_spawn(path, argv);
    if (pid < 0) {
        write_str("gosh: "); write_str(path); write_str(": command not found\n");
        return 127;
    }
    int status = 0;
    if (sys_waitpid(pid, &status, 0) < 0) return 127;
    return status;
}

static void execute_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') return;

    const char *argv[ARG_MAX];
    int argc = parse_line(line, argv);
    if (argc <= 0) return;

    char resolved[PATH_MAX];
    if (find_in_path(argv[0], resolved, sizeof(resolved)) == 0) {
        g_last_exit = execute_external(resolved, argv);
        return;
    }

    int r = run_builtin(argv[0], argc, argv);
    if (r >= 0) { g_last_exit = r; return; }

    write_str("gosh: "); write_str(argv[0]); write_str(": command not found\n");
    g_last_exit = 127;
}

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv;
    g_envp = envp;
    g_last_exit = 0;
    g_hist_count = 0;
    g_hist_pos = 0;

    load_theme();
    write_str("GOSH! v2.0 - OpenHobbyOS Shell\n\n");

    char line[LINE_MAX];

    for (;;) {
        show_prompt();
        int len = read_line(line, sizeof(line));
        if (len < 0) break;
        execute_line(line);
        if (g_last_exit < 0) break;
    }

    return (g_last_exit == -1) ? 0 : g_last_exit;
}
