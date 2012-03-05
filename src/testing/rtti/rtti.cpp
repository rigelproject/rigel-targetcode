/* A base class pointer can point to objects of any class which is derived 
 * from it. RTTI is useful to identify which type (derived class) of object is 
 * pointed to by a base cls pointer.
 */
 
#include <stdio.h>
#include <rigel.h> 

class abc   // base class
{
public:
  virtual ~abc() { } 
  virtual void hello() 
  {
    fprintf(stderr,"in abc");
  }
};
 
class xyz : public abc
{
  public:
  void hello() 
  {
    fprintf(stderr,"in xyz");
  }
};
 
int main()
{
	fprintf(stderr,"Hi");
	RigelPrint(0x12345678);
  abc *abc_pointer = new xyz();
  xyz *xyz_pointer;
  // to find whether abc_pointer is pointing to xyz type of object
  xyz_pointer = dynamic_cast<xyz*>(abc_pointer);
 
  if (xyz_pointer != NULL)
  {
    fprintf(stderr,"abc_pointer is pointing to a xyz class object");   // identified
  }
  else
  {
    fprintf(stderr,"abc_pointer is NOT pointing to a xyz class object");
  }
 
  // needs virtual destructor 
  delete abc_pointer;
 
  return 0;
}
