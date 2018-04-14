#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
ssize_t __real_write (int fd, const void *buf, size_t size)
{
  write(fd, buf, size);
}

ssize_t __real_open (const char *path, int flags, int mode)
{
  open(path, flags, mode);
}
}
