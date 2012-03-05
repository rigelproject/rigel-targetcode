/***************************************************************************
******************************* 05/Jan/93 **********************************
****************************   Version 1.0   *******************************
************************* Author: Paolo Cignoni ****************************
*                                                                          *
*    FILE:      main.c                                                     *
*                                                                          *
* PURPOSE:      An incremental costruction Delaunay 3d triangulator.       *
*                                                                          *
* IMPORTS:      OList                                                      *
*                                                                          *
* EXPORTS:      ProgramName to error.c                                     *
*               Statistic variables to unifgrid.c                          *
*                                                                          *
*   NOTES:      This is the optimized version of the InCoDe algorithm.     *
*               It use hashing and Uniform Grid techniques to speed up     *
*               list management and tetrahedra construction.               *
*                                                                          *
*               This program uses the 0.9 release of OList for list        *
*               management.                                                *
*                                                                          *
****************************************************************************
**************************************************************************/

#include "../common.h"

#include <rigel.h>
#include <rigel-tasks.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "rigel_math.h"
#include "OList/general.h"
#include "OList/error.h"
#include "OList/olist.h"
#include "OList/chronos.h"
#include "graphics.h"
#include "incode.h"

//#define MAX_TASKS 8

ATOMICFLAG_INIT_CLEAR(Init_flag);

const int QID = 0;


#define USAGE_MESSAGE "\nUsage: incode [-s[1|2]] [-u nnn] [-p] [-c] [-f|-t] Filein [Fileout]\n\t\
 -s\tTurn on statistic informations \n\t\
 -s1\tTurn on only numerical statistical informations \n\t\
 -s2\tAdd a description line to numerical statistical informations\n\t\
 -u nnn\t Set Uniform Grid size = nnn cell\n\t\
 -p print the number of built tetrahedra while processing\n\t\
 -c\tCheck every tetrahedron is a Delaunay one\n\t\
 -f\tCheck for float creating Face (caused by numerical errors) \n\t\
 -t\tCheck for float creating Tetrahedra (caused by num. errors) \n\
 "
/***************************************************************************
 * *                                                                          *
 * * Global Variables:                                                        *
 * *                                                                          *
 * *     - Statistic Informations                                             *
 * *     - Program Name (exported in error.c)                                 *
 * *     - Program Flags                                                      *
 * *                                                                          *
 * ***************************************************************************/


struct n_v_args n_v;


StatInfo SI;                    /* Statistic Infomations */

                                /************** Program Flags **************/

boolean CheckFlag       = OFF;  /* Whether checking each built tetrahedron */
                                /* is a Delaunay one.                      */

boolean StatFlag        = OFF;  /* Whether printing Statistic Infomations. */

boolean NumStatFlag     = OFF;  /* Whether printing only numerical values  */
                                /* of Statistic Infomations.               */

boolean NumStatTitleFlag= OFF;  /* Whether add title to previuos Statistics*/

boolean UGSizeFlag      = OFF;  /* Whether UG size is user defined         */
int     UGSize          = 1;    /* The UG size user proposed               */

boolean UpdateFlag      = OFF;  /* Whether printing the increasing number  */
                                /* of builded tetrahedra while processing. */

boolean SafeFaceFlag    = OFF;  /* Whether Testing AFL inserted Face to not*/
                                /* twice inserted. This situation happens  */
                                /* for numerical errors and cause loop the */
                                /* whole algorithm.                        */

boolean SafeTetraFlag   = OFF;  /* Analogous to SafeFaceFlag but check the */
                                /* tetrahedra instead of faces (Faster!)   */



/***************************************************************************
 * *                                                                          *
 * * CheckTetra                                                               *
 * *                                                                          *
 * * Test that all the points are out of the sphere circumscribed to          *
 * * tetrahedron t. This test need a O(n) time and is made only on explicit   *
 * * user request (-c parameter).                                             *
 * *                                                                          *
 * ***************************************************************************/

void CheckTetra(Tetra *t, Point3 v[], int n)
{
  int i;
  Point3 Center;
  float Radius;
  float d;

  if(!CalcSphereCenter(&(v[t->f[0]->v[0]]),
                       &(v[t->f[0]->v[1]]),
                       &(v[t->f[0]->v[2]]),
                       &(v[t->f[1]->v[1]]), &Center ))
    Error("CheckTetra, Bad Tetrahedron, CalcSphereCenter Failed!\n",EXIT);

  Radius = V3DistanceBetween2Points(&Center,&(v[t->f[0]->v[0]]));
  for(i=0;i<n;i++)
    {
      d=V3DistanceBetween2Points(&Center,&(v[i]));
      if( d < Radius - EPSILON){
        printf("points: %d %d %d %d\n",t->f[0]->v[0],t->f[0]->v[1],t->f[0]->v[2],t->f[1]->v[1]);
        printf("inside point is %d the radius is:%lf the dist is: %lf\n",i,Radius,d);
//      Error("CheckTetra, A not Delaunay Tetrahedron was built!\n",EXIT);
        }
    }
}


Tetra *BuildTetra(Face *f, int p)
{
  Tetra *t;
  Face *f0, *f1,*f2,*f3;

  t = (Tetra *)malloc(sizeof(Tetra));
  f0 =(Face *)malloc(sizeof(Face));
  f1 =(Face *) malloc(sizeof(Face));
  f2 =(Face *) malloc(sizeof(Face));
  f3 =(Face *) malloc(sizeof(Face));

  if(!f0 || !f1 || !f2 || !f3 || !t)
    Error("BuildTetra, Not enough memory for a new tetrahedron\n",EXIT);

  t->f[0]=f0;
  t->f[1]=f1;
  t->f[2]=f2;
  t->f[3]=f3;

  f0->v[0]=f->v[0];
  f0->v[1]=f->v[1];
  f0->v[2]=f->v[2];

  f1->v[0]=f->v[0];
  f1->v[1]=p;
  f1->v[2]=f->v[2];

  f2->v[0]=f->v[2];
  f2->v[1]=p;
  f2->v[2]=f->v[1];

  f3->v[0]=f->v[1];
  f3->v[1]=p;
  f3->v[2]=f->v[0];

  return t;
}

/***************************************************************************
 *									   *
 * MakeTetra								   *
 *									   *
 * Given a face find the dd-nearest point to it and joining it to the face  *
 * build a new Delaunay tetrahedron.					   *
 *									   *
 ***************************************************************************/

Tetra *MakeTetra(Face *f,Point3 *v,int n,int l_bound,int r_bound)
{
  Plane p,Mp;
  boolean found=FALSE;
  double Radius=BIGNUMBER,  rad;
  Line Lc;
  int pind=0;
  Tetra *t;
  Point3 Center, c;
  int i;
  int l,r;
  
  if(!CalcPlane(&(v[f->v[0]]),&(v[f->v[1]]),&(v[f->v[2]]),&p))
    Error("MakeTetra, Face with collinar vertices!\n",EXIT);
  
  CalcLineofCenter(&(v[f->v[0]]),&(v[f->v[1]]),&(v[f->v[2]]),&Lc);

  if (l_bound)
	l = l_bound - (r_bound-l_bound);
  if (r_bound < (n-1))
	r = r_bound + (r_bound-l_bound);

 // printf("r_bound: %d\n",r_bound);

  for(i=0;i<=n-1;i++)
    {
      if((i!=f->v[0]) &&
	 (i!=f->v[1]) &&
	 (i!=f->v[2]) &&
	 RightSide(&p,&(v[i])) )
	{
	  CalcMiddlePlane(&(v[i]),&(v[f->v[0]]),&Mp);
	  if(CalcLinePlaneInter(&Lc,&Mp,&c))
	    {
	      rad=V3SquaredDistanceBetween2Points(&c, &(v[i]));
	      
	      if(!RightSide(&p,&c)) rad=-rad;
	      if(rad==Radius) Error("MakeTetra, Five cocircular points!\n",EXIT);
	      if(rad<Radius)
		{
		  found=TRUE;
		  Radius=rad;
		  Center=c;
		  pind=i;
		}
	    }
	}
    }
  
  if(!found) return NULL;
  
  t=BuildTetra(f,pind);
  
  if(CheckFlag)  CheckTetra(t,v,n);
  
  return t;
}

/***************************************************************************
 *									   *
 * FirstTetra								   *
 *									   *
 * Build the first Delaunay tetrahedron of the triangulation.		   *
 *									   *
 ***************************************************************************/

Tetra *FirstTetra(Point3 *v, int n,int l_boundary, int r_boundary, int upper, int y_median)
{
  int i, MinIndex=0;
  float Radius, MinRadius=BIGNUMBER;
  
  Tetra *t;
  Plane p[3], Mp;
  Line Lc;
  Face f;
  Point3 c;
  
  boolean found=FALSE;
  int source = l_boundary+((r_boundary-l_boundary)/2);

  if(upper){
  	while(v[source].y <= (y_median+0.25)){
		source--;
  	}
	printf("found good point\n");
  }
  else{
  	while(v[source].y > (y_median - 0.25)){
        	source--;
        }
  }



  //make rr/2 more chances to be insde the rr
    f.v[0] = source; 	/* The first point of the face is the 0 */
			/* (any point is equally good).		*/
  
  for(i=l_boundary;i<=r_boundary;i++)	/* The 2nd point of the face is the	*/
    if(i!=f.v[0]) 	/* euclidean nearest to first point	*/
      {
	Radius=V3SquaredDistanceBetween2Points(&(v[source]), &(v[i]));
	if(Radius<MinRadius)
	  {
	    MinRadius=Radius;
	    MinIndex=i;
	  }
      }
  f.v[1]=MinIndex;
  /* The 3rd point is that with previous	*/
  /* ones builds the smallest circle.	*/
  
  CalcMiddlePlane(&(v[f.v[0]]),&(v[f.v[1]]), &(p[0]));
  
  MinRadius=BIGNUMBER;
  
  for(i=l_boundary;i<=r_boundary;i++)
    if(i!=f.v[0] && i!=f.v[1])
      {
	CalcMiddlePlane(&(v[f.v[0]]), &(v[i]),&(p[1]));
	if(CalcPlane(&(v[f.v[0]]),&(v[f.v[1]]),&(v[i]),&(p[2])))
	  if(CalcPlaneInter(&(p[0]), &(p[1]), &(p[2]), &c))
	    {
	      Radius=V3DistanceBetween2Points(&c, &(v[source]));
	      if(Radius<MinRadius)
		{
		  MinRadius=Radius;
		  MinIndex=i;
		}
	    }
      }
 //   fprintf(stdout,"third face point %d\n",MinIndex);  
  f.v[2]=MinIndex;
  
  
  /* The first tetrahedron construction is analogous to normal */
  /* MakeTetra, only we dont limit search to an halfspace.     */
  
  MinRadius=BIGNUMBER;
  
  CalcPlane(&(v[f.v[0]]),&(v[f.v[1]]),&(v[f.v[2]]),&p[0]);
  CalcLineofCenter(&(v[f.v[0]]),&(v[f.v[1]]),&(v[f.v[2]]),&Lc);
  
  for(i=l_boundary;i<=r_boundary;i++)
    if(i!=f.v[0] && i!=f.v[1] && i!=f.v[2] )
      {
	CalcMiddlePlane(&(v[i]),&(v[f.v[0]]),&Mp);
	if(CalcLinePlaneInter(&Lc,&Mp,&c))
	  {
	    Radius=V3SquaredDistanceBetween2Points(&c, &(v[i]));
	    
	    if(MinRadius==Radius) Error("FirstTetra, Five cocircular points!\n",EXIT);
	    if(Radius<MinRadius)
	      {
		found=TRUE;
		MinRadius=Radius;
		MinIndex=i;
	      }
	  }
      }

  if(!found) Error("FirstTetra, Planar dataset, unable to build first tetrahedron.\n",EXIT);
  
  if(!RightSide(&p[0],&(v[MinIndex])))	ReverseFace(&f);
  
  t=BuildTetra(&f, MinIndex);
  
  CheckTetra(t,v,n);
  
  ReverseFace(t->f[0]); /* First Face in first Tetra   */
  /* must be outward oriented    */
  return t;
  
}	




/***************************************************************************
 *									   *
 * InCoDe								   *
 *									   *
 * Given a vector v of n Point3 this function returns the tetrahedra list   *
 * of the Delaunay triangulation of points. This Functions uses the incre-  *
 * mental construction algoritm InCoDe [Cignoni 92].			   *
 *									   *
 * We build the triangulation adding to an initial Delaunay tetrahedron new *
 * Delaunay tetrahedra built from its faces (using MakeTetra function).	   *
 *									   *
 * This algorithm make use of Speed up techniques suggeste in the paper	   *
 * yelding an average linear performance (against a teorethical cubic worst *
 * case.								   *
 *									   *
 * [Cignoni 92]								   *
 * P. Cignoni, C. Montani, R. Scopigno		  d			   *
 *"A Merge-First Divide and Conquer Algorithm for E  Delaunay Triangulation"*
 * CNUCE Internal Report C92/16 Oct 1992					   *
 *									   *
 ***************************************************************************/


List InCoDe(struct n_v_args* n_v, int l_boundary,int r_boundary,int upper, float y_median)
{
	List Q=NULL_LIST;
	List T=NULL_LIST;
	List OldFace=NULL_LIST;
  	Tetra *t;
  	ShortTetra *st;
  	Face  *f;
  	int i,j;
  	UG g;
	int out_of_boundary;
	int n = n_v->n;
	Point3* v = n_v->v;
	int work_count = 0;
 	printf("starting incode\n"); 
  	Q=NewList(FIFO,sizeof(Face));			/* Initialize Active Face */
  	ChangeEqualObjectList(EqualFace,Q);		/* List Q.		  */
 // 	printf("checkpoint 1\n");
	HashList(n/4,HashFace,Q);
   //     printf("checkpoint 2\n");
  	if(SafeFaceFlag){						/* Initialize list for	  */
    	OldFace=NewList(FIFO,sizeof(Face)); 	/* preventing float face */
      	ChangeEqualObjectList(EqualFace,OldFace);	/* looping on numerical   */
      	HashList(n/4,HashFace,OldFace);		/* errors		  */
    }
  
  	T=NewList(FIFO,sizeof(ShortTetra));		/* Initialize Built Tetra-*/
  	ChangeEqualObjectList(EqualTetra,T);		/* hedra List T.	  */
  	if(SafeTetraFlag) 
		HashList(n/4,HashTetra,T);
  

 // 	if(UGSizeFlag) 
//		BuildUG(v,n,UGSize,&g); 	/* Initialize Uniform Grid */
  //	else 
		
		BuildUG(v,n,n,&g);
  
  
  	t=FirstTetra(v,n,l_boundary,r_boundary,upper, y_median);
  	for(i=0;i<4;i++){
      	InsertList(t->f[i],Q);
      	if(SafeFaceFlag)  InsertList(t->f[i],OldFace);
      	for(j=0;j<3;j++)
			if(g.UsedPoint[t->f[i]->v[j]]==-1) 
	  			g.UsedPoint[t->f[i]->v[j]]=1;
			else 
	  			g.UsedPoint[t->f[i]->v[j]]++; 
    	}
  
  	SI.Face=4;
  
  	st=Tetra2ShortTetra(t);
	fprintf(stdout,"first tetra %i %i %i %i\n",st->v[0],st->v[1],st->v[2],st->v[3]);	
	
 //insert check for first tetra to be fully in region 
  	free(t);
  
  	InsertList(st,T);
 // 	fprintf(stdout,"before while\n");
  	while(ExtractList(&f,Q)){
//		fprintf(stdout,"I'm in while loop\n");
  		t=FastMakeTetra(f,v,n,&g);
//		t = MakeTetra(f,v,n,l_boundary,r_boundary);
    	if(t==NULL) SI.CHFace++;
    	else{
			work_count++;
			out_of_boundary = 0;
			
			st=Tetra2ShortTetra(t);
//			printf("tetra %d %d %d %d\n",st->v[0],st->v[1],st->v[2],st->v[3]);
                        if (((v[st->v[0]].x < v[l_boundary].x) ||
                            (v[st->v[1]].x < v[l_boundary].x) ||
                            (v[st->v[2]].x < v[l_boundary].x) ||
			    (v[st->v[3]].x < v[l_boundary].x)) ||
			    (upper &&  ((v[st->v[0]].y < y_median) ||
                            (v[st->v[1]].y < y_median) ||
                            (v[st->v[2]].y < y_median) ||
                            (v[st->v[3]].y < y_median)))) 
			    out_of_boundary = 1;
			
	  		if(SafeTetraFlag){ 
	   			if(MemberList(st, T))
	   	   			Error("Cyclic Tetrahedra Creation\n",EXIT);
			}
			if(!out_of_boundary)
	  			InsertList(st,T);
	  /*
	  		if(UpdateFlag){
	  			if(++SI.Tetra%50 == 0) 
						printf("Tetrahedra Built %i\r",SI.Tetra++);

	  		}
*/
	 		for(i=1;i<4;i++){
	    		if(MemberList(t->f[i],Q)){
					for(j=0;j<3;j++)
		  				g.UsedPoint[t->f[i]->v[j]]--;
		
					DeleteCurrList(Q);
					free(t->f[i]);
		
		//	       for(j=0;j<3;j++)
		//		 g.UsedPoint[t->f[i]->v[j]]--;
	      		}
	    		else if(
				((v[t->f[i]->v[0]].x > v[l_boundary].x) && 
				(v[t->f[i]->v[1]].x > v[l_boundary].x) && 
				(v[t->f[i]->v[2]].x > v[l_boundary].x)) && 
				
				((v[t->f[i]->v[0]].x <= v[r_boundary].x) || 
                                (v[t->f[i]->v[1]].x <= v[r_boundary].x) || 
                                (v[t->f[i]->v[2]].x <= v[r_boundary].x)) &&
				
				((!upper && ((v[t->f[i]->v[0]].y <= y_median) ||
                                (v[t->f[i]->v[1]].y <= y_median) ||
                                (v[t->f[i]->v[2]].y <= y_median))) ||

				(upper && ((v[t->f[i]->v[0]].y > y_median) &&
                                (v[t->f[i]->v[1]].y > y_median) &&
                                (v[t->f[i]->v[2]].y > y_median))))){
						
					InsertList(t->f[i],Q);
					SI.Face++;
		
					for(j=0;j<3;j++){
		  				if(g.UsedPoint[t->f[i]->v[j]]==-1) 
		    				g.UsedPoint[t->f[i]->v[j]]=1;
		  				else 
							g.UsedPoint[t->f[i]->v[j]]++;
					}
		
					if(SafeFaceFlag){
		    			if(MemberList(t->f[i], OldFace))
		      				Error("Cyclic insertion in Active Face List\n",EXIT);
		    			InsertList(t->f[i],OldFace);
		  			}
	      			}
			else{

			                for(j=0;j<3;j++){
                                             if(g.UsedPoint[t->f[i]->v[j]]==-1)
                                        	     g.UsedPoint[t->f[i]->v[j]]=1;
                                             else
                                                     g.UsedPoint[t->f[i]->v[j]]++;
                                        }
/*
                                        if(SafeFaceFlag){
                                        if(MemberList(t->f[i], OldFace))
                                                Error("Cyclic insertion in Active Face List\n",EXIT);
                                        InsertList(t->f[i],OldFace);
                                        }
*/


			 }

			}
	  		free(t->f[0]);
	  		free(t);
			
		}
      	for(i=0;i<3;i++){
	  		g.UsedPoint[f->v[i]]--;
	  /*  if(g.UsedPoint[f->v[i]]==0) printf("Point %i completed\n",f->v[i]);*/
		}
      	if(!SafeFaceFlag) 
			free(f);
	}
  
 	fprintf(stdout,"I did %d work iters\n",work_count); 
  	return T;
}

/*****************************************************************************/
/*  FROM TRIANGLE CODE                                                       */
/*  sort_points()   Sort an array of points by x-coordinate, using the         */
/*                y-coordinate as a secondary key.                           */
/*                                                                           */
/*  Uses quicksort.  Randomized O(n log n) time.  No, I did not make any of  */
/*  the usual quicksort mistakes.                                            */
/*                                                                           */
/*****************************************************************************/


void sort_points(Point3* sortarray,int size)
{
  
  int left, right;
  int pivot;
  float pivotx, pivoty;
  Point3 temp;
  
  if (size == 2) {
    /* Recursive base case. */
    if ((sortarray[0].x > sortarray[1].x) ||
        ((sortarray[0].x == sortarray[1].x) &&
         (sortarray[0].y > sortarray[1].y))) {
      temp = sortarray[1];
      sortarray[1] = sortarray[0];
      sortarray[0] = temp;
    }
    return;
  }
  /* Choose a random pivot to split the array. */
  pivot = rand() % size;
  pivotx = sortarray[pivot].x;
  pivoty = sortarray[pivot].y;
  /* Split the array. */
  left = -1;
  right = size;
  while (left < right) {
    /* Search for a point whose x-coordinate is too large for the left. */
    do {
      left++;
    } while ((left <= right) && ((sortarray[left].x < pivotx) ||
                                 ((sortarray[left].x == pivotx) &&
                                  (sortarray[left].y < pivoty))));
    /* Search for a point whose x-coordinate is too small for the right. */
    do {
      right--;
    } while ((left <= right) && ((sortarray[right].x > pivotx) ||
                                 ((sortarray[right].x == pivotx) &&
                                  (sortarray[right].y > pivoty))));
    if (left < right) {
      /* Swap the left and right points. */
      temp = sortarray[left];
      sortarray[left] = sortarray[right];
      sortarray[right] = temp;
    }
  }
  if (left > 1) {
    /* Recursively sort the left subset. */
    sort_points(sortarray, left);
  }
  if (right < size - 2) {
    /* Recursively sort the right subset. */
    sort_points(&sortarray[right + 1], size - right - 1);
  }	

}

void WorkOnPartition(struct n_v_args* n_v)
{
  	TQ_TaskDesc deq_tdesc;
        TQ_RetType retval;



        while (1) {
                #ifdef FAST_DEQUEUE
                RIGEL_DEQUEUE(QID, &deq_tdesc, &retval);
                #else
                retval = RIGEL_DEQUEUE(QID, &deq_tdesc);
                #endif
                if (retval == TQ_RET_BLOCK) continue;
                if (retval == TQ_RET_SYNC) break;
                if (retval != TQ_RET_SUCCESS) RigelAbort();
          
            // Dispatch the task
         {
	      printf("n = %d v[15].x  = %f start = %d end = %d upper = %d\n",n_v->n,n_v->v[15].x,deq_tdesc.v1,deq_tdesc.v2,deq_tdesc.v3);
              InCoDe(n_v,deq_tdesc.v1,deq_tdesc.v2,deq_tdesc.v3,0.0);
          }
          // END TASK
       }

}



/***************************************************************************
*									   *
* main									   *
*									   *
* Do the usual command line parsing, file I/O and timing matters.	   *
*									   *
***************************************************************************/



int main()
{
	int regions = 1;
	int i;
	int corenum = RigelGetCoreNum();
	int numcores = RigelGetNumCores();
        Point3* v = NULL;
	int n = 0;
	FILE* fp;
	int start,end;
	volatile float temp1;
	float temp2;
	float temp3;
	List T_orig[2];
	TQ_TaskDesc enq_tdesc[regions*2];
	
	if (corenum == 0) {
    		SIM_SLEEP_ON();
  		

         //       v=ReadPoints("points",&n);
         	v = GeneratePoints(&n);
		printf("n=%d\n",n);
		temp1 = n/(110.0);
		temp2 = n/(110.0);
		temp3 = n/(n-14);
  		fprintf(stdout,"read the data set\n");
		fprintf(stdout,"sqrt test sqrt(%f) = %f\n",temp1,sqrt(temp1));
		fprintf(stdout,"ceil test ceil(%f) = %f\n",temp2,ceilf(temp2));
		fprintf(stdout,"pow test pow(%f,0.333333) = %f\n",temp3,powf(temp3,0.333333));
  		//sort the points by x coord
  		sort_points(v,n);
		fprintf(stdout,"sorted points\n");
  		//write to file to make sure sort was successful    
//  		WritePoints("sorted",v,n);
//		fprintf(stdout,"wrote to sorted file\n");
               
  		//read the points only in the region end - start and the neighboring regions 
  		// for 1 dimension cuts read (end+rr) - (start-rr) 
                       
  //		fp=fopen("tmp","w");
  		//group args for lvm needs
  		n_v.n = n;
  		n_v.v = v;
  
  		SI.Point=n;
                ClearTimer(0);
                ClearTimer(1);
                //Start full run timer
                StartTimer(0);
  		RIGEL_INIT(QID, MAX_TASKS);
    
   	SW_TaskQueue_Set_DATA_PARALLEL_MODE();

    		SIM_SLEEP_OFF();
  		T_orig[0] = NULL_LIST;
  		T_orig[1] = NULL_LIST;
		//T_orig[0] = InCoDe(&n_v,start,end,1,0.0);
		//T_orig[1] = InCoDe(&n_v,start,end,0,0.0);
		//int part = n_v->n/regions;
  		start = 0;
  		end = n/regions-1;
		atomic_flag_set(&Init_flag);
        	for (i = 0; i < regions; i++) {
			enq_tdesc[i].v1  = start;
			enq_tdesc[i].v2  = end;
			enq_tdesc[i].v3  = 1;
//			 printf("upper? %d\n",enq_tdesc[i].v3);
			RIGEL_ENQUEUE(QID,&enq_tdesc[i]);
                        enq_tdesc[i+1].v1  = start;
                        enq_tdesc[i+1].v2  = end;
                        enq_tdesc[i+1].v3  = 0;
			//enq_tdesc[i+1] = {start,end,0x0, 0x0};
//			printf("upper? %d\n",enq_tdesc[i+1].v3);
			RIGEL_ENQUEUE(QID,&enq_tdesc[i+1]);
		        start = end;
		        end = end + n/regions;
 		}

 		SI.Tetra=0;
  //		for(i=0;i<2;i++)
  //      		SI.Tetra+=CountList(T_orig[i]);
 
//  		if(StatFlag && !NumStatFlag) PrintStat();
 // 		if(StatFlag && NumStatFlag && NumStatTitleFlag) PrintNumStatTitle();
//  		if(StatFlag && NumStatFlag) PrintNumStat();
//  		if(!StatFlag)
    		printf("Points:%7i Secs:%6.2f Tetras:%7i\n",SI.Point,SI.Secs,SI.Tetra);
  
//  		WriteTetraList("faces.ply",T_orig,2);

        //        atomic_flag_set(&Init_flag);
           }

	atomic_flag_spin_until_set(&Init_flag);
	
	//parallel task
        WorkOnPartition(&n_v);

  
        // Stopfull run timer
  	if (corenum == 0) 
 	 	StopTimer(0);
  	
  	return 0;
}
  

/*
  int regions = 8;
  char buf[80];
  Point3 *v;
  List   T[regions*2];
  List   T_orig[2];
  List   T_part;
  int n,i=1;
  int start_ms,finish_ms,diff_ms;
  FILE *fp=stdout;
  int start,end;
//  struct timespec start_time, finish_time;
  UG g;
  int y_median;
  struct n_v_args {
	Point3 *v;
	int n;
  };
  struct n_v_args n_v;
  

  
  SetProgramName("InCoDe");
  srand(3);
  if((argc<2) ||
     (strcmp(argv[i],"/?")==0)||
     (strcmp(argv[i],"-?")==0)||
     (strcmp(argv[i],"/h")==0)||
     (strcmp(argv[i],"-h")==0)  ) Error(USAGE_MESSAGE, EXIT);
  
  while(*argv[i]=='-')
    {
      switch(argv[i][1])
	{
	case 'p' : UpdateFlag=ON;			break;
	case 'c' : CheckFlag=ON; 			break;
	case 'f' : SafeFaceFlag=ON;			break;
	case 't' : SafeTetraFlag=ON;			break;
	case 's' : StatFlag=ON;
	  if(argv[i][2]=='1') NumStatFlag=ON;
	  if(argv[i][2]=='2')
	    {
	      NumStatTitleFlag=ON;
	      NumStatFlag=ON;
	    }
	  break;
	  
	case 'u' : UGSizeFlag=ON;
	  if(argv[i][2]==0 && isdigit(argv[i+1][0]))
	    UGSize=atoi(argv[++i]);
	  else UGSize=atoi(argv[i]+2);
	  break;
	  
	default	: sprintf(buf,"Unknown options '%s'\n",argv[i]);
	  Error(buf,NO_EXIT);
	}
      i++;
    }
  
  
  
  //read points all points
  v=ReadPoints(argv[i++],&n);
  
  //sort the points by x coord
  sort_points(v,n);
  
  
  
  WritePoints("sorted",v,n);
   
  //read the points only in the region end - start and the neighboring regions 
  // for 1 dimension cuts read (end+rr) - (start-rr) 
  
  
  if(argc>i) fp=fopen(argv[i],"w");

//clock_gettime(CLOCK_REALTIME,&start_time);
  
  //group args for lvm needs
  n_v.n = n;
  n_v.v = v;

  SI.Point=n;
  BuildUG(v,n,n,&g);

  start = 0;
  end = n-1;
  T_orig[0] = NULL_LIST;
  T_orig[1] = NULL_LIST;
//  T_orig[0] = InCoDe(v,n,start,end,g,1,0.0);
//  T_orig[1] = InCoDe(v,n,start,end,g,0,0.0);

  ResetUG(&g,n);
//  clock_gettime(CLOCK_REALTIME,&finish_time);
 // start_ms = start_time.tv_sec*1000+start_time.tv_nsec/1000000;
//  finish_ms = finish_time.tv_sec*1000+finish_time.tv_nsec/1000000;
//  diff_ms  = finish_ms - start_ms;
//  printf("execution time seq run is:%fs\n",(float)diff_ms/1000);
  printf("Number of tetra: %d\n",CountList(T_orig[1])+CountList(T_orig[1]));

//clock_gettime(CLOCK_REALTIME,&start_time);

  
  SI.Point=n;
  BuildUG(v,n,n,&g);

  start = 0;
  end = n/regions-1;
  for(i=0;i<regions*2;i++)
        T[i] = NULL_LIST;
  for(i=0;i<regions*2;i+=2){
//        T[i]=InCoDe(v,n,start,end,g,1,0.0);
        printf("done with upper part of chunk %d with %d tetras\n",i/2,CountList(T[i]));
//        T[i+1]=InCoDe(v,n,start,end,g,0,0.0);
        printf("done with lower part of chunk %d with %d tetras\n",i/2,CountList(T[i+1]));	
        ResetUG(&g,n);
        start = end;
        end = end + n/regions;
  }
//  clock_gettime(CLOCK_REALTIME,&finish_time);
//  start_ms = start_time.tv_sec*1000+start_time.tv_nsec/1000000;
//  finish_ms = finish_time.tv_sec*1000+finish_time.tv_nsec/1000000;
//  diff_ms  = finish_ms - start_ms;
//  printf("execution time parallel full run is:%fs\n",(float)diff_ms/1000);

 T_part = NULL_LIST;
//clock_gettime(CLOCK_REALTIME,&start_time); 
  
  start = 0;
  end = n/regions-1;  
//  T_part=InCoDe(v,n,start,end,g,1,0.0);
  
//  clock_gettime(CLOCK_REALTIME,&finish_time);
//  start_ms = start_time.tv_sec*1000+start_time.tv_nsec/1000000;
//  finish_ms = finish_time.tv_sec*1000+finish_time.tv_nsec/1000000;
//  diff_ms  = finish_ms - start_ms;
//  printf("execution time 1 part of chunck is:%fs\n",(float)diff_ms/1000); 

  SI.Tetra=0;
  for(i=0;i<regions*2;i++)
        SI.Tetra+=CountList(T[i]);
 

  if(StatFlag && !NumStatFlag) PrintStat();
  if(StatFlag && NumStatFlag && NumStatTitleFlag) PrintNumStatTitle();
  if(StatFlag && NumStatFlag) PrintNumStat();
  if(!StatFlag)
    printf("Points:%7i Secs:%6.2f Tetras:%7i\n",SI.Point,SI.Secs,SI.Tetra);
  
  WriteTetraList("faces.ply",T,regions*2,fp);
  
  return 0;
}
*/
