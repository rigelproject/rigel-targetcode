/***************************************************************************
******************************* 05/Jan/93 **********************************
****************************   Version 1.0   *******************************
************************* Author: Paolo Cignoni ****************************
*                                                                          *
*    FILE:      file.c                                                     *
*                                                                          *
* PURPOSE:      Functions for I/O and support to list management.          *
*                                                                          *
* IMPORTS:      OList                                                      *
*                                                                          *
* EXPORTS:      ReadPoints          Read a Point file                      *
*               Tetra2ShortTetra    Trasform a Tetra in a ShortTetra       *
*               WriteTetraList      Write a tetra list on a file           *
*               HashFace            Hash key function for Faces            *
*               EqualFace           Testing equalness of Faces             *
*               HashTetra           Hash key function for Tetras           *
*               EqualTetra          Testing equalness of Tetras            *
*                                                                          *
*   NOTES:      This is the optimized version of the InCoDe algorithm.     *
*               It use hashing and Uniform Grid techniques to speed up     *
*               list management and tetrahedra construction.               *
*                                                                          *
*               This program uses the 0.9 release of OList for list        *
*               management.                                                *
*                                                                          *
****************************************************************************
***************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <OList/general.h>
#include <OList/error.h>
#include <OList/olist.h>

#include "graphics.h"
#include "incode.h"

/* Global for Statistic Infomations */

extern StatInfo SI;

Point3 *GeneratePoints(int *n){
	long i;
	float x,y,z;
	int signX,signY,signZ;
	Point3 *vec;
	srand(3);


	*n = POINT_NUM;
	vec=(Point3 *)malloc((POINT_NUM)*sizeof(Point3));
	if(!vec) Error("ReadPoints, Not enough memory to load point dataset.\n",EXIT);

	for(i=0;i<POINT_NUM;i++){

        	do{
                	x = rand()%POINT_NUM+1;
                	y = rand()%POINT_NUM+1;
                	z = rand()%POINT_NUM+1;
                	x = x/(x+rand()%POINT_NUM+1);
                	y = y/(y+rand()%POINT_NUM+1);
                	z = z/(z+rand()%POINT_NUM+1);
        	}while((x*x+y*y+z*z)<1);
        	signX = rand() % 2;
        	if(signX == 1)
                	x = -x;
        	signY = rand() % 2;
        	if(signY == 1)
                	y = -y;
       		signZ = rand() % 2;
        	if(signZ == 1)
                	z = -z;

		vec[i].x = x;
		vec[i].y = y;
		vec[i].z = z;

        }

	return vec;

}



/*
 * ReadPoints
 *
 * Legge un file di punti e restituisce un vettore di punti e il numero n di punti letti.
 * Formato del File:
 *      N
 *      X Y Z
 *      X Y Z
 *      ...
 *
 */




Point3 *ReadPoints(char *filename, int *n)
{
 FILE *fp;
 int i;
 Point3 *vec;
 float x,y;

 fp=fopen(filename,"r+");
 if(!fp) Error("ReadPoints, Unable to open input file.\n",EXIT);

 fscanf(stdin,"%d",n);
 vec=(Point3 *)malloc((*n)*sizeof(Point3));
 if(!vec) Error("ReadPoints, Not enough memory to load point dataset.\n",EXIT);

 for(i=0;i<*n;i++){
 	fscanf(stdin,"%f %f %f",&(vec[i].x),&(vec[i].y),&(vec[i].z));

 }
 return vec;
}

void WritePoints(char *filename,Point3* vec,int n){
	
	FILE *fp;
	int i;
	fp=fopen(filename,"w+");
	fprintf(fp,"%d\n",n);
	for(i=0;i<n;i++)
		fprintf(fp,"%f %f %f\n",vec[i].x,vec[i].y,vec[i].z);
//	fprintf(stdout,"%f %f %f\n",vec[i].x,vec[i].y,vec[i].z);    

	fclose(fp);




}


/*
 * Tetra2ShortTetra
 *
 * Dato un tetra restituisce un puntatore ad un ShortTetra
 * vettore di int con i quattro vertici
 *
 */

ShortTetra *Tetra2ShortTetra(Tetra *t)
{
 int i;
 ShortTetra *st;

 st=(ShortTetra *)malloc(sizeof(ShortTetra));
 if(!st)
   Error("Tetra2ShortTetra, Unable to allocate memory for ShortTetra\n",EXIT);


 st->v[0]=t->f[0]->v[0];
 st->v[1]=t->f[0]->v[1];
 st->v[2]=t->f[0]->v[2];
 for(i=0;i<3;i++)
 {
  if(t->f[1]->v[i] != st->v[0] &&
     t->f[1]->v[i] != st->v[1] &&
     t->f[1]->v[i] != st->v[2] ) st->v[3] = t->f[1]->v[i];
 }

 return st;
}

void createPLY(FILE* f,int face_num,int rnum){

	int point_num;
	int i,j;
	float x,y,z;
	int r,g,b,r1,g1,b1;
	FILE* sort = fopen("sorted","r+");
	srand(1024);
	fscanf(sort,"%d",&point_num);
	fprintf(f,"%s\n","ply");
	fprintf(f,"%s\n","format ascii 1.0");
	fprintf(f,"%s\n","comment zipper output");
	fprintf(f,"%s %d\n","element vertex",point_num);
	fprintf(f,"%s\n","property float x");
	fprintf(f,"%s\n","property float y");
	fprintf(f,"%s\n","property float z");
	fprintf(f,"%s\n","property uint8 red"); 
	fprintf(f,"%s\n","property uint8 green"); 
	fprintf(f,"%s\n","property uint8 blue");
	fprintf(f,"%s %d\n","element face",face_num);
	fprintf(f,"%s\n","property list uchar int vertex_indices");
	fprintf(f,"%s\n","end_header");
	for(i=0;i<rnum;i++){
	        r = rand() % 256;
	        g = rand() % 256;
      		b = rand() % 256;
		r1 = rand() % 256;
                g1 = rand() % 256;
                b1 = rand() % 256;
		for(j=0;j<point_num/rnum;j++){
			fscanf(sort,"%lf %lf %lf",&x,&y,&z);
			if(y<0)
				fprintf(f,"%lf %lf %lf %d %d %d\n",x,y,z,r,g,b);
			else
				fprintf(f,"%lf %lf %lf %d %d %d\n",x,y,z,r1,g1,b1);
		}
	}




}

/*
 * WriteTetraList
 *
 * Scrive nel file fp una lista di tetraedri,
 * ogni tetraedro e' scritto come 4 vertici.
 * All'inizio del file viene scritto il numero di tetraedri, ricavato dalla
 * variabile globale Tetra.
 */

void WriteTetraList(char* FileName, List* TList,int rnum, FILE *fp)
{
 ShortTetra *t;
 int i;
 int tetra_num=0; 
 int face_num=0;
 FILE* f = fopen(FileName,"w+");
 for(i=0;i<rnum;i++){
	tetra_num+=CountList(TList[i]);
	face_num = face_num+CountList(TList[i])*3+1;
 }
 fprintf(fp,"%d\n",tetra_num);
 createPLY(f,face_num,rnum);
 for(i=0;i<rnum;i++){
 	ExtractList(&t,TList[i]);
 	fprintf(f,"3 %6i %6i %6i\n",t->v[0],t->v[1],t->v[2]);
 	fprintf(fp,"%6i %6i %6i %6i\n",t->v[0],t->v[1],t->v[2],t->v[3]);
 	do{
    		fprintf(fp,"%6i %6i %6i %6i\n",t->v[0],t->v[1],t->v[2],t->v[3]);
    		fprintf(f,"3 %6i %6i %6i\n",t->v[0],t->v[1],t->v[3]);
		fprintf(f,"3 %6i %6i %6i\n",t->v[1],t->v[2],t->v[3]);
		fprintf(f,"3 %6i %6i %6i\n",t->v[0],t->v[2],t->v[3]);

 	}while(ExtractList(&t,TList[i]));
  }

 fclose(fp);
}




/*
 * HashFace
 *
 * Data una faccia ed un intero indicante la lunghezza del vettore Hash
 * restituisce la posizione nel vettore.
 *
 * tecnica usata: XOR dei tre vertici
 *
 */


int HashFace(void *F)
{
 Face *f=(Face *)F;
 return (f->v[0]) ^ (f->v[1]) ^ (f->v[2]) ;
}



/*
 * EqualFace
 *
 * Equal testing function for faces list operations
 * two faces are equals if they have the same vertices
 * (even in different order).
 *
 */

boolean EqualFace(void *F1, void *F2)
{
 Face *f1=(Face *)F1;
 Face *f2=(Face *)F2;

 SI.EqualTest++;
 if( f1->v[0] != f2->v[0] &&
     f1->v[0] != f2->v[1] &&
     f1->v[0] != f2->v[2] ) return FALSE;

 if( f1->v[1] != f2->v[0] &&
     f1->v[1] != f2->v[1] &&
     f1->v[1] != f2->v[2] ) return FALSE;

 if( f1->v[2] != f2->v[0] &&
     f1->v[2] != f2->v[1] &&
     f1->v[2] != f2->v[2] ) return FALSE;

 return TRUE;
}

/* EqualTetra
 *
 * Equal testing function for tetrahedra list operations
 * two tetrahedra are equals if they have the same vertices
 * (even in different order).
 */

boolean EqualTetra(void *T0, void *T1)
{
 ShortTetra *t0=(ShortTetra *)T0;
 ShortTetra *t1=(ShortTetra *)T1;

 if(t0->v[0]!=t1->v[0] &&
    t0->v[0]!=t1->v[1] &&
    t0->v[0]!=t1->v[2] &&
    t0->v[0]!=t1->v[3] ) return FALSE;

 if(t0->v[1]!=t1->v[0] &&
    t0->v[1]!=t1->v[1] &&
    t0->v[1]!=t1->v[2] &&
    t0->v[1]!=t1->v[3] ) return FALSE;

 if(t0->v[2]!=t1->v[0] &&
    t0->v[2]!=t1->v[1] &&
    t0->v[2]!=t1->v[2] &&
    t0->v[2]!=t1->v[3] ) return FALSE;

 if(t0->v[3]!=t1->v[0] &&
    t0->v[3]!=t1->v[1] &&
    t0->v[3]!=t1->v[2] &&
    t0->v[3]!=t1->v[3] ) return FALSE;

 return TRUE;
}

/*
 * Hash Function for fast list operation.
 * It must return the same key for equal tetrahedra (even if vertices are
 * differentely ordered).
 */

int HashTetra(void *T)
{
 ShortTetra *t=(ShortTetra *)T;

 return (t->v[0]*73)^(t->v[1]*73)^(t->v[2]*73)^(t->v[3]*73);
}
