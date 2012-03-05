#ifndef __CGSOLVER_H__
#define __CGSOLVER_H__
#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>
//
// Type used in all calculations (int can be used as a check)
//
typedef float ElemType;

//
// Type used to hold pair lists for iter_task_list
//
typedef struct task_list_entry_t {
  int start;
  int count;
} task_list_entry_t;
// Number of task_list_entry_t per core.   Since we assign one iter or more per
// tasks, we are probably going to be able to support upto 8*MAX_TASKLIST_ENTRY
// rows in the matrix which is *a lot*
#define MAX_TASKLIST_ENTRY 1024

//
// Actual Data Structure
//
typedef struct SparseMatrix {
	// Arrays that hold indices into array
	int *IA, *IJ;
	// Actual array data
	ElemType *MatData;
	int NumRows;
	int NumCols;
	int NonZeroes;
} SparseMatrix;

typedef struct ElemTypeListEntry {
	ElemType value;
	int row;
	int col;
	char space[4];
} ElemTypeListEntry;

void 
SMatInit(SparseMatrix *A, int rows, int cols, const ElemTypeListEntry *list);
	
ElemType 
SMatGetElem(const SparseMatrix *A, int row, int idx, int *col);

int
SMatGetRowSize(const SparseMatrix *A, int row);

void
ElemTypeListEntrySet(ElemTypeListEntry *elem, int row, int col, ElemType value);

int 
SMatGetRowSize(const SparseMatrix *A, int row);
void 
ElemTypeListEntrySet(ElemTypeListEntry *elem, int row, int col, ElemType value);
ElemType 
SMatGetElem(const SparseMatrix *A, int row, int idx, int *col);
void 
MatVecMul(const SparseMatrix *A, const ElemType *vecn, ElemType *vec_out);
void 
Task_SMVMRows(const SparseMatrix *A, const ElemType *vec_in, ElemType *vec_out, TQ_TaskDesc *tdesc);
void
Task_VecDot(const ElemType *a, const ElemType *b, int dim, ElemType *ret);
void 
Task_VecVecAdd(const ElemType *a, const ElemType *b, ElemType *out, int dim);
void
Task_VecScaleAdd(const ElemType scale, const ElemType *a, const ElemType *b, ElemType *out, int dim);
void
Task_VecScaleAdd(const ElemType scale, const ElemType *a, const ElemType *b, ElemType *out, int dim);

// START: GLOBALS
extern int MATRIX_DIM;
extern int NONZERO_ELEMS;
extern int MAXITER;
extern ElemType * b;
extern SparseMatrix Amat;
extern ElemType * x;
extern ElemType * res;
extern ElemType *temp;
extern ElemType *p;
extern ElemType *q;
extern volatile ElemType rho;
extern ElemTypeListEntry *elem_list;
extern ElemType *__MatData;
// Accumulated error only written by core zero
extern ElemType error;
extern int *__IA;
extern int *__IJ;
extern volatile unsigned int GlobalReduceClustersDone;
extern volatile ElemType GlobalReduceReturn;
extern ElemType *ReduceVals;

// Global QID since we all use the same queue
#define QID 0
#define MAX_TASKS 1024*1024

#endif
