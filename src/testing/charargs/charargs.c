#include <stdio.h>
#include <rigel.h>

void charargs(char a, char b, char c, char d, char e)
{
	char array[6];
	RigelPrint(a);
	RigelPrint(b);
	RigelPrint(c);
	RigelPrint(d);
	RigelPrint(e);
	array[0] = a;
	array[1] = b;
	array[2] = c;
	array[3] = d;
	array[4] = e;
	array[5] = '\0';
	RigelPrint(*(int *)array);
	RigelPrint(*(((int *)array)+1));
	printf("%s\n", array);
	printf("%c\n", array[0]);
	fprintf(stderr, "%c%c%c%c%c\n", array[0], array[1], array[2], array[3], array[4]);
  FILE *buf = fopen("buf.txt", "w");
	FILE *nbuf = fopen("nbuf.txt", "w");
	setbuf(nbuf, NULL);
	fprintf(buf, "%c%c%c%c%c\n", array[0], array[1], array[2], array[3], array[4]);
	fprintf(stderr, "%c%c\n", array[0], array[1]);
	fclose(buf);
	fclose(nbuf);
}

int main(int argc, char *argv[])
{
	charargs('R','I','G','E','L');
  return 0;
}
