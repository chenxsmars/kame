/*	$NetBSD: sunos_syscalls.c,v 1.59 2000/04/09 06:49:17 mrg Exp $	*/

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.54 2000/04/09 06:47:39 mrg Exp 
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_nfsserver.h"
#include "opt_sysv.h"
#include "fs_nfs.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/syscallargs.h>
#include <compat/sunos/sunos.h>
#include <compat/sunos/sunos_syscallargs.h>
#endif /* _KERNEL && ! _LKM */

char *sunos_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"wait4",			/* 7 = wait4 */
	"creat",			/* 8 = creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execv",			/* 11 = execv */
	"chdir",			/* 12 = chdir */
	"#13 (obsolete old_time)",		/* 13 = obsolete old_time */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"#18 (obsolete old_stat)",		/* 18 = obsolete old_stat */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"#21 (obsolete sunos_old_mount)",		/* 21 = obsolete sunos_old_mount */
	"#22 (unimplemented System V umount)",		/* 22 = unimplemented System V umount */
	"setuid",			/* 23 = setuid */
	"getuid",			/* 24 = getuid */
	"stime",			/* 25 = stime */
	"ptrace",			/* 26 = ptrace */
	"#27 (unimplemented old_sunos_alarm)",		/* 27 = unimplemented old_sunos_alarm */
	"#28 (unimplemented old_sunos_fstat)",		/* 28 = unimplemented old_sunos_fstat */
	"#29 (unimplemented old_sunos_pause)",		/* 29 = unimplemented old_sunos_pause */
	"#30 (unimplemented old_sunos_utime)",		/* 30 = unimplemented old_sunos_utime */
	"#31 (unimplemented old_sunos_stty)",		/* 31 = unimplemented old_sunos_stty */
	"#32 (unimplemented old_sunos_gtty)",		/* 32 = unimplemented old_sunos_gtty */
	"access",			/* 33 = access */
	"#34 (unimplemented old_sunos_nice)",		/* 34 = unimplemented old_sunos_nice */
	"#35 (unimplemented old_sunos_ftime)",		/* 35 = unimplemented old_sunos_ftime */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"stat",			/* 38 = stat */
	"#39 (unimplemented sunos_setpgrp)",		/* 39 = unimplemented sunos_setpgrp */
	"lstat",			/* 40 = lstat */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"#43 (unimplemented sunos_times)",		/* 43 = unimplemented sunos_times */
	"profil",			/* 44 = profil */
	"#45 (unimplemented)",		/* 45 = unimplemented */
	"setgid",			/* 46 = setgid */
	"getgid",			/* 47 = getgid */
	"#48 (unimplemented sunos_ssig)",		/* 48 = unimplemented sunos_ssig */
	"#49 (unimplemented reserved for USG)",		/* 49 = unimplemented reserved for USG */
	"#50 (unimplemented reserved for USG)",		/* 50 = unimplemented reserved for USG */
	"acct",			/* 51 = acct */
	"#52 (unimplemented)",		/* 52 = unimplemented */
	"mctl",			/* 53 = mctl */
	"ioctl",			/* 54 = ioctl */
	"reboot",			/* 55 = reboot */
	"#56 (obsolete sunos_owait3)",		/* 56 = obsolete sunos_owait3 */
	"symlink",			/* 57 = symlink */
	"readlink",			/* 58 = readlink */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"fstat",			/* 62 = fstat */
	"#63 (unimplemented)",		/* 63 = unimplemented */
	"getpagesize",			/* 64 = getpagesize */
	"omsync",			/* 65 = omsync */
	"vfork",			/* 66 = vfork */
	"#67 (obsolete vread)",		/* 67 = obsolete vread */
	"#68 (obsolete vwrite)",		/* 68 = obsolete vwrite */
	"sbrk",			/* 69 = sbrk */
	"sstk",			/* 70 = sstk */
	"mmap",			/* 71 = mmap */
	"vadvise",			/* 72 = vadvise */
	"munmap",			/* 73 = munmap */
	"mprotect",			/* 74 = mprotect */
	"madvise",			/* 75 = madvise */
	"vhangup",			/* 76 = vhangup */
	"#77 (unimplemented vlimit)",		/* 77 = unimplemented vlimit */
	"mincore",			/* 78 = mincore */
	"getgroups",			/* 79 = getgroups */
	"setgroups",			/* 80 = setgroups */
	"getpgrp",			/* 81 = getpgrp */
	"setpgrp",			/* 82 = setpgrp */
	"setitimer",			/* 83 = setitimer */
	"#84 (unimplemented { int sunos_sys_wait ( void ) ; })",		/* 84 = unimplemented { int sunos_sys_wait ( void ) ; } */
	"swapon",			/* 85 = swapon */
	"getitimer",			/* 86 = getitimer */
	"gethostname",			/* 87 = gethostname */
	"sethostname",			/* 88 = sethostname */
	"getdtablesize",			/* 89 = getdtablesize */
	"dup2",			/* 90 = dup2 */
	"#91 (unimplemented getdopt)",		/* 91 = unimplemented getdopt */
	"fcntl",			/* 92 = fcntl */
	"select",			/* 93 = select */
	"#94 (unimplemented setdopt)",		/* 94 = unimplemented setdopt */
	"fsync",			/* 95 = fsync */
	"setpriority",			/* 96 = setpriority */
	"socket",			/* 97 = socket */
	"connect",			/* 98 = connect */
	"accept",			/* 99 = accept */
	"getpriority",			/* 100 = getpriority */
	"send",			/* 101 = send */
	"recv",			/* 102 = recv */
	"#103 (unimplemented old socketaddr)",		/* 103 = unimplemented old socketaddr */
	"bind",			/* 104 = bind */
	"setsockopt",			/* 105 = setsockopt */
	"listen",			/* 106 = listen */
	"#107 (unimplemented vtimes)",		/* 107 = unimplemented vtimes */
	"sigvec",			/* 108 = sigvec */
	"sigblock",			/* 109 = sigblock */
	"sigsetmask",			/* 110 = sigsetmask */
	"sigsuspend",			/* 111 = sigsuspend */
	"sigstack",			/* 112 = sigstack */
	"recvmsg",			/* 113 = recvmsg */
	"sendmsg",			/* 114 = sendmsg */
	"#115 (obsolete vtrace)",		/* 115 = obsolete vtrace */
	"gettimeofday",			/* 116 = gettimeofday */
	"getrusage",			/* 117 = getrusage */
	"getsockopt",			/* 118 = getsockopt */
	"#119 (unimplemented)",		/* 119 = unimplemented */
	"readv",			/* 120 = readv */
	"writev",			/* 121 = writev */
	"settimeofday",			/* 122 = settimeofday */
	"fchown",			/* 123 = fchown */
	"fchmod",			/* 124 = fchmod */
	"recvfrom",			/* 125 = recvfrom */
	"setreuid",			/* 126 = setreuid */
	"setregid",			/* 127 = setregid */
	"rename",			/* 128 = rename */
	"truncate",			/* 129 = truncate */
	"ftruncate",			/* 130 = ftruncate */
	"flock",			/* 131 = flock */
	"#132 (unimplemented)",		/* 132 = unimplemented */
	"sendto",			/* 133 = sendto */
	"shutdown",			/* 134 = shutdown */
	"socketpair",			/* 135 = socketpair */
	"mkdir",			/* 136 = mkdir */
	"rmdir",			/* 137 = rmdir */
	"utimes",			/* 138 = utimes */
	"sigreturn",			/* 139 = sigreturn */
	"adjtime",			/* 140 = adjtime */
	"getpeername",			/* 141 = getpeername */
	"gethostid",			/* 142 = gethostid */
	"#143 (unimplemented old sethostid)",		/* 143 = unimplemented old sethostid */
	"getrlimit",			/* 144 = getrlimit */
	"setrlimit",			/* 145 = setrlimit */
	"killpg",			/* 146 = killpg */
	"#147 (unimplemented)",		/* 147 = unimplemented */
	"#148 (unimplemented)",		/* 148 = unimplemented */
	"#149 (unimplemented)",		/* 149 = unimplemented */
	"getsockname",			/* 150 = getsockname */
	"#151 (unimplemented getmsg)",		/* 151 = unimplemented getmsg */
	"#152 (unimplemented putmsg)",		/* 152 = unimplemented putmsg */
	"poll",			/* 153 = poll */
	"#154 (unimplemented)",		/* 154 = unimplemented */
#ifdef NFSSERVER
	"nfssvc",			/* 155 = nfssvc */
#else
	"#155 (unimplemented)",		/* 155 = unimplemented */
#endif
	"getdirentries",			/* 156 = getdirentries */
	"statfs",			/* 157 = statfs */
	"fstatfs",			/* 158 = fstatfs */
	"unmount",			/* 159 = unmount */
#ifdef NFS
	"async_daemon",			/* 160 = async_daemon */
	"getfh",			/* 161 = getfh */
#else
	"#160 (unimplemented)",		/* 160 = unimplemented */
	"#161 (unimplemented)",		/* 161 = unimplemented */
#endif
	"getdomainname",			/* 162 = getdomainname */
	"setdomainname",			/* 163 = setdomainname */
	"#164 (unimplemented rtschedule)",		/* 164 = unimplemented rtschedule */
	"quotactl",			/* 165 = quotactl */
	"exportfs",			/* 166 = exportfs */
	"mount",			/* 167 = mount */
	"ustat",			/* 168 = ustat */
#ifdef SYSVSEM
	"semsys",			/* 169 = semsys */
#else
	"#169 (unimplemented semsys)",		/* 169 = unimplemented semsys */
#endif
#ifdef SYSVMSG
	"msgsys",			/* 170 = msgsys */
#else
	"#170 (unimplemented msgsys)",		/* 170 = unimplemented msgsys */
#endif
#ifdef SYSVSHM
	"shmsys",			/* 171 = shmsys */
#else
	"#171 (unimplemented shmsys)",		/* 171 = unimplemented shmsys */
#endif
	"auditsys",			/* 172 = auditsys */
	"#173 (unimplemented rfssys)",		/* 173 = unimplemented rfssys */
	"getdents",			/* 174 = getdents */
	"setsid",			/* 175 = setsid */
	"fchdir",			/* 176 = fchdir */
	"fchroot",			/* 177 = fchroot */
	"#178 (unimplemented vpixsys)",		/* 178 = unimplemented vpixsys */
	"#179 (unimplemented aioread)",		/* 179 = unimplemented aioread */
	"#180 (unimplemented aiowrite)",		/* 180 = unimplemented aiowrite */
	"#181 (unimplemented aiowait)",		/* 181 = unimplemented aiowait */
	"#182 (unimplemented aiocancel)",		/* 182 = unimplemented aiocancel */
	"sigpending",			/* 183 = sigpending */
	"#184 (unimplemented)",		/* 184 = unimplemented */
	"setpgid",			/* 185 = setpgid */
	"pathconf",			/* 186 = pathconf */
	"fpathconf",			/* 187 = fpathconf */
	"sysconf",			/* 188 = sysconf */
	"uname",			/* 189 = uname */
};
