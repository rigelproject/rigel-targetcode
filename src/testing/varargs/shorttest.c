#include "rigel.h"
#include "stdio.h"

extern struct
{
	  short a;
		  short b;
} s;

extern char c;

void shorttest()
{
	  RigelPrint(s.a);
		RigelPrint(s.b);
}

void chartest(char b)
{
	  char a = 0;
		a += b;
		a += c;
		c = a;
}

void consttest()
{
	RigelPrint(0x08800000);
}

