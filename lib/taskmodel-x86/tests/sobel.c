#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rigel.h"
#include "rigel-math.h"
#include "rigel-string.h"
#include "rigel-tasks.h"


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Sobel Edge Detector (tasked)
////////////////////////////////////////////////////////////////////////////////
// FIXME:
// Currently, this version does not properly normalize and just arbitrarily
// thresholds -- but the basic computational pattern is there.
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// compile switches 
////////////////////////////////////////////////////////////////////////////////
#define DEBUG         0
#define DEBUG_SOBEL   0 
#define SW_COHERENCE  1
#define PRINT_RESULTS 0 

#define PROCESS_ROWS  1
//#define INIT_DATA

// types
#ifndef IMG_TYPE
	#define IMG_TYPE unsigned int
#endif
#ifndef MAT_TYPE
	#define MAT_TYPE IMG_TYPE
#endif

#if PROCESS_ROWS
	#define SOBEL_DO_TASK Sobel_DoRowTask
#endif

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
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// benchmark parameters
////////////////////////////////////////////////////////////////////////////////
#define WIDTH 		  2048 
#define HEIGHT 			WIDTH 
#define KERNEL_DIM 	3 
#define THRESH 			50
#define MAX 				255
#define OFFSET 			0
#define QID 				0
#define MAX_TASKS 	(1<<20)
#define OUTER_LOOP_ENQUEUERS 8
#define ROWS_PER_TASK 1 
//#define BLOCK_SIZE 	4 // BLOCK_SIZE x BLOCK_SIZE blocks
//#define UNROLL_FACTOR 8 (need X and Y block factors)
////////////////////////////////////////////////////////////////////////////////


// Rigel Includes
#include "macros.h"
#include "types.h"
#include "rigel_cv.h"


////////////////////////////////////////////////////////////////////////////////
// Global Declarations
////////////////////////////////////////////////////////////////////////////////

// Edge differencing kernels
#define EK_DIM 3
IMG_TYPE EX_KERNEL[EK_DIM][EK_DIM] = 
								{{-1, 0, 1}, 
								{ -2, 0, 2},
								{ -1, 0, 1} };
IMG_TYPE EY_KERNEL[EK_DIM][EK_DIM] =
								{{ 1, 2, 1}, 
								 { 0, 0, 0},
								 {-1,-2,-1} };

// matrices
rigelmatrix_t in;  // Input Image
rigelmatrix_t out; // Output: Thresholded Edge Strength
// kernels
rigelmatrix_t Ex_kernel;  // X-direction edge differencing kernel
rigelmatrix_t Ey_kernel;  // Y-direction edge differencing kernel

// task types
typedef enum { TASK_INNER, TASK_OUTER } TASK_TYPE;

volatile int GlobalSense;
volatile int BarrierCount = 0;
volatile int FLAG_Ready   = 0;


////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// GetSubMatrix()
////////////////////////////////////////////////////////////////////////////////
// copy a submatrix (size specifed in sub) out of larger matrix mat
// starting at element (row, col)
////////////////////////////////////////////////////////////////////////////////
void GetSubMatrix(rigelmatrix_t *mat, int row, int col, rigelmatrix_t *sub) {
	int i=0;
	int c,r;
	// stride across larger matrix to collect sub rows
	for( c=col; c<col+sub->width; c++ ){
		for( r=row; r<row+sub->height; r++ ){
			i++; // linearly set data inside submatrix
			sub->data[i] = mat->data[ INDEX(r,c,mat->width) ];
		}
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////
// SetSubMatrix()
////////////////////////////////////////////////////////////////////////////////
// copy a submatrix (size specified in sub) into larger matrix mat
// startint at element (row,col)
////////////////////////////////////////////////////////////////////////////////
void SetSubMatrix(rigelmatrix_t *mat, int row, int col, rigelmatrix_t *sub) {
	int i=0;
	int c,r;
	// stride across larger matrix to collect sub rows
	for( c=col; c<col+sub->width; c++ ){
		for( r=row; r<row+sub->height; r++ ){
			i++; // linearly set data inside submatrix
			mat->data[ INDEX(r,c,mat->width) ] = sub->data[i];
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// Convolve2D_Pixel()
////////////////////////////////////////////////////////////////////////////////
// Convolution for a single pixel value
////////////////////////////////////////////////////////////////////////////////
IMG_TYPE Convolve2D_Pixel( rigelmatrix_t *in_pixels, rigelmatrix_t *kernel ){

	int i;
	IMG_TYPE out_pixel = 0;

	for( i=0; i<kernel->size; i++ ) {
		out_pixel += in_pixels->data[i] *	kernel->data[i];
	}

	return out_pixel;

}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ROW Task Functions
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Sobel_DoRow()
////////////////////////////////////////////////////////////////////////////////
// Inner Loop Task
// Task: Perform Sobel Edge Detection for a Row
////////////////////////////////////////////////////////////////////////////////
void Sobel_DoRow( int row ) {

	int col;
	int Ex, Ey, Gx, Gy, G;

	rigelmatrix_t window;
	window.width  = KERNEL_DIM;
	window.height = KERNEL_DIM;
	window.size   = KERNEL_DIM * KERNEL_DIM;
	window.data   = (IMG_TYPE*) malloc( sizeof(IMG_TYPE) * window.size );

	IMG_TYPE* out_ptr = &(out.data[ INDEX(row,0,WIDTH) ]);

	// process pixels linearly
	// TODO: process in line-sized groups
	for( col=0; col<WIDTH; col++ ) {

		// copy window of image for current pixel
		GetSubMatrix( &in, row, col, &window );

		// X-direction Edges
		#if DEBUG_SOBEL
		fprintf( stderr, "X Edges (Conv)...\n");
		#endif
		//Convolve2D( in, &Ex, &Gx, CV_FLAGS_NULL );
		Ex = Convolve2D_Pixel( &window, &Ex_kernel );
		
		// Y-direction Edges
		#if DEBUG_SOBEL
		fprintf( stderr, "Y Edges (Conv)...\n");
		#endif
		//Convolve2D( in, &Ey, &Gy, CV_FLAGS_NULL );
		Ey = Convolve2D_Pixel( &window, &Ey_kernel );
		
		// approximate edge stregth
		// |G| = |Gx| + |Gy|
		#if DEBUG_SOBEL
		fprintf( stderr, "AbsSum...\n");
		#endif
		//AbsSum(&Gx, &Gy, G);
		G = abs(Gx) + abs(Gy);
		
		// threshold 
		#if DEBUG_SOBEL
		fprintf( stderr, "Thresh...\n");
		#endif
		//Threshold(G,out,THRESH,MAX);
		if( G < THRESH  ) { G=0; } else { G=MAX; }

		// set output
		//*(out_ptr+col) = G;
		out.data[ INDEX(row,col,WIDTH) ] = G;
		#ifdef SW_COHERENCE
		if( col%8 == 7 ) { // flush output on every 8th write (full dirty cacheline)
			RigelFlushLine( &out.data[ INDEX(row,col,WIDTH) ] );
		}
		#endif

	}

	return;

}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Sobel_DoRowTask()
////////////////////////////////////////////////////////////////////////////////
// Do Row Task
// v2 is offset of starting line
////////////////////////////////////////////////////////////////////////////////
void Sobel_DoRowTask(TQ_TaskDesc *task){

	int i;
	int offset = task->v2;

	#if DEBUG
	fprintf(stderr,"DoRowTask start\n");
	#endif

	// process assigned lines linearly from top to bottom
	for( i = task->begin + offset; i < task->end + offset; i++ ){

		Sobel_DoRow( i );	

	}

	#if DEBUG
	fprintf(stderr,"DoRowTask finish\n");
	#endif
	
}
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// Sobel_InnerLoopEnqueue()
////////////////////////////////////////////////////////////////////////////////
// Inner Loop Enqueue
// enqueue inner loops (a range of lines) to the task queue
////////////////////////////////////////////////////////////////////////////////
void Sobel_InnerLoopEnqueue( TQ_TaskDesc* task  ){

	int num_lines = HEIGHT / OUTER_LOOP_ENQUEUERS; // our section of tasks to enqueue 
	int num_tasks = num_lines / ROWS_PER_TASK;
	int offset = task->begin * num_lines; // where this task starts
	int retval;

	#if DEBUG
	fprintf(stderr,"num_tasks %d, ROWS_PER_TASK%d \n", num_tasks, ROWS_PER_TASK);
	#endif

	TQ_TaskDesc task_inner = { TASK_INNER, offset, 0x0, 0x0 };

	retval = RIGEL_LOOP(QID, num_tasks, ROWS_PER_TASK, &task_inner );
	
}
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// Sobel_OuterLoopEnqueue()
////////////////////////////////////////////////////////////////////////////////
// Outer Loop Enqueue
// enqueue inner loops (a range of lines) to the task queue
////////////////////////////////////////////////////////////////////////////////
void Sobel_OuterLoopEnqueue(){

	int num_tasks = OUTER_LOOP_ENQUEUERS;
	int lines_per_task = HEIGHT / OUTER_LOOP_ENQUEUERS;
	int retval;

	TQ_TaskDesc task_outer = { TASK_OUTER, 0x0, 0x0, 0x0};

	#if DEBUG
	fprintf(stderr,"num_tasks %d, lines_per_task %d \n", num_tasks, lines_per_task);
	#endif

	retval = RIGEL_LOOP(QID, num_tasks, lines_per_task , &task_outer );
	
}
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// ProcessTasks()
////////////////////////////////////////////////////////////////////////////////
// Process tasks from the task queue
////////////////////////////////////////////////////////////////////////////////
void ProcessTasks(){
	
	TQ_RetType retval;
	TQ_TaskDesc task;

	int corenum = RigelGetCoreNum();

	//********** enqueue initial tasks **********
	// enqueue the outer loop tasks that enqueue inner loop tasks
	if(corenum==0){
		Sobel_OuterLoopEnqueue();
	}

	//********** task processing loop **********
	while(1) {

		#ifdef HW_TQ 
			TaskQueue_Dequeue_FAST(QID, &task, &retval);
		#else
			retval = RIGEL_DEQUEUE(QID, &task);
		#endif

		if(retval == TQ_RET_BLOCK)   { /*fprintf(stderr,"block\n");*/ continue;     }
		if(retval == TQ_RET_SYNC)    { /*fprintf(stderr,"sync\n"); */ break;        }
		if(retval != TQ_RET_SUCCESS) { RigelPrint(0xaaaaaaaa); RigelAbort(); }

		RigelPrint( 0xaaa00000 );

		switch( task.v1 ) {
			case(TASK_INNER) :
				SOBEL_DO_TASK(&task); // Inner Loop (Task)
				break;
			case(TASK_OUTER) :
				Sobel_InnerLoopEnqueue(&task); // Outer Loop (Enqueue)
				break;
			default:
				fprintf(stderr,"Error, Task Descriptor has no specified type!\n");
				RigelAbort();
				break;
		}
	}

}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
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

	int i, j, kr, kc;
	int sum;

	int corenum = RigelGetCoreNum();
	int numcores = RigelGetNumCores();
	TQ_RetType retval;

	//********** setup barrier **********
#if 0
	BarrierInfo bi;
	BarrierInfo *bip = &bi;
	bi.corenum  = corenum;
	bi.numcores = numcores;
	bi.local_sense   = 0;
	bi.global_sense  = (int *)&GlobalSense;
	bi.barrier_count = (int *)&BarrierCount;
#endif

	// v1 field of task descriptor specifies the TASK_TYPE
	TQ_TaskDesc task_outer = { TASK_OUTER, OFFSET, 0x0, 0x0};

	//********** initialize (core 0) **********
	if(corenum==0) {

		// init task queue
		RIGEL_INIT(QID, MAX_TASKS);

		//********** init matrices **********
		in.width  = WIDTH;
		in.height = HEIGHT;
		in.size   = WIDTH*HEIGHT;
		in.data   = (IMG_TYPE*) malloc( sizeof(IMG_TYPE)*WIDTH*HEIGHT );

		out = in;
		out.data  = (IMG_TYPE*) malloc( sizeof(IMG_TYPE)*WIDTH*HEIGHT );

		//********** init kernels **********
		Ex_kernel.width  = EK_DIM;
		Ex_kernel.height = EK_DIM;
		Ex_kernel.data   = (IMG_TYPE*)&EX_KERNEL;
		Ey_kernel.width  = EK_DIM;
		Ey_kernel.height = EK_DIM;
		Ey_kernel.data   = (IMG_TYPE*)&EY_KERNEL;

		//********** init input image data **********
		#ifdef INIT_DATA
			for(i=0;i<WIDTH;i++){
				for(j=0;j<HEIGHT;j++){
					if( j==2 || j==3 || j==(HEIGHT-2) || i==2 || i==3 || i==(WIDTH-2) ) {
						in.data[ INDEX(j,i,WIDTH) ] = 0x00FF;
					}else{
						in.data[ INDEX(j,i,WIDTH) ] = 0;				
					}
				}
			}
			#ifdef DEBUG
			fprintf( stderr, "Input Image...\n");
	 		for( i=0; i<WIDTH*HEIGHT; i++ ) {
				RigelPrint( out.data[i] );			
	 		}	
			//PrintMatrix( &in );
			#endif		
		#endif

		//********** init matrices to 0s **********
		#ifdef INIT_DATA
		for(i=0;i<WIDTH*HEIGHT;i++){
			out.data[i] = 0;
		}
		#endif


		ClearTimer(0);
		StartTimer(0);
		FLAG_Ready = 1;
	}
	//********** end initialize (core0) **********

	// wait to start;
	while(!FLAG_Ready) { 
		// do nothing 
	}	

	//********** enter the task processing loop **********
	ProcessTasks();

	// finished
	if(corenum==0) {
		StopTimer(0);
	}

	// final output
	#if PRINT_RESULTS
	if(corenum==0){
	 	for( i=0; i<WIDTH*HEIGHT; i++ ) {
			RigelPrint( out.data[i] );			
	 	}	
	}
	#endif

	return 0;
}
////////////////////////////////////////////////////////////////////////////////




