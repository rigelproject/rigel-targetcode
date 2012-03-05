#include <stdio.h>
#include <stdlib.h>
#include <rigel.h>

int main(int argc, char *argv[])
{
	FILE *f = fopen("test.txt", "w");
	fprintf(f, "Hi: Here's 1 2 3: %d %d %d\n", 1, 2, 3);
	fprintf(f, "HIHIHIHI\n");
	printf("rgc is %08x\n", RigelGetCycle());
	printf("rgc %08x\n", RigelGetCycle());
	fclose(f);
	exit(1);
	return 0;
}
