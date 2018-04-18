#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include "dmtcp.h"

extern "C"
{
  ssize_t __real_write (int fd, const void *buf, size_t size);
}

#define STR1 "Hello.  This call to 'write()' _IS_ being wrapped.\n"
#define STR2 "Hello.  This call to 'write()' is _NOT_ being wrapped.\n"

namespace switch_plugin {
class BaseClass
{
  public:
    virtual int calculate_sum(int , int ) = 0;
};

class MyTime //: public BaseClass
{
  private:
    void print_hello();
  public:
    float time_loop();
    int calculate_sum(int , int );
};

int
MyTime::calculate_sum(int a, int b)
{
#ifdef DEBUG
printf("DEBUG: ");
#endif
printf("sum = %d\n", a + b);
}

void
MyTime::print_hello()
{
#ifdef DEBUG
  printf("Hi..\n");
#else
  printf("Hello..\n");
#endif
  calculate_sum(3, 2);
}

// This function will be replaced by its unoptimized
// (debug) version on restart!
float
MyTime::time_loop()
{
#ifdef DEBUG
  printf("Debug function!\n");
#endif
  volatile unsigned long long i;
  print_hello();
  float x = 0;
  for (i = 0; i < 1000000000ULL; ++i)
    x += 0.1;
  return x;
}

};


int
main(int argc, char *argv[])
{
  int count = 1;
  struct timeval s, e, r;
  switch_plugin::MyTime obj;

  int fd = open("temp.txt", O_CREAT | O_RDWR, 0666);
  write(fd, STR1, sizeof STR1);
  __real_write(fd, STR2, sizeof STR2);

  while (1) {
    printf(" %2d\n", count++);
    fflush(stdout);
    // Call checkpoint at some point and exit
    if (count == 4) {
      printf("Checkpointing...\n");
      int retval = dmtcp_checkpoint();
      if (retval == DMTCP_AFTER_CHECKPOINT) {
        printf("Exiting...\n");
        exit(0);
      }
    }
    gettimeofday(&s, NULL);
    obj.time_loop();
    gettimeofday(&e, NULL);
    timersub(&e, &s, &r);
    printf("Loop took: %llu\n", (long long)(r.tv_sec * 1e6 + r.tv_usec));
  }
  return 0;
}
