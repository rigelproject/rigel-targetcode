#include <rigel.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

struct
{
  short a;
  short b;
} s;

char c;

void printargs(int arg1, ...) /* print all int type args, finishing with -1 */
{
  va_list ap;
  int i;
  va_start(ap, arg1);
  for (i = arg1; i >= 0; i = va_arg(ap, int))
    RigelPrint(i);
  va_end(ap);
}

short shorttest2(short *s, int n) {
  short stack[7];
	stack[0] = 0;
	for(int i = 1; i < 6; i++)
		stack[i+1] = stack[i] + *s++;
	return stack[n+1];
}

void func2(int arg1, int arg2, int arg3)
{
  RigelPrint(arg1);
  RigelPrint(arg2);
  RigelPrint(arg3);
}

void func1(int arg1, int arg2, int arg3, int arg4)
{
  RigelPrint(arg4);
  func2(arg1, arg2, arg3);
}

void shorttest();

//void shorttest()
//{
//  RigelPrint(s.a);
//  RigelPrint(s.b);
//}

void fixed(int a, int b, int c, int d, int e, int f, int g, int h, int i)
{
  for(int i = a; i < b; i++) {
  RigelPrint(a);
  RigelPrint(b);
  RigelPrint(c);
  RigelPrint(d);
  RigelPrint(e);
  RigelPrint(f);
  RigelPrint(g);
  RigelPrint(h);
  RigelPrint(i);
  }
}

void floatfunc(float a, float b)
{
  RigelPrint((a > 2.0f));
  RigelPrint((b < 2.0f));
}

typedef struct __attribute__((__packed__)) _s2 { char c; unsigned long x; } s2;
s2 bla;

void unaligned_test()
{
	bla.c = 1;
	bla.x = 0x12344321;
	RigelPrint(&bla.c);
	RigelPrint(&bla.x);
	RigelPrint(bla.c);
	RigelPrint(bla.x);
}

int nostack_test(int x)
{
	return x;
}

int main(int argc, char *argv[])
{
   if(RigelGetThreadNum() == 0) {
   s.a = 0xAABB;
   s.b = 0xCCDD;
   RigelPrint(s.a);
   RigelPrint(s.b);
   //printargs(1, 2, 3, -1);
   //fprintf(stderr, "Hi\n");
   //fprintf(stderr, "Bye\n");
   //fprintf(stderr, "PERCENT SIGNS: %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %% %%\n");
   //fprintf(stderr, "%d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6);
   int hi = 3;
   uint8_t c = 0x36U;
   fprintf(stderr, "&hi = %p, char = %"PRIu8"\n", &hi, c);
   //fprintf(stderr, "%f\n", 1024.1);
   //fprintf(stderr, "%d %f %d %f %d %f %d %f %d %f\n", 10, 20.1, 30, -40.2, 50, 60.3, 70, -80.4, 90, 100.5);
   //fprintf(stderr, "E here's numbers: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
   //func1(1, 2, 3, 4);
   //for(int i = 0; i < argc; i++) {
   //  fixed(argc, argc+10, 3, 4, 5, 6, 7, 8, 9);
   //}
   floatfunc((float)argc, (float)argc+1.0f);
   shorttest();
	 RigelPrint(0x13240000);
	 RigelPrint(0x00001328);
	 unaligned_test();
   SIM_SLEEP_OFF();
   }
   return 0;
}
