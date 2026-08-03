#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define MAX_ENTRIES 4096
#define MAX_NAME    256

typedef struct {
    char      name[MAX_NAME];
    int       is_dir;
    long long size;
} Entry;

static int compare_entries(const void *a, const void *b)
{
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;

    if (ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;

    return strcasecmp(ea->name, eb->name);
}

static int read_dir(const char *path, Entry *out, int max)
{
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

static int print_dir(const char *path)
{
    Entry entries[MAX_ENTRIES];
    int count = read_dir(path, entries, MAX_ENTRIES);

    if (count < 0) {
        fprintf(stderr, "Error: '%s': %s\n", path, strerror(errno));
        return -1;
    }

    qsort(entries, count, sizeof(Entry), compare_entries);

    printf("%s\n\n", path);

    if (strcmp(path, "/") != 0)
        printf("  %-40s %8s\n", "/", "<ROOT>");

    for (int i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            printf("  %-40s %8s\n", entries[i].name, "<DIR>");
        } else {
            char size[16];
            format_size(entries[i].size, size, sizeof(size));
            printf("  %-40s %8s\n", entries[i].name, size);
        }
    }

    printf("\n%d items\n", count);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && chdir(argv[1]) != 0) {
        fprintf(stderr, "Error: '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    char cwd[PATH_MAX];
    char line[512];

    while (1) {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "Error getcwd: %s\n", strerror(errno));
            return 1;
        }

        print_dir(cwd);

        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0')
            continue;

        if (strcmp(line, "q") == 0)
            break;

        if (chdir(line) != 0)
            fprintf(stderr, "Path '%s' error: %s\n", line, strerror(errno));
    }

    return 0;
}