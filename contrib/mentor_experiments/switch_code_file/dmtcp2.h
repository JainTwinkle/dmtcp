#include<iostream>

class Helper
{
  private:
          int a;
  public:
         Helper() {
           this -> a = 10;
         }
         int get_a(void);
};

int
Helper :: get_a(void) {
#ifdef DEBUG
  std::cout <<"returning " << this->a << "\n";
#endif
  return this -> a;
}
