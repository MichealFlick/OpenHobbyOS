#ifndef OPENHOBBYOS_SYS_SYSINFO_H
#define OPENHOBBYOS_SYS_SYSINFO_H

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
};

#define SI_LOAD_SHIFT 16

int sysinfo(struct sysinfo *info);
int get_nprocs(void);
int get_nprocs_conf(void);

#endif
