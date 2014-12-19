
#include "elf32-tiny.h"
#include "SyscallWrapper.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/DataTypes.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/times.h>
#include <time.h>
#include <sys/utsname.h>

using namespace llvm;

namespace {

struct oi_stat
  {
    uint16_t st_dev;
    uint16_t st_ino;		//  File serial number.		
    uint32_t st_mode;	        // File mode.  
    uint16_t st_nlink;		// Link count. 
    uint16_t st_uid;		// User ID of the file's owner.	
    uint16_t st_gid;		// Group ID of the file's group.
    int16_t st_rdev;	// Device number, if device. 
    uint32_t st_size;		// Size of file, in bytes.  
    uint32_t my_atime;			// Time of last access.  
    uint32_t st_atimensec;	// Nscecs of last access.  
    uint32_t my_mtime;			// Time of last modification.  
    uint32_t st_mtimensec;	// Nsecs of last modification.  
    uint32_t my_ctime;			// Time of last status change.  
    uint32_t st_ctimensec;	// Nsecs of last status change.  
    int32_t st_blksize;	// Optimal block size for I/O.  
    int32_t st_blocks;	// Number of 512-byte blocks allocated.  
    uint32_t st_pad5[14];
  };


enum syscallscodes {
 sys_syscall = 4000,		/* 4000 */
  sys_exit	       ,
sys_fork		,
sys_read		,
sys_write		,
sys_open		,	/* 4005 */
sys_close		,
sys_waitpid		,
sys_creat		,
sys_link		,
sys_unlink		,	/* 4010 */
sys_execve		,
sys_chdir		,
sys_time		,
sys_mknod		,
sys_chmod		,	/* 4015 */
sys_lchown		,
sys_ni_syscall		,
sys_ni_syscall2		,	/* was sys_stat */
sys_lseek		,
sys_getpid		,	/* 4020 */
sys_mount		,
sys_oldumount		,
sys_setuid		,
sys_getuid		,
sys_stime		,	/* 4025 */
sys_ptrace		,
sys_alarm		,
sys_ni_syscall3		,	/* was sys_fstat */
sys_pause		,
sys_utime		,	/* 4030 */
sys_ni_syscall4		,
sys_ni_syscall5		,
sys_access		,
sys_nice		,
sys_ni_syscall6		,	/* 4035 */
sys_sync		,
sys_kill		,
sys_rename		,
sys_mkdir		,
sys_rmdir		,	/* 4040 */
sys_dup			,
sysm_pipe		,
sys_times		,
sys_ni_syscall7		,
sys_brk			,	/* 4045 */
sys_setgid		,
sys_getgid		,
sys_ni_syscall8		,	/* was signal(2) */
sys_geteuid		,
sys_getegid		,	/* 4050 */
sys_acct		,
sys_umount		,
sys_ni_syscall9		,
sys_ioctl		,
sys_fcntl		,	/* 4055 */
sys_ni_syscall10		,
sys_setpgid		,
sys_ni_syscall11		,
sys_olduname		,
sys_umask		,	/* 4060 */
sys_chroot		,
sys_ustat		,
sys_dup2		,
sys_getppid		,
sys_getpgrp		,	/* 4065 */
sys_setsid		,
sys_sigaction		,
sys_sgetmask		,
sys_ssetmask		,
sys_setreuid		,	/* 4070 */
sys_setregid		,
sys_sigsuspend		,
sys_sigpending		,
sys_sethostname		,
sys_setrlimit		,	/* 4075 */
sys_getrlimit		,
sys_getrusage		,
sys_gettimeofday	,
sys_settimeofday	,
sys_getgroups		,	/* 4080 */
sys_setgroups		,
sys_ni_syscall12		,	/* old_select */
sys_symlink		,
sys_ni_syscall13		,	/* was sys_lstat */
sys_readlink		,	/* 4085 */
sys_uselib		,
sys_swapon		,
sys_reboot		,
sys_old_readdir		,
sys_mmap		,	/* 4090 */
sys_munmap		,
sys_truncate		,
sys_ftruncate		,
sys_fchmod		,
sys_fchown		,	/* 4095 */
sys_getpriority		,
sys_setpriority		,
sys_ni_syscall14		,
sys_statfs		,
sys_fstatfs		,	/* 4100 */
sys_ni_syscall15		,	/* was ioperm(2) */
sys_socketcall		,
sys_syslog		,
sys_setitimer		,
sys_getitimer		,	/* 4105 */
sys_newstat		,
sys_newlstat		,
sys_newfstat		,
sys_uname		,
sys_ni_syscall16		,	/* 4110 was iopl(2) */
sys_vhangup		,
sys_ni_syscall17		,	/* was sys_idle() */
sys_ni_syscall18		,	/* was sys_vm86 */
sys_wait4		,
sys_swapoff		,	/* 4115 */
sys_sysinfo		,
sys_ipc			,
sys_fsync		,
sys_sigreturn		,
__sys_clone		,	/* 4120 */
sys_setdomainname	,
sys_newuname		,
sys_ni_syscall20		,	/* sys_modify_ldt */
sys_adjtimex		,
sys_mprotect		,	/* 4125 */
sys_sigprocmask		,
sys_ni_syscall21		,	/* was create_module */
sys_init_module		,
sys_delete_module	,
sys_ni_syscall22		,	/* 4130 was get_kernel_syms */
sys_quotactl		,
sys_getpgid		,
sys_fchdir		,
sys_bdflush		,
sys_sysfs		,	/* 4135 */
sys_personality		,
sys_ni_syscall23		,	/* for afs_syscall */
sys_setfsuid		,
sys_setfsgid		,
sys_llseek		,	/* 4140 */
sys_getdents		,
sys_select		,
sys_flock		,
sys_msync		,
sys_readv		,	/* 4145 */
sys_writev		,
sys_cacheflush		,
sys_cachectl		,
sys_sysmips		,
sys_ni_syscall24		,	/* 4150 */
sys_getsid		,
sys_fdatasync		,
sys_sysctl		,
sys_mlock		,
sys_munlock		,	/* 4155 */
sys_mlockall		,
sys_munlockall		,
sys_sched_setparam	,
sys_sched_getparam	,
sys_sched_setscheduler	,	/* 4160 */
sys_sched_getscheduler	,
sys_sched_yield		,
sys_sched_get_priority_max,
sys_sched_get_priority_min,
sys_sched_rr_get_interval,	/* 4165 */
sys_nanosleep		,
sys_mremap		,
sys_accept		,
sys_bind		,
sys_connect		,	/* 4170 */
sys_getpeername		,
sys_getsockname		,
sys_getsockopt		,
sys_listen		,
sys_recv		,	/* 4175 */
sys_recvfrom		,
sys_recvmsg		,
sys_send		,
sys_sendmsg		,
sys_sendto		,	/* 4180 */
sys_setsockopt		,
sys_shutdown		,
sys_socket		,
sys_socketpair		,
sys_setresuid		,	/* 4185 */
sys_getresuid		,
sys_ni_syscall25		,	/* was sys_query_module */
sys_poll		,
sys_ni_syscall26		,	/* was nfsservctl */
sys_setresgid		,	/* 4190 */
sys_getresgid		,
sys_prctl		,
sys_rt_sigreturn	,
sys_rt_sigaction	,
sys_rt_sigprocmask	,	/* 4195 */
sys_rt_sigpending	,
sys_rt_sigtimedwait	,
sys_rt_sigqueueinfo	,
sys_rt_sigsuspend	,
sys_pread64		,	/* 4200 */
sys_pwrite64		,
sys_chown		,
sys_getcwd		,
sys_capget		,
sys_capset		,	/* 4205 */
sys_sigaltstack		,
sys_sendfile		,
sys_ni_syscall27		,
sys_ni_syscall28		,
sys_mips_mmap2		,	/* 4210 */
sys_truncate64		,
sys_ftruncate64		,
sys_stat64		,
sys_lstat64		,
sys_fstat64		,	/* 4215 */
sys_pivot_root		,
sys_mincore		,
sys_madvise		,
sys_getdents64		,
sys_fcntl64		,	/* 4220 */
sys_ni_syscall29		,
sys_gettid		,
sys_readahead		,
sys_setxattr		,
sys_lsetxattr		,	/* 4225 */
sys_fsetxattr		,
sys_getxattr		,
sys_lgetxattr		,
sys_fgetxattr		,
sys_listxattr		,	/* 4230 */
sys_llistxattr		,
sys_flistxattr		,
sys_removexattr		,
sys_lremovexattr	,
sys_fremovexattr	,	/* 4235 */
sys_tkill		,
sys_sendfile64		,
  sys_futex		};


uint32_t *GetSyscallTable() {
  static uint32_t syscall_table[] = {
    666,//sys_restart_syscall,
    sys_exit,
    sys_fork,
    sys_read,
    sys_write,
    sys_open,
    sys_close,
    sys_creat,
    sys_time,
    sys_lseek,
    sys_getpid,
    sys_access,
    sys_kill,
    sys_dup,
    sys_times,
    sys_brk,
    666,//sys_mmap,
    sys_munmap,
    sys_newstat,//sys_stat,
    666,//sys_lstat,
    sys_newfstat,//sys_fstat,
    sys_uname,
    666,//sys__llseek,
    sys_readv,
    sys_writev,
    666,//sys_mmap2,
    sys_stat64,
    sys_lstat64,
    sys_fstat64,
    666,//sys_getuid32,
    666,//sys_getgid32,
    666,//sys_geteuid32,
    666,//sys_getegid32,
    sys_fcntl64,
    666,//sys_exit_group,
    sys_socketcall,
    sys_gettimeofday,
    sys_settimeofday
  };
  return syscall_table;
}

  uint32_t GetInt(OiMachineModel *MM, uint32_t num) {
    return MM->Bank[5 + num];
  }
  void SetInt(OiMachineModel *MM, uint32_t num, int32_t val) {
    MM->Bank[2 + num] = (uint32_t) val;
  }
  void SetBuffer(OiMachineModel *MM, int argn, unsigned char* buf,
                 unsigned int size) {
    memcpy(&MM->Mem->memory[MM->Bank[5+argn]],buf,size);
  }
  void GetBuffer(OiMachineModel *MM, int argn, unsigned char* buf,
                 unsigned int size) {
    memcpy(buf,&MM->Mem->memory[MM->Bank[5+argn]],size);
  }
  

}

#define NDEBUG

void ProcessSyscall(OiMachineModel *MM) {
#ifndef NDEBUG
  raw_ostream &DebugOut = outs();
#else
  raw_ostream &DebugOut = nulls();
#endif
#define DEBUG_SYSCALL(x) DebugOut << "Executing syscall: " << x
#define SET_BUFFER_CORRECT_ENDIAN(x,y,z) memcpy(&MM->Mem->memory[MM->Bank[5+x]],y,z)
#define FIX_OPEN_FLAGS(dst, src) do {                      \
    dst = 0;                                               \
    dst |= (src & 00000)? O_RDONLY : 0;                    \
    dst |= (src & 00001)? O_WRONLY : 0;                    \
    dst |= (src & 00002)? O_RDWR   : 0;                    \
    dst |= (src & 001000)? O_CREAT  : 0;                   \
    dst |= (src & 004000)? O_EXCL   : 0;                   \
    dst |= (src & 0100000)? O_NOCTTY   : 0;                \
    dst |= (src & 02000)? O_TRUNC    : 0;                  \
    dst |= (src & 00010)? O_APPEND   : 0;                  \
    dst |= (src & 040000)? O_NONBLOCK : 0;                 \
    dst |= (src & 020000)? O_SYNC  : 0;                    \
  } while (0)
// We don't know the mapping of these:
//    dst |= (src & 020000)? O_ASYNC   : 0;                \
//    dst |= (src & 0100000)? O_LARGEFILE  : 0;            \
//    dst |= (src & 0200000)? O_DIRECTORY  : 0;            \
//    dst |= (src & 0400000)? O_NOFOLLOW   : 0;            \
//    dst |= (src & 02000000)? O_CLOEXEC   : 0;            \
//    dst |= (src & 040000)? O_DIRECT      : 0;            \
//    dst |= (src & 01000000)? O_NOATIME   : 0;            \
//    dst |= (src & 010000000)? O_PATH     : 0;            \
//    dst |= (src & 010000)? O_DSYNC       : 0;            \
//
#define CORRECT_STAT_STRUCT(dst, src) do {                              \
    dst.st_dev = src.st_dev;                                            \
    dst.st_ino = src.st_ino;                                            \
    dst.st_mode = src.st_mode;                                          \
    dst.st_mode = 0;                                                    \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFDIR)?  0040000 : 0;   \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFCHR)?  0020000 : 0;   \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFBLK)?  0060000 : 0;   \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFREG)?  0100000 : 0;   \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFIFO)?  0010000 : 0;   \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFLNK)?  0120000 : 0;   \
    dst.st_mode |= ((src.st_mode & S_IFMT) == S_IFSOCK)? 0140000 : 0;   \
    dst.st_mode |= (src.st_mode & S_ISUID)?  04000 : 0;                 \
    dst.st_mode |= (src.st_mode & S_ISGID)?  02000 : 0;                 \
    dst.st_mode |= (src.st_mode & S_ISVTX)?  01000 : 0;                 \
    dst.st_mode |= (src.st_mode & S_IREAD)?  0400 : 0;                  \
    dst.st_mode |= (src.st_mode & S_IWRITE)? 0200 : 0;                  \
    dst.st_mode |= (src.st_mode & S_IEXEC)?  0100 : 0;                  \
    dst.st_nlink = src.st_nlink;                                        \
    dst.st_uid = src.st_uid;                                            \
    dst.st_gid = src.st_gid;                                            \
    dst.st_rdev = src.st_rdev;                                          \
    dst.st_size = src.st_size;                                          \
    dst.my_atime = src.st_atim.tv_sec;                                  \
    dst.st_atimensec = src.st_atim.tv_nsec;                             \
    dst.my_mtime = src.st_mtim.tv_sec;                                  \
    dst.st_mtimensec = src.st_mtim.tv_nsec;                             \
    dst.my_ctime = src.st_ctim.tv_sec;                                  \
    dst.st_ctimensec = src.st_ctim.tv_nsec;                             \
    dst.st_blksize = src.st_blksize;                                    \
    dst.st_blocks = src.st_blocks;                                      \
  } while (0)
  
#define CORRECT_SOCKADDR_STRUCT_TO_HOST(buf)                      
#define CORRECT_SOCKADDR_STRUCT_TO_GUEST(buf)                          
#define CORRECT_TIMEVAL_STRUCT(buf)                                     
#define CORRECT_TIMEZONE_STRUCT(buf)                                    

  const uint32_t *sctbl = GetSyscallTable();
  const uint32_t syscall = MM->Bank[4];
  
  if (syscall == sctbl[0]) {        // restart_syscall

  } else if (syscall == sctbl[1]) { // exit
    DEBUG_SYSCALL("exit");
    int exit_status = GetInt(MM, 0);
    exit(exit_status);
  } else if (syscall == sctbl[2]) { // fork
    int ret = ::fork();
    SetInt(MM, 0, ret);
    return;
  } else if (syscall == sctbl[3]) { // read
    DEBUG_SYSCALL("read");
    int fd = GetInt(MM, 0);
    unsigned count = GetInt(MM, 2);
    unsigned char *buf = (unsigned char*) malloc(count);
    int ret = ::read(fd, buf, count);
    SetBuffer(MM, 1, buf, ret);
    SetInt(MM, 0, ret);
    free(buf);
    return;

  } else if (syscall == sctbl[4]) { // write
    DEBUG_SYSCALL("write");
    int fd = GetInt(MM, 0);
    unsigned count = GetInt(MM, 2);
    unsigned char *buf = (unsigned char*) malloc(count);
    GetBuffer(MM, 1, buf, count);
    int ret = ::write(fd, buf, count);
    SetInt(MM, 0, ret);
    free(buf);
    return;

  } else if (syscall == sctbl[5]) { // open
    DEBUG_SYSCALL("open");
    unsigned char pathname[100];
    GetBuffer(MM, 0, pathname, 100);
    int flags = GetInt(MM, 1);
    int newflags = 0;
    FIX_OPEN_FLAGS(newflags, flags);
    int mode = GetInt(MM, 2);
    int ret = ::open((char*)pathname, newflags, mode);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[6]) { // close
    DEBUG_SYSCALL("close");
    int fd = GetInt(MM, 0);
    int ret;
    // Silently ignore attempts to close standard streams (newlib may try to do so when exiting)
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
      ret = 0;
    else
      ret = ::close(fd);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[7]) { // creat
    DEBUG_SYSCALL("creat");
    unsigned char pathname[100];
    GetBuffer(MM, 0, pathname, 100);
    int mode = GetInt(MM, 1);
    int ret = ::creat((char*)pathname, mode);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[8]) { // time
    DEBUG_SYSCALL("time");
    time_t param;
    time_t ret = ::time(&param);
    if (GetInt(MM, 0) != 0 && ret != (time_t)-1)
      SET_BUFFER_CORRECT_ENDIAN(0, (unsigned char *)&param,(unsigned) sizeof(time_t));
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[9]) { // lseek
    DEBUG_SYSCALL("lseek");
    off_t offset = GetInt(MM, 1);
    int whence = GetInt(MM, 2);
    int fd = GetInt(MM, 0);
    int ret;
    ret = ::lseek(fd, offset, whence);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[10]) { // getpid
    DEBUG_SYSCALL("getpid");
    pid_t ret = getpid();
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[11]) { // access
    DEBUG_SYSCALL("access");
    unsigned char pathname[100];
    GetBuffer(MM, 0, pathname, 100);
    int mode = GetInt(MM, 1);
    int ret = ::access((char*)pathname, mode);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[12]) { // kill
    DEBUG_SYSCALL("kill");
    SetInt(MM, 0, 0); 
    return;

  } else if (syscall == sctbl[13]) { // dup
    DEBUG_SYSCALL("dup");
    int fd = GetInt(MM, 0);
    int ret = dup(fd);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[14]) { // times
    DEBUG_SYSCALL("times");
    struct tms buf;
    clock_t ret = ::times(&buf);
    if (ret != (clock_t)-1)
      SET_BUFFER_CORRECT_ENDIAN(0, (unsigned char*)&buf, 
                                (unsigned)sizeof(struct tms));
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[15]) { // brk
    DEBUG_SYSCALL("brk");
    int ptr = GetInt(MM, 0);
    llvm_unreachable("brk unimplemented!");
    //SetInt(MM, 0, ref.ac_dyn_loader.mem_map.brk((Elf32_Addr)ptr));
    return;

  } else if (syscall == sctbl[16]) { // mmap
    DEBUG_SYSCALL("mmap");
    // Supports only anonymous mappings
    int flags = GetInt(MM, 3);
    Elf32_Addr addr = GetInt(MM, 0);
    Elf32_Word size = GetInt(MM, 1);
    llvm_unreachable("mmap unimplemented!");
    if ((flags & 0x20) == 0) { // Not anonymous
      SetInt(MM, 0, -EINVAL);
    } else {
      //      SetInt(MM, 0, ref.ac_dyn_loader.mem_map.mmap_anon(addr, size));
    }
    return;

  } else if (syscall == sctbl[17]) { // munmap
    DEBUG_SYSCALL("munmap");
    Elf32_Addr addr = GetInt(MM, 0);
    Elf32_Word size = GetInt(MM, 1);
    llvm_unreachable("munmap unimplemented!");
    //    if (ref.ac_dyn_loader.mem_map.munmap(addr, size))
    //            SetInt(MM, 0, 0);
    //    else
      SetInt(MM, 0, -EINVAL);
    return;

  } else if (syscall == sctbl[18]) { // stat
    DEBUG_SYSCALL("stat");
    unsigned char pathname[256];
    GetBuffer(MM, 0, pathname, 256);
    struct stat buf;
    int ret = ::stat((char *)pathname, &buf);
    if (ret >= 0) {
      struct oi_stat dst;
      CORRECT_STAT_STRUCT(dst, buf);
      SetBuffer(MM, 1, (unsigned char*)&dst, 60);
    }
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[19]) { // lstat
    DEBUG_SYSCALL("lstat");
    unsigned char pathname[256];
    GetBuffer(MM, 0, pathname, 256);
    struct stat buf;
    int ret = ::lstat((char *)pathname, &buf);
    if (ret >= 0) {
      struct oi_stat dst;
      CORRECT_STAT_STRUCT(dst, buf);
      SetBuffer(MM, 1, (unsigned char*)&dst, 60);
    }
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[20]) { // fstat
    DEBUG_SYSCALL("fstat");
    int fd = GetInt(MM, 0);
    struct stat buf;
    int ret = ::fstat(fd, &buf);
    if (ret >= 0) {
      struct oi_stat dst;
      CORRECT_STAT_STRUCT(dst, buf);
      SetBuffer(MM, 1, (unsigned char*)&dst, 60);
    }
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[21]) { // uname
    DEBUG_SYSCALL("uname");
    struct utsname *buf = (struct utsname*) malloc(sizeof(utsname));
    int ret = ::uname(buf);
    SetBuffer(MM, 0, (unsigned char *) buf, sizeof(utsname));
    free(buf);
    SetInt(MM, 0, ret);
    return; 

  } else if (syscall == sctbl[22]) { // _llseek
    DEBUG_SYSCALL("_llseek");
    unsigned fd = GetInt(MM, 0);
    unsigned long offset_high = GetInt(MM, 1);
    unsigned long offset_low = GetInt(MM, 2);
    off_t ret_off;
    int ret;
    unsigned whence = GetInt(MM, 4);
    if (offset_high == 0) {
      ret_off = ::lseek(fd, offset_low, whence);
      if (ret_off >= 0) {
	loff_t result = ret_off;
	SET_BUFFER_CORRECT_ENDIAN(3, (unsigned char*)&result,
                                  (unsigned) sizeof(loff_t));
	ret = 0;
      } else {
	ret = -1;
      }
    } else {
      ret = -1;
    }
    SetInt(MM, 0, ret);
    return; 

  } else if (syscall == sctbl[23]) { // readv
    DEBUG_SYSCALL("readv");
    int ret;
    int fd = GetInt(MM, 0);
    int iovcnt = GetInt(MM, 2);
    int *addresses = (int *) malloc(sizeof(int)*iovcnt);
    struct iovec *buf = (struct iovec *) malloc(sizeof(struct iovec)*iovcnt);
    GetBuffer(MM, 1, (unsigned char *) buf, sizeof(struct iovec)*iovcnt);
    for (int i = 0; i < iovcnt; i++) {
      memcpy(&addresses[i], &buf[i].iov_base, sizeof(int));
      unsigned char *tmp = (unsigned char *) malloc(buf[i].iov_len);
      buf[i].iov_base = (void *)tmp;
    }
    ret = ::readv(fd, buf, iovcnt);
    for (int i = 0; i < iovcnt; i++) {
      SetInt(MM, 1, addresses[i]);
      SetBuffer(MM, 1, (unsigned char *)buf[i].iov_base, buf[i].iov_len);
      free (buf[i].iov_base);
    }
    free(addresses);
    free(buf);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[24]) { // writev
    DEBUG_SYSCALL("writev");
    int ret;
    int fd = GetInt(MM, 0);
    int iovcnt = GetInt(MM, 2);
    struct iovec *buf = (struct iovec *) malloc(sizeof(struct iovec)*iovcnt);
    GetBuffer(MM, 1, (unsigned char *) buf, sizeof(struct iovec)*iovcnt);
    for (int i = 0; i < iovcnt; i++) {
      unsigned char *tmp;
      //      buf[i].iov_base = (void *) 
        //        CORRECT_ENDIAN((unsigned) buf[i].iov_base, sizeof(void *));
      //      buf[i].iov_len  = buf[i].iov_len;
      int iov_base = 0;
      memcpy(&iov_base, &buf[i].iov_base, sizeof(int));
      SetInt(MM, 1, iov_base);
      tmp = (unsigned char *) malloc(buf[i].iov_len);
      buf[i].iov_base = (void *)tmp;
      GetBuffer(MM, 1, tmp, buf[i].iov_len);
    }
    ret = ::writev(fd, buf, iovcnt);
    for (int i = 0; i < iovcnt; i++) {
      free (buf[i].iov_base);
    }
    free(buf);
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[25]) { // mmap2
    SetInt(MM,0, sctbl[16]);
    return ProcessSyscall(MM); //redirect to mmap

  } else if (syscall == sctbl[26]) { // stat64
    DEBUG_SYSCALL("stat64");
    unsigned char pathname[256];
    GetBuffer(MM, 0, pathname, 256);
    struct stat64 buf;
    int ret = ::stat64((char *)pathname, &buf);
    if (ret >= 0) {
      //CORRECT_STAT_STRUCT(buf);
      SetBuffer(MM, 1, (unsigned char*)&buf, sizeof(struct stat64));
    }
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[27]) { // lstat64
    DEBUG_SYSCALL("lstat64");
    unsigned char pathname[256];
    GetBuffer(MM, 0, pathname, 256);
    struct stat64 buf;
    int ret = ::lstat64((char *)pathname, &buf);
    if (ret >= 0) {
      //CORRECT_STAT_STRUCT(buf);
      SetBuffer(MM, 1, (unsigned char*)&buf, sizeof(struct stat64));
    }
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[28]) { // fstat64
    DEBUG_SYSCALL("fstat64");
    int fd = GetInt(MM, 0);
    struct stat64 buf;
    int ret = ::fstat64(fd, &buf);
    if (ret >= 0) {
      //CORRECT_STAT_STRUCT(buf);
      SetBuffer(MM, 1, (unsigned char*)&buf, sizeof(struct stat64));
    }
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[29]) { // getuid32
    DEBUG_SYSCALL("getuid32");
    uid_t ret = ::getuid();
    SetInt(MM, 0, (int)ret);
    return;

  } else if (syscall == sctbl[30]) { // getgid32
    DEBUG_SYSCALL("getgid32");
    gid_t ret = ::getgid();
    SetInt(MM, 0, (int)ret);
    return;

  } else if (syscall == sctbl[31]) { // geteuid32
    DEBUG_SYSCALL("geteuid32");
    uid_t ret = ::geteuid();
    SetInt(MM, 0, (int)ret);
    return;

  } else if (syscall == sctbl[32]) { // getegid32
    DEBUG_SYSCALL("getegid32");
    gid_t ret = ::getegid();
    SetInt(MM, 0, (int)ret);
    return;

  } else if (syscall == sctbl[33]) { // fcntl64
    DEBUG_SYSCALL("fcntl64");
    int ret = -EINVAL;
    SetInt(MM, 0, ret);
    return;

  } else if (syscall == sctbl[34]) { // exit_group
    DEBUG_SYSCALL("exit_group");
    int exit_status = GetInt(MM, 0);
    exit(exit_status);
    return;
  } else if (syscall == sctbl[35]) { // socketcall
    DEBUG_SYSCALL("socketcall");
    // See target toolchain include/linux/net.h and include/asm/unistd.h
    // for detailed information on socketcall translation. This works
    // form ARM.
    int ret;
    int call = GetInt(MM, 0);
    unsigned char tmp[256];
    GetBuffer(MM, 1, tmp, 256);
    unsigned *args = (unsigned*) tmp;
    switch (call) {
    case 1: // Assuming 1 = SYS_SOCKET
      {
        DEBUG_SYSCALL("\tsocket");
        ret = ::socket(args[0], args[1], args[2]);
        break;
      }
    case 2: // Assuming 2 = SYS_BIND
      {
        DEBUG_SYSCALL("\tbind");
        struct sockaddr buf;
        SetInt(MM, 0, args[1]);
        GetBuffer(MM, 0, (unsigned char*)&buf, sizeof(struct sockaddr));
        CORRECT_SOCKADDR_STRUCT_TO_HOST(buf);
        ret = ::bind(args[0], &buf, args[2]);
        break;
      }
    case 3: // Assuming 3 = SYS_CONNECT
      {
        DEBUG_SYSCALL("\tconnect");
        struct sockaddr buf;
        SetInt(MM, 0, args[1]);
        GetBuffer(MM, 0, (unsigned char*)&buf, sizeof(struct sockaddr));
        CORRECT_SOCKADDR_STRUCT_TO_HOST(buf);
        ret = ::connect(args[0], &buf, args[2]);
        break;
      }
    case 4: // Assuming 4 = SYS_LISTEN
      {
        DEBUG_SYSCALL("\tlisten");
        ret = ::listen(args[0], args[1]);
        break;
      }
    case 5: // Assuming 5 = SYS_ACCEPT
      {
        struct sockaddr addr;
        socklen_t addrlen;
        DEBUG_SYSCALL("\taccept");
        ret = ::accept(args[0], &addr, &addrlen);
        CORRECT_SOCKADDR_STRUCT_TO_GUEST(addr);
        //        addrlen = CORRECT_ENDIAN(addrlen, sizeof(socklen_t));
        SetInt(MM, 0, args[1]);
        SetBuffer(MM, 0, (unsigned char*)&addr, sizeof(struct sockaddr));
        SetInt(MM, 0, args[2]);
        SetBuffer(MM, 0, (unsigned char*)&addrlen, sizeof(socklen_t));
        break;
      }
    default:
      llvm_unreachable("Unimplemented socketcall() call number #");
      break;
    }
    SetInt(MM, 0, ret);
    return;
  } else if (syscall == sctbl[36]) { // gettimeofday
    DEBUG_SYSCALL("gettimeofday");
    int ret = -EINVAL;
    struct timezone tz;
    struct timeval tv;
    ret = ::gettimeofday(&tv, &tz);
    CORRECT_TIMEVAL_STRUCT(tv);
    CORRECT_TIMEZONE_STRUCT(tz);
    SetBuffer(MM, 0, (unsigned char*)&tv, sizeof(struct timeval));
    SetBuffer(MM, 1, (unsigned char*)&tz, sizeof(struct timezone));   
    SetInt(MM, 0, ret);
    return;
  } else if (syscall == sctbl[37]) { // settimeofday
    DEBUG_SYSCALL("settimeofday");
    int ret = -EPERM;
    llvm_unreachable("settimeofday: Ignored attempt to change host date");
    SetInt(MM, 0, ret);
    return;
  }

  /* Default case */
  SetInt(MM, 0, -EINVAL);
  return;
}
#undef DEBUG_SYSCALL
#undef SET_BUFFER_CORRECT_ENDIAN
#undef CORRECT_STAT_STRUCT
#undef CORRECT_SOCKADDR_STRUCT_TO_HOST
#undef CORRECT_SOCKADDR_STRUCT_TO_GUEST
#undef CORRECT_TIMEVAL_STRUCT                                     
#undef CORRECT_TIMEZONE_STRUCT
