#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
using namespace std;

extern "C"
{
  ssize_t __real_write (int fd, const void *buf, size_t size);
  ssize_t __real_open (const char *path, int flags, int mode);
}

#define STRING1 "** Hi, this is being wrapped. \n"
#define STRING2 "** Bye, this has beeen wrapped.\n"
#define STRING3 "Welcome!!\n"

extern "C" ssize_t __wrap_write (int fd, const void *buf, size_t size)
{
  __real_write(fd, STRING1, sizeof STRING1);
  int rc = __real_write(fd, buf, size);
  __real_write(fd, STRING2, sizeof STRING2);
  return rc;
}

extern "C" ssize_t __wrap_open (const char *path, int flags, int mode)
{
  int fd = __real_open(path, flags, mode);
  if (fd == -1) {
    perror("open error! \n");
  }
  write(fd, STRING3 , sizeof STRING3);
  return fd;
}
