#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <iostream>
#include "dmtcp.h"

extern "C"
{
  ssize_t __real_write (int fd, const void *buf, size_t size);
}

#define STR1 "Hello.  This call to 'write()' _IS_ being wrapped.\n"
#define STR2 "Hello.  This call to 'write()' is _NOT_ being wrapped.\n"
using namespace std;

void
function_ptr_example(int a)
{
#ifdef DEBUG
  cout << "DEBUG:";
#endif
  cout << "function pointer argument=" << a << "\n";
}

namespace switch_plugin {

class A {
  public:
    virtual void print() const
    {
     #ifdef DEBUG
      cout << "DEBUG: ";
     #endif
      cout << " A\n";
    }
};

class B {
  public:
    virtual void print() const
    {
     #ifdef DEBUG
      cout << "DEBUG: ";
     #endif
      cout << " B\n";
    }
};

class C: public A, public B {
  public:
    virtual void print() const
    {
     #ifdef DEBUG
      cout << "DEBUG: ";
     #endif
      cout << " C\n";
    }
};


// https://www.geeksforgeeks.org/templates-cpp/
template <typename T>
class Array
{
  private:
    T *ptr;
    int size;
  public:
    Array(T arr[], int s);
    void print();
};

template <typename T>
Array<T>::Array(T arr[], int s)
{
  ptr = new T[s];
  size = s;
  for(int i = 0; i < size; i++)
    ptr[i] = arr[i];
}

template <typename T>
void Array<T>::print() {
#ifdef DEBUG
  cout << "DEBUG:";
#endif
  for (int i = 0; i < size; i++) {
      cout << " " << *(ptr + i);
  }
  cout << endl;
}




class BaseClass
{
  public:
    virtual void pure_virtual(int) = 0;
    virtual int calculate_sum(int a, int b)
    {
     #ifdef DEBUG
       printf("Base class: DEBUG: ");
     #endif
       printf("Base class: sum = %d\n", a + b);
    }
    virtual void third() = 0;
    virtual void fourth() = 0;
    // overload example
    int my_num(void)
    {
     #ifdef DEBUG
      printf("Base DEBUG:");
      return 0;
     #endif
      return 1;
    }
};

class MyTime : public BaseClass
{
  private:
    void print_hello();
  public:
    float time_loop();
    int calculate_sum(int , int );
    void third() {}
    void fourth() {}
    // overload example
    float my_num(void);
    void dynamic_cast_test(void);
    void pure_virtual(int a) override
    {
      #ifdef DEBUG
        printf("pure_virtual DEBUG: %d", a);
      #else
        printf("pure_virtual: %d", a);
      #endif
    }
    template <typename T>
    T myMax(T x, T y)
    {
      return (x > y)? x: y;
    }
};

float
MyTime::my_num(void)
{
#ifdef DEBUG
  printf("Child DEBUG:");
  cout <<"Max(5, 11) = "<< myMax(5, 11) << "\n";
  return 0.0;
#else
  cout <<"Max(5.5, 11.1) = "<< myMax(5.5, 11.1) << "\n";
#endif
  return 1.1;
}

int
MyTime::calculate_sum(int a, int b)
{
#ifdef DEBUG
  printf("DEBUG: ");
#endif
  printf("sum = %d\n", a + b);
  cout << my_num() << "\n";
  void (* fptr)(int);
  fptr = &function_ptr_example;
  fptr(16);
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
  // Template class test
  int arr[5] = {1, 2, 3, 4, 5};
  Array<int> a(arr, 5);
  a.print();
  // dynamic_cast test
  dynamic_cast_test();
  float x = 0;
  for (i = 0; i < 1000000000ULL; ++i)
    x += 0.1;
  return x;
}

void
MyTime::dynamic_cast_test()
{

    switch_plugin::A* a = new A;
    switch_plugin::B* b = new B;
    switch_plugin::C* c = new C;

    a -> print(); b -> print(); c -> print();
    b = dynamic_cast< B*>(a);  // fails
    if (b)
       b -> print();
    else
       cout << "no B\n";
    a = c;
    a -> print(); // C prints
    b = dynamic_cast< B*>(a);  // succeeds
    if (b)
       b -> print();
    else
       cout << "no B\n";
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
