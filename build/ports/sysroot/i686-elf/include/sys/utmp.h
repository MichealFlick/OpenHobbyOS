#ifndef OPENHOBBYOS_SYS_UTMP_H
#define OPENHOBBYOS_SYS_UTMP_H

#include <sys/time.h>
#include <sys/types.h>

#define UT_LINESIZE 32
#define UT_NAMESIZE 32
#define UT_HOSTSIZE 256

#define EMPTY         0
#define RUN_LVL       1
#define BOOT_TIME     2
#define NEW_TIME      3
#define OLD_TIME      4
#define INIT_PROCESS  5
#define LOGIN_PROCESS 6
#define USER_PROCESS  7
#define DEAD_PROCESS  8

struct exit_status {
    short e_termination;
    short e_exit;
};

struct utmp {
    short ut_type;
    pid_t ut_pid;
    char ut_line[UT_LINESIZE];
    char ut_id[4];
    char ut_user[UT_NAMESIZE];
    char ut_host[UT_HOSTSIZE];
    struct exit_status ut_exit;
    long ut_session;
    struct timeval ut_tv;
    int ut_addr_v6[4];
    char ut_unused[20];
};

#ifdef __cplusplus
extern "C" {
#endif

void setutent(void);
void endutent(void);
struct utmp *getutent(void);
void setutxent(void);
void endutxent(void);
struct utmp *getutxent(void);

#ifdef __cplusplus
}
#endif

#endif
