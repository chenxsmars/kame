/*	$NetBSD: linux_syscall.h,v 1.17 2000/03/18 22:21:02 erh Exp $	*/

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.40 2000/02/03 10:02:59 abs Exp 
 */

/* syscall: "syscall" ret: "int" args: */
#define	LINUX_SYS_syscall	0

/* syscall: "exit" ret: "int" args: "int" */
#define	LINUX_SYS_exit	1

/* syscall: "fork" ret: "int" args: */
#define	LINUX_SYS_fork	2

/* syscall: "read" ret: "int" args: "int" "char *" "u_int" */
#define	LINUX_SYS_read	3

/* syscall: "write" ret: "int" args: "int" "char *" "u_int" */
#define	LINUX_SYS_write	4

/* syscall: "open" ret: "int" args: "const char *" "int" "int" */
#define	LINUX_SYS_open	5

/* syscall: "close" ret: "int" args: "int" */
#define	LINUX_SYS_close	6

/* syscall: "waitpid" ret: "int" args: "int" "int *" "int" */
#define	LINUX_SYS_waitpid	7

/* syscall: "creat" ret: "int" args: "const char *" "int" */
#define	LINUX_SYS_creat	8

/* syscall: "link" ret: "int" args: "const char *" "const char *" */
#define	LINUX_SYS_link	9

/* syscall: "unlink" ret: "int" args: "const char *" */
#define	LINUX_SYS_unlink	10

/* syscall: "execve" ret: "int" args: "const char *" "char **" "char **" */
#define	LINUX_SYS_execve	11

/* syscall: "chdir" ret: "int" args: "const char *" */
#define	LINUX_SYS_chdir	12

/* syscall: "time" ret: "int" args: "linux_time_t *" */
#define	LINUX_SYS_time	13

/* syscall: "mknod" ret: "int" args: "const char *" "int" "int" */
#define	LINUX_SYS_mknod	14

/* syscall: "chmod" ret: "int" args: "const char *" "int" */
#define	LINUX_SYS_chmod	15

/* syscall: "lchown" ret: "int" args: "const char *" "int" "int" */
#define	LINUX_SYS_lchown	16

/* syscall: "break" ret: "int" args: "char *" */
#define	LINUX_SYS_break	17

				/* 18 is obsolete ostat */
/* syscall: "lseek" ret: "long" args: "int" "long" "int" */
#define	LINUX_SYS_lseek	19

/* syscall: "getpid" ret: "pid_t" args: */
#define	LINUX_SYS_getpid	20

/* syscall: "setuid" ret: "int" args: "uid_t" */
#define	LINUX_SYS_setuid	23

/* syscall: "getuid" ret: "uid_t" args: */
#define	LINUX_SYS_getuid	24

/* syscall: "stime" ret: "int" args: "linux_time_t *" */
#define	LINUX_SYS_stime	25

/* syscall: "ptrace" ret: "int" args: "int" "int" "int" "int" */
#define	LINUX_SYS_ptrace	26

/* syscall: "alarm" ret: "int" args: "unsigned int" */
#define	LINUX_SYS_alarm	27

				/* 28 is obsolete ofstat */
/* syscall: "pause" ret: "int" args: */
#define	LINUX_SYS_pause	29

/* syscall: "utime" ret: "int" args: "const char *" "struct linux_utimbuf *" */
#define	LINUX_SYS_utime	30

				/* 31 is obsolete stty */
				/* 32 is obsolete gtty */
/* syscall: "access" ret: "int" args: "const char *" "int" */
#define	LINUX_SYS_access	33

/* syscall: "nice" ret: "int" args: "int" */
#define	LINUX_SYS_nice	34

				/* 35 is obsolete ftime */
/* syscall: "sync" ret: "int" args: */
#define	LINUX_SYS_sync	36

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	LINUX_SYS_kill	37

/* syscall: "rename" ret: "int" args: "const char *" "const char *" */
#define	LINUX_SYS_rename	38

/* syscall: "mkdir" ret: "int" args: "const char *" "int" */
#define	LINUX_SYS_mkdir	39

/* syscall: "rmdir" ret: "int" args: "const char *" */
#define	LINUX_SYS_rmdir	40

/* syscall: "dup" ret: "int" args: "u_int" */
#define	LINUX_SYS_dup	41

/* syscall: "pipe" ret: "int" args: "int *" */
#define	LINUX_SYS_pipe	42

/* syscall: "times" ret: "int" args: "struct times *" */
#define	LINUX_SYS_times	43

				/* 44 is obsolete prof */
/* syscall: "brk" ret: "int" args: "char *" */
#define	LINUX_SYS_brk	45

/* syscall: "setgid" ret: "int" args: "gid_t" */
#define	LINUX_SYS_setgid	46

/* syscall: "getgid" ret: "gid_t" args: */
#define	LINUX_SYS_getgid	47

/* syscall: "signal" ret: "int" args: "int" "linux_handler_t" */
#define	LINUX_SYS_signal	48

/* syscall: "geteuid" ret: "uid_t" args: */
#define	LINUX_SYS_geteuid	49

/* syscall: "getegid" ret: "gid_t" args: */
#define	LINUX_SYS_getegid	50

/* syscall: "acct" ret: "int" args: "char *" */
#define	LINUX_SYS_acct	51

				/* 52 is obsolete phys */
				/* 53 is obsolete lock */
/* syscall: "ioctl" ret: "int" args: "int" "u_long" "caddr_t" */
#define	LINUX_SYS_ioctl	54

/* syscall: "fcntl" ret: "int" args: "int" "int" "void *" */
#define	LINUX_SYS_fcntl	55

				/* 56 is obsolete mpx */
/* syscall: "setpgid" ret: "int" args: "int" "int" */
#define	LINUX_SYS_setpgid	57

				/* 58 is obsolete ulimit */
/* syscall: "oldolduname" ret: "int" args: "struct linux_oldold_utsname *" */
#define	LINUX_SYS_oldolduname	59

/* syscall: "umask" ret: "int" args: "int" */
#define	LINUX_SYS_umask	60

/* syscall: "chroot" ret: "int" args: "char *" */
#define	LINUX_SYS_chroot	61

/* syscall: "dup2" ret: "int" args: "u_int" "u_int" */
#define	LINUX_SYS_dup2	63

/* syscall: "getppid" ret: "pid_t" args: */
#define	LINUX_SYS_getppid	64

/* syscall: "getpgrp" ret: "int" args: */
#define	LINUX_SYS_getpgrp	65

/* syscall: "setsid" ret: "int" args: */
#define	LINUX_SYS_setsid	66

/* syscall: "sigaction" ret: "int" args: "int" "const struct linux_old_sigaction *" "struct linux_old_sigaction *" */
#define	LINUX_SYS_sigaction	67

/* syscall: "siggetmask" ret: "int" args: */
#define	LINUX_SYS_siggetmask	68

/* syscall: "sigsetmask" ret: "int" args: "linux_old_sigset_t" */
#define	LINUX_SYS_sigsetmask	69

/* syscall: "setreuid" ret: "int" args: "int" "int" */
#define	LINUX_SYS_setreuid	70

/* syscall: "setregid" ret: "int" args: "int" "int" */
#define	LINUX_SYS_setregid	71

/* syscall: "sigsuspend" ret: "int" args: "caddr_t" "int" "int" */
#define	LINUX_SYS_sigsuspend	72

/* syscall: "sigpending" ret: "int" args: "linux_old_sigset_t *" */
#define	LINUX_SYS_sigpending	73

/* syscall: "sethostname" ret: "int" args: "char *" "u_int" */
#define	LINUX_SYS_sethostname	74

/* syscall: "setrlimit" ret: "int" args: "u_int" "struct orlimit *" */
#define	LINUX_SYS_setrlimit	75

/* syscall: "getrlimit" ret: "int" args: "u_int" "struct orlimit *" */
#define	LINUX_SYS_getrlimit	76

/* syscall: "getrusage" ret: "int" args: "int" "struct rusage *" */
#define	LINUX_SYS_getrusage	77

/* syscall: "gettimeofday" ret: "int" args: "struct timeval *" "struct timezone *" */
#define	LINUX_SYS_gettimeofday	78

/* syscall: "settimeofday" ret: "int" args: "struct timeval *" "struct timezone *" */
#define	LINUX_SYS_settimeofday	79

/* syscall: "getgroups" ret: "int" args: "u_int" "gid_t *" */
#define	LINUX_SYS_getgroups	80

/* syscall: "setgroups" ret: "int" args: "u_int" "gid_t *" */
#define	LINUX_SYS_setgroups	81

/* syscall: "oldselect" ret: "int" args: "struct linux_oldselect *" */
#define	LINUX_SYS_oldselect	82

/* syscall: "symlink" ret: "int" args: "const char *" "const char *" */
#define	LINUX_SYS_symlink	83

/* syscall: "oolstat" ret: "int" args: "const char *" "struct stat43 *" */
#define	LINUX_SYS_oolstat	84

/* syscall: "readlink" ret: "int" args: "const char *" "char *" "int" */
#define	LINUX_SYS_readlink	85

/* syscall: "uselib" ret: "int" args: "const char *" */
#define	LINUX_SYS_uselib	86

/* syscall: "swapon" ret: "int" args: "char *" */
#define	LINUX_SYS_swapon	87

/* syscall: "reboot" ret: "int" args: "int" "int" "int" "void *" */
#define	LINUX_SYS_reboot	88

/* syscall: "readdir" ret: "int" args: "int" "caddr_t" "unsigned int" */
#define	LINUX_SYS_readdir	89

/* syscall: "old_mmap" ret: "int" args: "struct linux_oldmmap *" */
#define	LINUX_SYS_old_mmap	90

/* syscall: "munmap" ret: "int" args: "caddr_t" "int" */
#define	LINUX_SYS_munmap	91

/* syscall: "truncate" ret: "int" args: "const char *" "long" */
#define	LINUX_SYS_truncate	92

/* syscall: "ftruncate" ret: "int" args: "int" "long" */
#define	LINUX_SYS_ftruncate	93

/* syscall: "fchmod" ret: "int" args: "int" "int" */
#define	LINUX_SYS_fchmod	94

/* syscall: "fchown" ret: "int" args: "int" "int" "int" */
#define	LINUX_SYS_fchown	95

/* syscall: "getpriority" ret: "int" args: "int" "int" */
#define	LINUX_SYS_getpriority	96

/* syscall: "setpriority" ret: "int" args: "int" "int" "int" */
#define	LINUX_SYS_setpriority	97

/* syscall: "profil" ret: "int" args: "caddr_t" "u_int" "u_int" "u_int" */
#define	LINUX_SYS_profil	98

/* syscall: "statfs" ret: "int" args: "const char *" "struct linux_statfs *" */
#define	LINUX_SYS_statfs	99

/* syscall: "fstatfs" ret: "int" args: "int" "struct linux_statfs *" */
#define	LINUX_SYS_fstatfs	100

/* syscall: "ioperm" ret: "int" args: "unsigned int" "unsigned int" "int" */
#define	LINUX_SYS_ioperm	101

/* syscall: "socketcall" ret: "int" args: "int" "void *" */
#define	LINUX_SYS_socketcall	102

/* syscall: "setitimer" ret: "int" args: "u_int" "struct itimerval *" "struct itimerval *" */
#define	LINUX_SYS_setitimer	104

/* syscall: "getitimer" ret: "int" args: "u_int" "struct itimerval *" */
#define	LINUX_SYS_getitimer	105

/* syscall: "stat" ret: "int" args: "const char *" "struct linux_stat *" */
#define	LINUX_SYS_stat	106

/* syscall: "lstat" ret: "int" args: "const char *" "struct linux_stat *" */
#define	LINUX_SYS_lstat	107

/* syscall: "fstat" ret: "int" args: "int" "struct linux_stat *" */
#define	LINUX_SYS_fstat	108

/* syscall: "olduname" ret: "int" args: "struct linux_old_utsname *" */
#define	LINUX_SYS_olduname	109

/* syscall: "iopl" ret: "int" args: "int" */
#define	LINUX_SYS_iopl	110

/* syscall: "wait4" ret: "int" args: "int" "int *" "int" "struct rusage *" */
#define	LINUX_SYS_wait4	114

/* syscall: "ipc" ret: "int" args: "int" "int" "int" "int" "caddr_t" */
#define	LINUX_SYS_ipc	117

/* syscall: "fsync" ret: "int" args: "int" */
#define	LINUX_SYS_fsync	118

/* syscall: "sigreturn" ret: "int" args: "struct linux_sigcontext *" */
#define	LINUX_SYS_sigreturn	119

/* syscall: "clone" ret: "int" args: "int" "void *" */
#define	LINUX_SYS_clone	120

/* syscall: "setdomainname" ret: "int" args: "char *" "int" */
#define	LINUX_SYS_setdomainname	121

/* syscall: "uname" ret: "int" args: "struct linux_utsname *" */
#define	LINUX_SYS_uname	122

/* syscall: "modify_ldt" ret: "int" args: "int" "void *" "size_t" */
#define	LINUX_SYS_modify_ldt	123

/* syscall: "mprotect" ret: "int" args: "caddr_t" "int" "int" */
#define	LINUX_SYS_mprotect	125

/* syscall: "sigprocmask" ret: "int" args: "int" "const linux_old_sigset_t *" "linux_old_sigset_t *" */
#define	LINUX_SYS_sigprocmask	126

/* syscall: "getpgid" ret: "int" args: "int" */
#define	LINUX_SYS_getpgid	132

/* syscall: "fchdir" ret: "int" args: "int" */
#define	LINUX_SYS_fchdir	133

/* syscall: "personality" ret: "int" args: "int" */
#define	LINUX_SYS_personality	136

/* syscall: "setfsuid" ret: "int" args: "uid_t" */
#define	LINUX_SYS_setfsuid	138

/* syscall: "getfsuid" ret: "int" args: */
#define	LINUX_SYS_getfsuid	139

/* syscall: "llseek" ret: "int" args: "int" "u_int32_t" "u_int32_t" "caddr_t" "int" */
#define	LINUX_SYS_llseek	140

/* syscall: "getdents" ret: "int" args: "int" "struct linux_dirent *" "unsigned int" */
#define	LINUX_SYS_getdents	141

/* syscall: "select" ret: "int" args: "int" "fd_set *" "fd_set *" "fd_set *" "struct timeval *" */
#define	LINUX_SYS_select	142

/* syscall: "flock" ret: "int" args: "int" "int" */
#define	LINUX_SYS_flock	143

/* syscall: "msync" ret: "int" args: "caddr_t" "int" "int" */
#define	LINUX_SYS_msync	144

/* syscall: "readv" ret: "int" args: "int" "struct iovec *" "u_int" */
#define	LINUX_SYS_readv	145

/* syscall: "writev" ret: "int" args: "int" "struct iovec *" "u_int" */
#define	LINUX_SYS_writev	146

/* syscall: "getsid" ret: "pid_t" args: "pid_t" */
#define	LINUX_SYS_getsid	147

/* syscall: "fdatasync" ret: "int" args: "int" */
#define	LINUX_SYS_fdatasync	148

/* syscall: "__sysctl" ret: "int" args: "struct linux___sysctl *" */
#define	LINUX_SYS___sysctl	149

/* syscall: "mlock" ret: "int" args: "caddr_t" "size_t" */
#define	LINUX_SYS_mlock	150

/* syscall: "munlock" ret: "int" args: "caddr_t" "size_t" */
#define	LINUX_SYS_munlock	151

/* syscall: "sched_setparam" ret: "int" args: "pid_t" "const struct linux_sched_param *" */
#define	LINUX_SYS_sched_setparam	154

/* syscall: "sched_getparam" ret: "int" args: "pid_t" "struct linux_sched_param *" */
#define	LINUX_SYS_sched_getparam	155

/* syscall: "sched_setscheduler" ret: "int" args: "pid_t" "int" "const struct linux_sched_param *" */
#define	LINUX_SYS_sched_setscheduler	156

/* syscall: "sched_getscheduler" ret: "int" args: "pid_t" */
#define	LINUX_SYS_sched_getscheduler	157

/* syscall: "sched_yield" ret: "int" args: */
#define	LINUX_SYS_sched_yield	158

/* syscall: "sched_get_priority_max" ret: "int" args: "int" */
#define	LINUX_SYS_sched_get_priority_max	159

/* syscall: "sched_get_priority_min" ret: "int" args: "int" */
#define	LINUX_SYS_sched_get_priority_min	160

/* syscall: "nanosleep" ret: "int" args: "const struct timespec *" "struct timespec *" */
#define	LINUX_SYS_nanosleep	162

/* syscall: "mremap" ret: "void *" args: "void *" "size_t" "size_t" "u_long" */
#define	LINUX_SYS_mremap	163

/* syscall: "setresuid" ret: "int" args: "uid_t" "uid_t" "uid_t" */
#define	LINUX_SYS_setresuid	164

/* syscall: "getresuid" ret: "int" args: "uid_t *" "uid_t *" "uid_t *" */
#define	LINUX_SYS_getresuid	165

/* syscall: "poll" ret: "int" args: "struct pollfd *" "u_int" "int" */
#define	LINUX_SYS_poll	168

/* syscall: "setresgid" ret: "int" args: "gid_t" "gid_t" "gid_t" */
#define	LINUX_SYS_setresgid	170

/* syscall: "getresgid" ret: "int" args: "gid_t *" "gid_t *" "gid_t *" */
#define	LINUX_SYS_getresgid	171

/* syscall: "rt_sigreturn" ret: "int" args: "struct linux_rt_sigframe *" */
#define	LINUX_SYS_rt_sigreturn	173

/* syscall: "rt_sigaction" ret: "int" args: "int" "const struct linux_sigaction *" "struct linux_sigaction *" "size_t" */
#define	LINUX_SYS_rt_sigaction	174

/* syscall: "rt_sigprocmask" ret: "int" args: "int" "const linux_sigset_t *" "linux_sigset_t *" "size_t" */
#define	LINUX_SYS_rt_sigprocmask	175

/* syscall: "rt_sigpending" ret: "int" args: "linux_sigset_t *" "size_t" */
#define	LINUX_SYS_rt_sigpending	176

/* syscall: "rt_queueinfo" ret: "int" args: "int" "int" "void *" */
#define	LINUX_SYS_rt_queueinfo	178

/* syscall: "rt_sigsuspend" ret: "int" args: "linux_sigset_t *" "size_t" */
#define	LINUX_SYS_rt_sigsuspend	179

/* syscall: "pread" ret: "int" args: "int" "char *" "size_t" "linux_off_t" */
#define	LINUX_SYS_pread	180

/* syscall: "pwrite" ret: "int" args: "int" "char *" "size_t" "linux_off_t" */
#define	LINUX_SYS_pwrite	181

/* syscall: "chown" ret: "int" args: "const char *" "int" "int" */
#define	LINUX_SYS_chown	182

/* syscall: "__getcwd" ret: "int" args: "char *" "size_t" */
#define	LINUX_SYS___getcwd	183

/* syscall: "__vfork14" ret: "int" args: */
#define	LINUX_SYS___vfork14	190

#define	LINUX_SYS_MAXSYSCALL	191
