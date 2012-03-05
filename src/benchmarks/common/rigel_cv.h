



////////////////////////////////////////////////////////////////////////////////
// rigel_cv_flags_t
////////////////////////////////////////////////////////////////////////////////
typedef enum {

	CV_FLAGS_NULL

} rigel_cv_flags_t;
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Convolve2D()
////////////////////////////////////////////////////////////////////////////////
// compute a 2D convolution borders are undefined
////////////////////////////////////////////////////////////////////////////////
void Convolve2D( rigelmatrix_t* in, 
								 rigelmatrix_t* kern, 
								 rigelmatrix_t* out, 
								 rigel_cv_flags_t flags	 	
							 )
{
	int i,j,kr,kc,sum;

	assert( kern->width == kern->height ); // square kernels

	// iterate across all non-border pixels
	// border pixels are ignored (output is undefined)
	for (i = kern->height/2; i < (in->height - kern->height/2); i++) { // rows
		for (j = kern->width/2; j < (in->width - kern->width/2); j++) {  // columns

			// compute an output point under current window
			sum = 0;
			for ( kr = 0; kr < kern->height; kr++ ){
				for ( kc = 0; kc < kern->width; kc++ ){
					sum += kern->data[ INDEX(kr,kc,kern->width) ] * in->data[ INDEX(j+kr,i+kc,in->width) ];
				}		
			}
			out->data[ INDEX(i,j,in->width) ] = sum;
			printf("sum: %d \n",sum);

		}
	}




	return;
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// AbsSum()
////////////////////////////////////////////////////////////////////////////////
// Computes the sum of absolute values for the two input matricies
// |Out| = |In1| + |In2|
////////////////////////////////////////////////////////////////////////////////
void AbsSum( rigelmatrix_t* In1,
						 rigelmatrix_t* In2,
						 rigelmatrix_t* Out
						)
{
	// computes AbsSum linearly
	int i;
	for(i=0; i<In1->size; i++) {
		MAT_TYPE sum = abs(In1->data[i]) + abs(In2->data[i]);	
		Out->data[i] = sum;
	}
	return;
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Threshold()
////////////////////////////////////////////////////////////////////////////////
// Clamps values below threshold to 0, greater than or equal to threshold to max 
////////////////////////////////////////////////////////////////////////////////
void Threshold( rigelmatrix_t* In, rigelmatrix_t* Out, int t, int max) {
	int i;
	for(i=0; i<In->size; i++) {
		if( In->data[i] < t ) { 
			Out->data[i] = 0;
		} else {
			Out->data[i] = max;
		}
	}
	return;
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// PrintMatrix()
////////////////////////////////////////////////////////////////////////////////
// print a rigelmatrix_t 
////////////////////////////////////////////////////////////////////////////////
void PrintMatrix( rigelmatrix_t* mat ) {
	int i;
	for (i = 0; i < mat->size; i++) {
		fprintf( stdout, "%08d " , mat->data[i] );
		if( (i%mat->width) == (mat->width-1) ) {
			fprintf(stdout,"\n\n");
		}
	}
}
////////////////////////////////////////////////////////////////////////////////

// print a rigelmatrix_t with floating pt elements
////////////////////////////////////////////////////////////////////////////////
void PrintMatrixFlt( rigelmatrix_t* mat ) {
	int i;
	for (i = 0; i < mat->size; i++) {
		fprintf( stdout, "%08f " , mat->data[i] );
		if( (i%mat->width) == (mat->width-1) ) {
			fprintf(stdout,"\n\n");
		}
	}
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// histogram()
////////////////////////////////////////////////////////////////////////////////
// perform a simple histogramming operation on an input image
// input:  rigelmatrix_t mat- pointer to image data
//         int* hist      - pointer to histogram
// output: 
////////////////////////////////////////////////////////////////////////////////
void histogram( const rigelmatrix_t* mat, int* hist )
{
	int i=0;
	for(i=0; i<mat->size; i++) {
		hist[ (int)mat->data[i] ] += 1; // increment histogram bin
	}
	return;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// generate_random_matrix()
////////////////////////////////////////////////////////////////////////////////
// generate a random matrix
// input:
// output: 
////////////////////////////////////////////////////////////////////////////////
void generate_random_matrix( rigelmatrix_t* mat, unsigned int size_mask )
{
	int i=0;
	for(i=0; i<mat->size; i++) {
		mat->data[i] = rand() & size_mask;
	}
	return;
}
////////////////////////////////////////////////////////////////////////////////

