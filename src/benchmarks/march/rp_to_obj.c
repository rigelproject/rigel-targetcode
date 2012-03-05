#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  int vertexNum = 0;
  int floatNum = 0;
  int i;
  float coord;
  int numVertices;
  sscanf(argv[1], "%d", &numVertices);
  FILE *out = fopen("out.obj", "w");
  for(i = 0; i < numVertices; i++)
  {
    float x, y, z;
    scanf("%f", &x);
    scanf("%f", &y);
    scanf("%f", &z);
    fprintf(out, "v %f %f %f\n", x, y, z);
  }

  for(i = 0; i < (numVertices/3); i++)
  {
    fprintf(out, "f %d %d %d\n", 3*i+1, 3*i+2, 3*i+3);
  }
  fclose(out);
  return 0;
}