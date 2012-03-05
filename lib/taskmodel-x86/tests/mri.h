#define FPTYPE float
#define SCALAR_COS cosf
#define SCALAR_SIN sinf
#define MAX_TASKS 1024*1024
#define QID 1

#define DEBUG_PRINT 0
#define PRINT_SAMPLE_RATE 8192
#define K_ELEMS_PER_GRID 2048

#define PI   3.1415926535897932384626433832795029f
#define PIx2 6.2831853071795864769252867665590058f

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

typedef struct kValues {
  FPTYPE Kx;
  FPTYPE Ky;
  FPTYPE Kz;
  FPTYPE RealRhoPhi;
  FPTYPE ImagRhoPhi;
} kValues;

typedef struct fhparams {
  int threadid;
  int threadcount;
  int totalNumK;
  int threadBaseX;
  int threadNumX;
  volatile struct kValues* kVals;
  volatile FPTYPE* x;
  volatile FPTYPE* y;
  volatile FPTYPE* z;
  volatile FPTYPE* outR;
  volatile FPTYPE* outI;
} fhparams;

// thread prototype
void* taskFH(int task_num);
void GenerateTasks(int num_tasks);
