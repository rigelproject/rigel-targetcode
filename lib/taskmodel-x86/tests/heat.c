#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// heat (tasked)
////////////////////////////////////////////////////////////////////////////////
// this benchmark performs a 5-point stencil computation for heat flow
// ported initially from Cilk
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// benchmark parameters
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// compiler switches
////////////////////////////////////////////////////////////////////////////////
#define SW_COHERENCE 0

#define DEBUG 0

#ifdef HW_TQ
	#define RIGEL_DEQUEUE	TaskQueue_Dequeue_FAST
	#define RIGEL_ENQUEUE	TaskQueue_Enqueue
	#define RIGEL_LOOP		TaskQueue_Loop	
	#define RIGEL_INIT		TaskQueue_Init
	#define RIGEL_END			TaskQueue_End
#endif

#ifdef CLUSTER_LOCAL_TQ
	#define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_CLUSTER_FAST
	#define RIGEL_ENQUEUE	SW_TaskQueue_Enqueue_CLUSTER
	#define RIGEL_LOOP		SW_TaskQueue_Loop_CLUSTER_FAST
	#define RIGEL_INIT		SW_TaskQueue_Init_CLUSTER
	#define RIGEL_END			SW_TaskQueue_End_CLUSTER
#endif

#ifdef X86_RTM
  #define RIGEL_DEQUEUE	TaskQueue_Dequeue
  #define RIGEL_ENQUEUE	TaskQueue_Enqueue
  #define RIGEL_LOOP		TaskQueue_Loop	
  #define RIGEL_INIT		TaskQueue_Init
  #define RIGEL_END			TaskQueue_End
#endif

extern int errno;

////////////////////////////////////////////////////////////////////////////////
// macros
////////////////////////////////////////////////////////////////////////////////
#define f(x,y)     (sin(x)*sin(y))
#define randa(x,t) (0.0)
#define randb(x,t) (expf(-2*(t))*sin(x))
#define randc(y,t) (0.0)
#define randd(y,t) (expf(-2*(t))*sin(y))
#define solu(x,y,t) (expf(-2*(t))*sin(x)*sin(y))

////////////////////////////////////////////////////////////////////////////////
// globals
////////////////////////////////////////////////////////////////////////////////
int QID;							// Rigel Queue ID shared by all of the cores
int MAX_TASKS = 64*1024; 	// 640KiB should be enough for anyone
// Flag used to signal to other cores that the initialization work has been
// completed by core zero.  Set when complete
ATOMICFLAG_INIT_CLEAR(Flag_Init);


volatile int nx, ny, nt;
volatile float xu, xo, yu, yo, tu, to;
volatile float dx, dy, dt;

volatile float dtdxsq, dtdysq;
volatile float t;

int leafmaxcol;


////////////////////////////////////////////////////////////////////////////////
// allcgrid
////////////////////////////////////////////////////////////////////////////////
/*****************   Allocation of grid partition  ********************/
////////////////////////////////////////////////////////////////////////////////
void 
allcgrid(float **new, float **old, int lb, int ub)
{
  int j;
  float **rne, **rol;

  for (j=lb, rol=old+lb, rne=new+lb; j < ub; j++, rol++, rne++) {
    *rol = (float *) malloc(2*ny * sizeof(float));
		*rol += (32 * (((j) * ub) ^ 0x52c3)) % ny;
    *rne = (float *) malloc(2*ny * sizeof(float));
		*rne += (32 * (((j) * ub) ^ 0x52c3)) % ny;
  }
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// initgrid()
////////////////////////////////////////////////////////////////////////////////
/*****************   Initialization of grid partition  ********************/
////////////////////////////////////////////////////////////////////////////////
void 
initgrid(float **old, int lb, int ub)
{
  int a, b, llb, lub;

  llb = (lb == 0) ? 1 : lb;
  lub = (ub == nx) ? nx - 1 : ub;
  
  for (a=llb, b=0; a < lub; a++)		/* boundary nodes */
    old[a][b] = randa(xu + a * dx, 0);
  
  for (a=llb, b=ny-1; a < lub; a++)
    old[a][b] = randb(xu + a * dx, 0);
  
  if (lb == 0) {
    for (a=0, b=0; b < ny; b++)
      old[a][b] = randc(yu + b * dy, 0);
  }
  if (ub == nx) {
    for (a=nx-1, b=0; b < ny; b++)
      old[a][b] = randd(yu + b * dy, 0);
  }
  for (a=llb; a < lub; a++) {	/* inner nodes */
    for (b=1; b < ny-1; b++) {
      old[a][b] = f(xu + a * dx, yu + b * dy);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// compstripe()
////////////////////////////////////////////////////////////////////////////////
/***************** Five-Point-Stencil Computation ********************/
// Task descriptor will contain the new matrix in v1 and the old matrix
// in v2.  v3 will be the start iteration (lb) and v4 is the end iteration (ub)
////////////////////////////////////////////////////////////////////////////////
void 
compstripe(float **new, float **old, int lb, int ub)
{
  int a, b, llb, lub;
 
 	#if DEBUG 
  RigelPrint(0xaaa00000);
	#endif

  llb = (lb == 0) ? 1 : lb;
  lub = (ub == nx) ? nx - 1 : ub;
  
  for (a=llb; a < lub; a++) {
    for (b=1; b < ny-1; b++) {
      new[a][b] =   dtdxsq * (old[a+1][b] - 2 * old[a][b] + old[a-1][b])
	          + dtdysq * (old[a][b+1] - 2 * old[a][b] + old[a][b-1])
  	          + old[a][b];
    }
  }

	// borders  
	
	// right border
  for (a=llb, b=ny-1; a < lub; a++)
    new[a][b] = randb(xu + a * dx, t);

 	// left border 
  for (a=llb, b=0; a < lub; a++)
    new[a][b] = randa(xu + a * dx, t);
 
 	// top border 
  if (lb == 0) {
    for (a=0, b=0; b < ny; b++)
      new[a][b] = randc(yu + b * dy, t);
  }

	// bottom border
  if (ub == nx) {
    for (a=nx-1, b=0; b < ny; b++)
      new[a][b] = randd(yu + b * dy, t);
  }

	#if SW_COHERENCE
	// initiate a flush every 8 words (cacheline)
	// assumes 8-word aligned dataset, word-sized elements, and cacheline-multiple
	// dataset size
	for(a=lb; a<ub; a+=8) {
		for(b=0; b<ny; b+=8) {
			// flush output data
			RigelFlushLine( &new[a][b] );
			// invalidate read-shared input data
			RigelInvalidateLine( &old[a][b] );
		}
	}
	//the following aggressively invalidate read-shared data used for this
	//iteration (these lines are read-shared with stripes above and/or below this
	//stripe and may still be needed and have to be read back in)
	if(lb!=0) { // line above output set, which we read
		for(b=0; b<ny; b+=8) {
			RigelInvalidateLine( &old[lb-1][b] );
		}
	}
	if(ub!=nx) { // line below output set, which we read
		for(b=0; b<ny; b+=8) {
			RigelInvalidateLine( &old[ub+1][b] );
		}
	}
	#endif

}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// heat()
////////////////////////////////////////////////////////////////////////////////
//volatile float **old = NULL, **new = NULL;
float **old = NULL, **new = NULL;
int 
heat(void)
{
  int  c = 0;
	int core_num = RigelGetCoreNum();
 
 	// setup on core 0 
	if (0 == core_num) {
	  /* Memory Allocation */
	  old = (float **) malloc(nx * sizeof(float *));
	  new = (float **) malloc(nx * sizeof(float *));

		allcgrid(new, old, 0, nx);
		atomic_flag_set(&Flag_Init);
	}
	atomic_flag_spin_until_set(&Flag_Init);

  
	/************* Parrallel Init *****************/
  do {
		TQ_RetType TQRet = TQ_RET_SUCCESS;
		TQ_TaskDesc tdesc;
		TQ_TaskDesc tdesc_enq;

		// core 0 enqueues a loop of tasks
		if (0 == core_num) {
			ClearTimer(0);
			StartTimer(0);
			// Each iteration here is a timestep
			RIGEL_LOOP(QID, nx, leafmaxcol, &tdesc_enq);
		}

		// Dequeue tasks until we run out of tasks
		while(1) {
			#ifdef HW_TQ
				TaskQueue_Dequeue_FAST(QID, &tdesc, &TQRet);
			#else
				TQRet = RIGEL_DEQUEUE(QID, &tdesc);
			#endif

			// Exit when SYNC point reached, i.e., there are no tasks left
			if (TQRet == TQ_RET_BLOCK) continue;
			if (TQRet != TQ_RET_SUCCESS) break;
			// DISPATCH: Do compstripe on my section and then iterate 
			initgrid(old, tdesc.begin, tdesc.end);
		} 

		// The only valid thing here is for everyone to sync since this is how
		// each timestep proceeds
		assert(TQ_RET_SYNC == TQRet);
  } while (0);

	/************* Parallel Jacobi *****************/
	/* Jacobi Iteration (divide x-dimension of 2D grid into stripes) */
  for (c = 1; c <= nt; c++) {
		TQ_RetType TQRet = TQ_RET_SUCCESS;
		// TASK DESCRIPTOR: Will contain the new matrix in v1 and the old matrix
		// in v2.  v3 will be the start iteration (lb) and v4 is the end iteration (ub)
		TQ_TaskDesc tdesc;

		// Each iteration here is a timestep
		// core 0 enqueues new tasks for timestep
  	if (0 == core_num) {
			TQ_TaskDesc tdesc_enq;
			t = tu + c * dt;
			// GENERATE: Enqueue tasks for timestep
			if (c % 2) {
				tdesc_enq.v1 = (uint32_t)((void*)new);
				tdesc_enq.v2 = (uint32_t)((void*)old);
			} else {
				tdesc_enq.v1 = (uint32_t)((void*)old);
				tdesc_enq.v2 = (uint32_t)((void*)new);
			}
			// Dump the timestep
			#if DEBUG
			RigelPrint(c);
			#endif
			// queue new tasks
			RIGEL_LOOP(QID, nx, leafmaxcol, &tdesc_enq);
  	} 
	
		// Dequeue tasks until we run out of tasks
		while(1) {
			#ifdef HW_TQ 
				TaskQueue_Dequeue_FAST(QID, &tdesc, &TQRet);
			#else
				TQRet = RIGEL_DEQUEUE(QID, &tdesc);
			#endif

			// Exit when SYNC point reached, i.e., there are no tasks left
			if (TQRet == TQ_RET_BLOCK) continue;
			if (TQRet != TQ_RET_SUCCESS) break;
			// DISPATCH TASK: Do compstripe on my section and then iterate 
			compstripe((float **)tdesc.v1, (float **)tdesc.v2, tdesc.v3, tdesc.v4);
		} 

		// The only valid thing here is for everyone to sync since this is how
		// each timestep proceeds
		assert(TQ_RET_SYNC == TQRet);
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////
typedef enum { 	
	TEST_SIZE_T = 0,
	NORMAL_SIZE_T = 1,
	HUGE_SIZE_T = 2,
} problem_size_t;

int main() 
#ifdef X86_RTM
{
  // Initialize the emulation framework for the RTM
  X86_TASK_MODEL_INIT();
  // Call what will be 'main()' in the actual Rigel code
  X86_TASK_MODEL_WAIT();

  return 0;
}

// XXX: This must be defined by the user and will become main in the actual
// XXX: Rigel code that runs on RigelSim
int rigel_main()
#endif
{
	int core_num = RigelGetCoreNum();

	// init on core 0
	if (0 == core_num) {
		problem_size_t prob_size;

		RIGEL_INIT(QID, MAX_TASKS);
  	nx = 512; ny = 512; nt = 100; 
		xu = 0.0; xo = 1.570796326794896558; 
		yu = 0.0; yo = 1.570796326794896558;
		tu = 0.0; to = 0.0000001;
  	leafmaxcol = 10;

		fscanf(stdin, "%d", (int*)(&prob_size));
	
		switch (prob_size) {
			case TEST_SIZE_T:   
				nx = 32; ny = 32; nt = 2;
				xu = 0.0; xo = 1.570796326794896558;
				yu = 0.0; yo = 1.570796326794896558;
				tu = 0.0; to = 0.000001;
				leafmaxcol = 4;
				break;

    	case NORMAL_SIZE_T:  
				nx = 2048; ny = 128; nt = 4;
				xu = 0.0; xo = 1.570796326794896558;
				yu = 0.0; yo = 1.570796326794896558;
				tu = 0.0; to = 0.0000001;
				leafmaxcol = 2;
				break;

    	case HUGE_SIZE_T: 
				nx = 4096; ny = 256; nt = 32;
				xu = 0.0; xo = 1.570796326794896558;
				yu = 0.0; yo = 1.570796326794896558;
				tu = 0.0; to = 0.0000001;
				leafmaxcol = 4;
				break;

			default:
				assert(0 && "Unrecognized option");
    }	
  	
		dx = (xo - xu) / (nx - 1);
		dy = (yo - yu) / (ny - 1);
		dt = (to - tu) / nt;	/* nt effective time steps! */

		dtdxsq = dt / (dx * dx);
		dtdysq = dt / (dy * dy);
	} 
	// end init on core 0

	// main loop function (processes tasks until complete)
  heat();

	if (0 == core_num) StopTimer(0);

  return 0;
}
