#include "platform.h"
#include <malloc.h>
#include <Memory.h>

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

int setitimer(__attribute__ ((unused)) int which,
              __attribute__ ((unused)) const struct itimerval *value,
              __attribute__ ((unused)) struct itimerval *ovalue) {
  errno = ENOSYS;
  return -1;
}
