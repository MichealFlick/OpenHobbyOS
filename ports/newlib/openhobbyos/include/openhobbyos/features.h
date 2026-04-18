#ifndef OPENHOBBYOS_FEATURES_H
#define OPENHOBBYOS_FEATURES_H

/*
 * A few Linux-facing ports probe GNU open flags directly. Newlib doesn't
 * advertise O_PATH for our target, so we reserve the same bit Cygwin uses and
 * translate it when requests cross into the kernel ABI.
 */
#ifndef O_PATH
#define O_PATH 0x02000000
#endif

#endif
