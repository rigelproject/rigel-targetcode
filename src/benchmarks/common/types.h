
// just some common macros used in benchmarks
// Compute the linearized index into a "2D" array
#define INDEX(row,col,width) ( (row)*(width) + (col) )
/*
 Rigel Matrix
#define RIGELMAT_T( TYPE )  \
struct { \
	TYPE* data; \       // pointer to matrix data
	uint32_t width;  \  // number of elements per row
	uint32_t height; \  // number of elements per row
	uint32_t size;   \  // total number of elements = width * height
} \
*/
////////////////////////////////////////////////////////////////////////////////
// Rigel Matrix
////////////////////////////////////////////////////////////////////////////////
typedef struct { 
	MAT_TYPE* data; // pointer to matrix data 
	int width;      // number of elements per row 
	int height;     // number of elements per column 
	int size;       // total number of elements = width * height 
	// member functions not valid in C
	//MAT_TYPE get(int r, int c) {
	//	return data[ INDEX(r,c,width) ];
	//};
	//void set(int r, int c, int val) {
	//	data[ INDEX(r,c,width) ] = val;
	//};
} rigelmatrix_t;

// Rigel Matrix helpers (TODO: macros)
inline MAT_TYPE GET(rigelmatrix_t* m, int r, int c) {
	return m->data[ INDEX(r,c,m->width) ];
}
inline void SET(rigelmatrix_t* m, int r, int c, MAT_TYPE val) {
	m->data[ INDEX(r,c,m->width) ] = val;
}
