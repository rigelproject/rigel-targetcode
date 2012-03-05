#include <cstdlib>
#include <cstdio>
#include <exception>

//void * operator new(size_t size) throw (std::bad_alloc) { return malloc(size); }
//void * operator new(size_t size) { return malloc(size); }

#include <vector>

class a {
private:
  int d1;

public:
  a();

  void init(int d);
  int get();

};
