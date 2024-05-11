#include "platform.h"
#include <malloc.h>
#include <Memory.h>
#include <Timer.h>

size_t GC_pageSize (void) {
        /* Just return 256 rather than calling gestaltLogicalPageSize, because
         * we don't have anonymous mmap(). Alignment is handled manually.
         */
        return 256;
}

uintmax_t GC_physMem (void) {
  return LMGetApplLimit() - (char *) ApplicationZone();
}

void *GC_mmapAnon (__attribute__ ((unused)) void *start, size_t length) {
        size_t extra_length = GC_pageSize() + length;
        void *mem = malloc (extra_length);
        if (!mem) {
                return (void *) -1;
        }
        char *aligned = (char *) ((int) ((char *) mem + GC_pageSize()) & ~(GC_pageSize()-1));
        *((int *)aligned - 1) = (int) mem;
        return memset (aligned, 0, length);
}

void *GC_mmapAnonFlags (void *start, size_t length,
                        __attribute__ ((unused)) int flags) {
        return GC_mmapAnon (start, length);
}

void GC_release (void *base, __attribute__ ((unused)) size_t length) {
  free ((void *) *((int *)base - 1));
}

void GC_displayMem (void) {
}

void GC_diskBack_close (__attribute__ ((unused)) void *data) {
        die ("Disk-backed heap not supported");
}

void GC_diskBack_read (__attribute__ ((unused)) void *data,
                       __attribute__ ((unused)) pointer buf,
                       __attribute__ ((unused)) size_t size) {
        die ("Disk-backed heap not supported");
}

void *GC_diskBack_write (__attribute__ ((unused)) pointer buf,
                         __attribute__ ((unused)) size_t size) {
        die ("Disk-backed heap not supported");
}

int getrusage (__attribute__ ((unused)) int who,
               struct rusage *usage) {
  memset (usage, 0, sizeof (struct rusage));
  return 0;
}

int lstat (const char *path, struct stat *buf) {
  return stat (path, buf);
}

char *getcwd(char *buf, size_t size) {
  if (size == 0) {
    errno = EINVAL;
    return NULL;
  }
  if (size < 1) {
    errno = ERANGE;
    return NULL;
  }
  if (buf == NULL) {
    buf = malloc(1);
  }
  buf[0] = '\0';
  return buf;
}

int chdir(__attribute__ ((unused)) const char *path) {
  errno = ENOENT;
  return -1;
}

/* ------------------------------------------------- */
/*                      Signals                      */
/* ------------------------------------------------- */

static void (*SIGALRM_handler)(int sig) = SIG_DFL;
static void (*SIGVTAM_handler)(int sig) = SIG_DFL;
static void (*SIGPROF_handler)(int sig) = SIG_DFL;

static sigset_t signals_blocked = 0;

int sigaction (int signum,
               const struct sigaction *newact,
               struct sigaction *oldact) {
        _sig_func_ptr old;

        if (signum < 0 or signum >= NSIG) {
                errno = EINVAL;
                return -1;
        }

        switch (signum) {
        case SIGALRM:
                old = SIGALRM_handler;
                if (newact) SIGALRM_handler = newact->sa_handler;
                break;
        case SIGVTALRM:
                old = SIGVTAM_handler;
                if (newact) SIGVTAM_handler = newact->sa_handler;
                break;
        case SIGPROF:
                old = SIGPROF_handler;
                if (newact) SIGPROF_handler = newact->sa_handler;
                break;
        default:
                errno = EINVAL;
                return -1;
        }

        if (oldact)
                oldact->sa_handler = old;
        return 0;
}

int sigprocmask (int how, const sigset_t *set, sigset_t *oldset) {
        if (oldset) {
                *oldset = signals_blocked;
        }
        if (set) {
                sigset_t newmask = signals_blocked;

                switch (how) {
                        case SIG_BLOCK:
                                /* add set to current mask */
                                newmask |= *set;
                        break;
                        case SIG_UNBLOCK:
                                /* remove set from current mask */
                                newmask &= ~*set;
                        break;
                        case SIG_SETMASK:
                                /* just set it */
                                newmask = *set;
                        break;
                        default:
                                return -1;
                }

                signals_blocked = newmask;
        }
        return 0;
}
__attribute__ ((noreturn))
int sigsuspend (__attribute__ ((unused)) const sigset_t *mask) {
        die("sigsuspend is unimplemented");
}

/* ------------------------------------------------- */
/*                      ITimer                       */
/* ------------------------------------------------- */

typedef struct {
  TMTask tm;
  long interval;
  long a5World;
  bool installed;
} TMInfo;

static TMInfo gTMInfo;

static bool validTimeval(const struct timeval *tv) {
  return (tv->tv_sec >= 0) && (tv->tv_usec >= 0) && (tv->tv_usec < 1000000);
}

static void countToTimeval(long count, struct timeval *tv) {
    if (count < 0) {
      /* Negated microseconds. */
      tv->tv_sec = (-count) / 1000000;
      tv->tv_usec = (-count) % 1000000;
    } else {
      /* Milliseconds. */
      tv->tv_sec = count / 1000;
      tv->tv_usec = (count % 1000) * 1000;
    }
}

static long timevalToCount(const struct timeval *tv) {
  /* Round to milliseconds. */
  return (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
}

static void itimer_real_handler(TMInfo *tmInfo) {
  __asm__("\tMOVE.L %%a1,%0" : "=r"(tmInfo));
  long oldA5 = SetA5(tmInfo->a5World);

  if (SIGALRM_handler == SIG_IGN) {
    /* noop */
  } else if (SIGALRM_handler == SIG_DFL) {
    ExitToShell();
  } else {
    (*SIGALRM_handler)(SIGALRM);
  }
  if (tmInfo->interval != 0) {
    PrimeTime((QElemPtr) tmInfo, tmInfo->interval);
  }
  SetA5(oldA5);
}

static ProcPtr OriginalExitToShell;

static void PatchedExitToShell(void) {
  if (gTMInfo.installed) {
    /* Remove the task or else it will run later and crash the OS. */
    RmvTime((QElemPtr) &gTMInfo);
  }
  ((void (*)(void)) OriginalExitToShell)();
}

int setitimer(int which,
              const struct itimerval *value,
              struct itimerval *ovalue) {
  if (OriginalExitToShell == NULL) {
    OriginalExitToShell = GetToolboxTrapAddress(_ExitToShell);
    SetToolboxTrapAddress((ProcPtr) PatchedExitToShell, _ExitToShell);
  }

  if (which != ITIMER_REAL) {
    /* TODO */
    errno = ENOSYS;
    return -1;
  }
  if (value == NULL) {
    errno = EFAULT;
    return -1;
  }
  if (!validTimeval(&value->it_interval) || !validTimeval(&value->it_value)) {
    errno = EINVAL;
    return -1;
  }

  long remaining = 0;
  if (gTMInfo.installed) {
    RmvTime((QElemPtr) &gTMInfo);
    remaining = gTMInfo.tm.tmCount;
    gTMInfo.installed = false;
  }
  if (ovalue != NULL) {
    countToTimeval(remaining, &ovalue->it_value);
    countToTimeval(gTMInfo.interval, &ovalue->it_interval);
  }

  /* Save interval for repriming. */
  gTMInfo.interval = timevalToCount(&value->it_interval);

  /* Save A5 world for later. */
  gTMInfo.a5World = SetCurrentA5();

  gTMInfo.tm.tmAddr = (ProcPtr) itimer_real_handler;
  gTMInfo.tm.tmWakeUp = 0;
  gTMInfo.tm.tmReserved = 0;

  long count = timevalToCount(&value->it_value);
  if (count != 0) {
    InsXTime((QElemPtr) &gTMInfo);
    gTMInfo.installed = true;
    PrimeTime((QElemPtr) &gTMInfo, count);
  }

  return 0;
}
