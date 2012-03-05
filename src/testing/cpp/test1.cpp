#include "test1.h"
#include <exception>
#include <iostream>

using namespace std;


a::a() { printf("ctr\n"); d1=43; }

void a::init(int d) { d1=d; }
int a::get() { return d1; }

int main() {

  a* a1 = new a;
  //a* a1 = (a*) malloc(sizeof(a));

  int c = a1->get();
  printf("%d\n",c);
  //a a1;
  a1->init(3);

  vector<int> v1(10);
  printf("vector size %d\n",v1.size());

  int b = a1->get();
  printf("%d\n",b);
  return b;

	try
	{
		  int * myarray= new int[1000];
	}
	catch (bad_alloc&)
	{
		cout << "Error allocating memory." << endl;
	}
}
