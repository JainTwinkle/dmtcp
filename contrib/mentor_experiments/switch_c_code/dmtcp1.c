#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "dmtcp.h"

void
print_hello()
{
#ifdef DEBUG
  printf("Hi..\n");
#else
  printf("Hello..\n");
#endif
}

// This function will be replaced by its unoptimized
// (debug) version on restart!
float
time_loop()
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

int
main(int argc, char *argv[])
{
  int count = 1;
  struct timeval s, e, r;

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
    time_loop();
    gettimeofday(&e, NULL);
    timersub(&e, &s, &r);
    printf("Loop took: %llu\n", (long long)(r.tv_sec * 1e6 + r.tv_usec));
  }
  return 0;
}
