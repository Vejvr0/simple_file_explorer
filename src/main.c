#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <ncurses.h>
#include <wchar.h>
#include <locale.h>

#define MAX_ENTRIES 4096
#define MAX_NAME    256
#define PAGE_STEP   10

typedef struct {
    char      name[MAX_NAME];
    int       is_dir;
    int       is_root;
    long long size;
} Entry;

static int compare_entries(const void *a, const void *b) {
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;

    if (ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;

    return strcasecmp(ea->name, eb->name);
}

static int read_dir(const char *path, Entry *out, int max) {
    DIR *dir = opendir(path);
    if (dir == NULL)
        return -1;

    int count = 0;
    struct dirent *de;

    while ((de = readdir(dir)) != NULL && count < max) {

        if (strcmp(de->d_name, ".") == 0)
            continue;

        Entry *e = &out[count];

        strncpy(e->name, de->d_name, MAX_NAME - 1);
        e->name[MAX_NAME - 1] = '\0';
        e->is_root = 0;

        char full[MAX_NAME * 4];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);

        struct stat st;
        if (lstat(full, &st) == 0) {
            e->is_dir = S_ISDIR(st.st_mode);
            e->size   = (long long)st.st_size;
        } else {
            e->is_dir = 0;
            e->size   = 0;
        }

        count++;
    }

    closedir(dir);
    return count;
}

static void format_size(long long bytes, char *buf, size_t bufsize) {
    const char *units[] = { "B", "K", "M", "G", "T" };
    double value = (double)bytes;
    int unit = 0;

    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }

    if (unit == 0)
        snprintf(buf, bufsize, "%lld%s", bytes, units[unit]);
    else
        snprintf(buf, bufsize, "%.1f%s", value, units[unit]);
}

static int load_dir(const char *path, Entry *out, int max) {
    int base = 0;

    if (strcmp(path, "/") != 0) {
        strncpy(out[0].name, "/", MAX_NAME - 1);
        out[0].name[MAX_NAME - 1] = '\0';
        out[0].is_dir  = 1;
        out[0].is_root = 1;
        out[0].size    = 0;
        base = 1;
    }

    int count = read_dir(path, out + base, max - base);
    if (count < 0)
        return -1;

    qsort(out + base, count, sizeof(Entry), compare_entries);
    return count + base;
}

static void Create(char *status, size_t max_len, const char *path, const char *name) {
    int check;

    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

    check = mkdir(fullpath, 0777);

    if (check) {
        snprintf(status, max_len, "Create folder error '%s'", name);
    } else {
        snprintf(status, max_len, "Folder '%s' was create", name);
    }

    refresh();
}

static void Remove(char *status, size_t max_len, const char *path, const char *name) {
    if (strcmp(name, "/") == 0 || strcmp(name, "..") == 0) {
        snprintf(status, max_len, "'%s' cannot be removed.", name);
        return;
    }

    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

    if (remove(fullpath) == 0) {
        snprintf(status, max_len, "'%s' was removed.", name);
    } else {
        snprintf(status, max_len, "Remove error '%s'", name);
    }

    refresh();
}

static int run_capture(char *const argv[], char *out, size_t outsz) {
    int fd[2];
    if (pipe(fd) != 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    if (pid == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(fd[1]);

    size_t len = 0;
    ssize_t n;
    while (len + 1 < outsz && (n = read(fd[0], out + len, outsz - 1 - len)) > 0)
        len += (size_t)n;
    out[len] = '\0';
    close(fd[0]);

    int wstatus;
    waitpid(pid, &wstatus, 0);

    if (len > 0 && out[len - 1] == '\n')
        out[len - 1] = '\0';

    return (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) ? 0 : -1;
}

static int run_interactive(char *const argv[]) {
    def_prog_mode();
    endwin();

    int result = -1;
    pid_t pid = fork();

    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    } else if (pid > 0) {
        int wstatus;
        waitpid(pid, &wstatus, 0);
        if (WIFEXITED(wstatus))
            result = WEXITSTATUS(wstatus);
    }

    reset_prog_mode();
    refresh();

    return result;
}

static void Open(char *status, size_t max_len, const char *path, const char *name) {
    char fullpath[PATH_MAX];
    char mime[128];

    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

    char *file_argv[] = { "file", "--mime-type", "-b", fullpath, NULL };
    if (run_capture(file_argv, mime, sizeof(mime)) != 0) {
        snprintf(status, max_len, "Cannot detect type of '%s'", name);
        return;
    }

    if (strncmp(mime, "text/", 5) == 0 || strcmp(mime, "application/json") == 0 ||
        strcmp(mime, "inode/x-empty") == 0) {
        char *editor_argv[] = { "sh", "-c", "${EDITOR:-nano} \"$1\"", "sh", fullpath, NULL };
        int rc = run_interactive(editor_argv);

        if (rc == 0)
            snprintf(status, max_len, "Closed '%s'", name);
        else
            snprintf(status, max_len, "Editor error for '%s' (exit %d)", name, rc);
    } else {
        snprintf(status, max_len, "Unsupported file type: %s", mime);
    }
}

static void save_cwd(const char *cwd) {
    const char *target = getenv("EXPLORER_CWD_FILE");
    if (target == NULL || target[0] == '\0' || cwd[0] == '\0')
        return;

    FILE *fp = fopen(target, "w");
    if (fp == NULL)
        return;

    fprintf(fp, "%s", cwd);
    fclose(fp);
}

static void fit_text(const char *src, char *dst, size_t dstsize, int width) {
    mbstate_t ps;
    size_t si = 0, di = 0;
    size_t slen = strlen(src);
    int shown = 0;

    memset(&ps, 0, sizeof(ps));

    while (src[si] != '\0' && shown < width) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, src + si, slen - si, &ps);
        int w;

        if (n == (size_t)-1 || n == (size_t)-2) {
            memset(&ps, 0, sizeof(ps));
            if (di + 1 >= dstsize)
                break;
            dst[di++] = '?';
            shown++;
            si++;
            continue;
        }

        if (n == 0)
            break;

        w = wcwidth(wc);

        if (w < 0) {
            if (di + 1 >= dstsize)
                break;
            dst[di++] = '?';
            shown++;
            si += n;
            continue;
        }

        if (shown + w > width)
            break;

        if (di + n >= dstsize)
            break;

        memcpy(dst + di, src + si, n);
        di += n;
        si += n;
        shown += w;
    }

    while (shown < width && di + 1 < dstsize) {
        dst[di++] = ' ';
        shown++;
    }

    dst[di] = '\0';
}

static void draw_line(int y, int width, int color, const char *text, int attr) {
    char buf[4096];

    fit_text(text, buf, sizeof(buf), width);

    if (attr) attron(attr);

    if (color == 0) {
        mvprintw(y, 0, "%s", buf);
    } else {
        attron(COLOR_PAIR(color));
        mvprintw(y, 0, "%s", buf);
        attroff(COLOR_PAIR(color));
    }

    if (attr) attroff(attr);
}

static void draw(const char *path, const Entry *entries, int count, int sel, int top, const char *status) {
    int rows, cols;

    erase();
    getmaxyx(stdscr, rows, cols);

    int list_rows = rows - 4;
    if (list_rows < 1)
        list_rows = 1;

    int name_w = cols - 14;
    if (name_w < 10)
        name_w = 10;

    draw_line(0, cols - 1, 1, path, A_BOLD);

    for (int i = 0; i < list_rows && top + i < count; i++) {
        const Entry *e = &entries[top + i];
        const char *right;
        char size[16];
        char name[MAX_NAME * 4 + 8];
        char row[MAX_NAME * 4 + 64];

        if (e->is_root)
            right = "<ROOT>";
        else if (e->is_dir)
            right = "<DIR>";
        else {
            format_size(e->size, size, sizeof(size));
            right = size;
        }

        fit_text(e->name, name, sizeof(name), name_w);
        snprintf(row, sizeof(row), "  %s %8s", name, right);

        draw_line(2 + i, cols - 1, 2, row, (top + i == sel) ? A_REVERSE : 0);
    }

    if (status[0] != '\0')
        draw_line(rows - 2, cols - 1, 3, status, A_BOLD);

    char help[256];
    snprintf(
        help,
        sizeof(help),
        "[Arrows] move | [Enter] open | [q] quit | [r] delete | [a] create |  Items: %d/%d",
        count > 0 ? sel + 1 : 0, count
    );

    draw_line(rows - 1, cols - 1, 2, help, A_REVERSE);

    refresh();
}

int main(int argc, char *argv[]) {
    if (argc > 1 && chdir(argv[1]) != 0) {
        fprintf(stderr, "Error: '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    Entry *entries = malloc(sizeof(Entry) * MAX_ENTRIES);
    if (entries == NULL) {
        fprintf(stderr, "Error: No more memory. \n");
        return 1;
    }

    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors() == FALSE) {
        endwin();
        free(entries);
        printf("Error: Terminal does not support colors!");
        return 1;
    }

    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_WHITE);
    init_pair(3, COLOR_WHITE, COLOR_RED);

    char cwd[PATH_MAX];
    char status[256] = "";
    int count = 0, sel = 0, top = 0;
    int reload = 1;
    int running = 1;

    while (running) {

        if (reload) {
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                snprintf(status, sizeof(status), "Error getcwd: %s", strerror(errno));
                cwd[0] = '\0';
            }

            count = load_dir(cwd, entries, MAX_ENTRIES);
            if (count < 0) {
                snprintf(status, sizeof(status), "Cannot read '%s': %s",
                         cwd, strerror(errno));
                count = 0;
            }

            top = 0;
            reload = 0;

            refresh();
        }

        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        (void)cols;

        int list_rows = rows - 4;
        if (list_rows < 1) list_rows = 1;

        if (sel < top) top = sel;
        if (sel >= top + list_rows) top = sel - list_rows + 1;

        draw(cwd, entries, count, sel, top, status);

        int ch = getch();

        switch (ch) {

        case KEY_UP:
            if (sel > 0) sel--;
            reload = 1;
            break;

        case KEY_DOWN:
            if (sel < count - 1) sel++;
            reload = 1;
            break;

        case KEY_PPAGE:
            sel -= PAGE_STEP;
            if (sel < 0) sel = 0;
            reload = 1;
            break;

        case KEY_NPAGE:
            sel += PAGE_STEP;
            if (sel > count - 1) sel = count - 1;
            if (sel < 0) sel = 0;
            reload = 1;
            break;

        case KEY_HOME:
            sel = 0;
            reload = 1;
            break;

        case KEY_END:
            sel = (count > 0) ? count - 1 : 0;
            reload = 1;
            break;

        case KEY_ENTER:
        case '\n':
        case '\r':
            if (count > 0) {
                const Entry *e = &entries[sel];

                if (!e->is_dir) {
                    Open(status, sizeof(status), cwd, e->name);
                } else if (chdir(e->name) != 0) {
                    snprintf(status, sizeof(status), "Cannot access to '%s': %s", e->name, strerror(errno));
                } else {
                    status[0] = '\0';
                    reload = 1;
                    sel = 0;
                }
            }
            break;

        case 'r':
        case 'R':
            if (count > 0) {
                const Entry *e = &entries[sel];
                Remove(status, sizeof(status), cwd, e->name);
                reload = 1;
            }
            break;

        case 'a':
        case 'A':
            Create(status, sizeof(status), cwd, "new_folder");
            reload = 1;
            break;

        case KEY_RESIZE:
            break;

        case 'q':
        case 'Q':
            running = 0;
            break;
        }
    }

    endwin();
    save_cwd(cwd);
    free(entries);

    return 0;
}
