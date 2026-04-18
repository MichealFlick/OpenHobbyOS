#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

static long oh_read_uptime_seconds(void) {
    FILE *fp = fopen("/proc/uptime", "r");
    double seconds = 0.0;

    if (fp == NULL) {
        return 0;
    }

    if (fscanf(fp, "%lf", &seconds) != 1) {
        seconds = 0.0;
    }
    fclose(fp);
    return (long) seconds;
}

static void oh_read_loads(unsigned long loads[3]) {
    FILE *fp = fopen("/proc/loadavg", "r");
    double values[3] = { 0.0, 0.0, 0.0 };

    loads[0] = loads[1] = loads[2] = 0;
    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%lf %lf %lf", &values[0], &values[1], &values[2]) == 3) {
        for (int i = 0; i < 3; ++i) {
            loads[i] = (unsigned long) (values[i] * (double) (1 << SI_LOAD_SHIFT));
        }
    }
    fclose(fp);
}

static void oh_read_meminfo(unsigned long *total, unsigned long *free_mem, unsigned long *shared, unsigned long *buffered) {
    FILE *fp = fopen("/proc/meminfo", "r");
    char key[64];
    unsigned long value;
    char unit[16];

    *total = 0;
    *free_mem = 0;
    *shared = 0;
    *buffered = 0;

    if (fp == NULL) {
        return;
    }

    while (fscanf(fp, "%63[^:]: %lu %15s\n", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal") == 0) {
            *total = value * 1024u;
        } else if (strcmp(key, "MemFree") == 0) {
            *free_mem = value * 1024u;
        } else if (strcmp(key, "Shmem") == 0) {
            *shared = value * 1024u;
        } else if (strcmp(key, "Buffers") == 0) {
            *buffered = value * 1024u;
        }
    }

    fclose(fp);
}

int sysinfo(struct sysinfo *info) {
    if (info == NULL) {
        errno = EFAULT;
        return -1;
    }

    memset(info, 0, sizeof(*info));
    info->uptime = oh_read_uptime_seconds();
    oh_read_loads(info->loads);
    oh_read_meminfo(&info->totalram, &info->freeram, &info->sharedram, &info->bufferram);
    info->mem_unit = 1u;
    info->procs = 1u;
    return 0;
}

static int oh_count_processors(void) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[128];
    int count = 0;

    if (fp == NULL) {
        return 1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) {
            count++;
        }
    }
    fclose(fp);
    return count > 0 ? count : 1;
}

int get_nprocs(void) {
    return oh_count_processors();
}

int get_nprocs_conf(void) {
    return oh_count_processors();
}
