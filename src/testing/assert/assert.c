#include <assert.h>
#include <rigel.h>

int main(int argc, char *argv[])
{
	if(RigelGetThreadNum() == 0) {
		SIM_SLEEP_ON();
		//The below should print: assertion "0 == 1 && "bla"" failed: file "assert.c", line 9, function: main
	  assert(0 == 1 && "bla"); //This should print out the assert expression, file name, line number, etc.
    SIM_SLEEP_OFF();
	}
	return 0;
}
