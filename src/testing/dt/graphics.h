/* 
 * GraphicsGems.h  
 * Version 1.0 - Andrew Glassner
 * from "Graphics Gems", Academic Press, 1990
 */

/*
 * Modified on 26/12/92 to avoid redefinition of some macros.
 */

#define GG_H 1

/*********************/
/* 2d geometry types */
/*********************/

typedef struct Point2Struct {   /* 2d point */
	float x, y;
 	} Point2;
typedef Point2 Vector2;

typedef struct IntPoint2Struct {        /* 2d integer point */
	int x, y;
	} IntPoint2;

typedef struct Matrix3Struct {  /* 3-by-3 matrix */
	float element[3][3];
	} Matrix3;

typedef struct Box2dStruct {            /* 2d box */
	Point2 min, max;
	} Box2;
	

/*********************/
/* 3d geometry types */
/*********************/

typedef struct Point3Struct {   /* 3d point */
	float x, y, z;
	} Point3;
typedef Point3 Vector3;

typedef struct IntPoint3Struct {        /* 3d integer point */
	int x, y, z;
	} IntPoint3;


typedef struct Matrix4Struct {  /* 4-by-4 matrix */
	float element[4][4];
	} Matrix4;

typedef struct Box3dStruct {            /* 3d box */
	Point3 min, max;
	} Box3;



/***********************/
/* one-argument macros */
/***********************/

/* absolute value of a */
//#define abs(a)          (((a)<0) ? -(a) : (a))

#define ABS(a)          (((a)<0) ? -(a) : (a))

/* round a to nearest integer towards 0 */
#define FLOOR(a)                ((a)>0 ? (int)(a) : -(int)(-a))

/* round a to nearest integer away from 0 */
#define CEILING(a) \
((a)==(int)(a) ? (a) : (a)>0 ? 1+(int)(a) : -(1+(int)(-a)))

/* round a to nearest int */
#define ROUND(a)        ((a)>0 ? (int)(a+0.5) : -(int)(0.5-a))          

/* take sign of a, either -1, 0, or 1 */
#define ZSGN(a)         (((a)<0) ? -1 : (a)>0 ? 1 : 0)  

/* take binary sign of a, either -1, or 1 if >= 0 */
#define SGN(a)          (((a)<0) ? -1 : 0)

/* shout if something that should be true isn't */
/*
#define ASSERT(x) \
if (!(x)) fprintf(stderr," Assert failed: x\n");
*/

/* square a */
#define SQR(a)          ((a)*(a))       


/***********************/
/* two-argument macros */
/***********************/

/* find minimum of a and b */

#ifndef MIN
#define MIN(a,b)	(((a)<(b))?(a):(b))
#endif
#ifndef min
#define min(a,b)        (((a)<(b))?(a):(b))     
#endif


/* find maximum of a and b */

#ifndef MAX
#define MAX(a,b)        (((a)>(b))?(a):(b))     
#endif
#ifndef max
#define max(a,b)        (((a)>(b))?(a):(b))     
#endif

/* swap a and b (see Gem by Wyvill) */
#ifndef SWAP
#define SWAP(a,b)       { a^=b; b^=a; a^=b; }
#endif

/* linear interpolation from l (when a=0) to h (when a=1)*/
/* (equal to (a*h)+((1-a)*l) */
#define LERP(a,l,h)     ((l)+(((h)-(l))*(a)))

/* clamp the input to the specified range */
#define CLAMP(v,l,h)    ((v)<(l) ? (l) : (v) > (h) ? (h) : v)


/****************************/
/* memory allocation macros */
/****************************/

/* create a new instance of a structure (see Gem by Hultquist) */
#define NEWSTRUCT(x)    (struct x *)(malloc((unsigned)sizeof(struct x)))

/* create a new instance of a type */
#define NEWTYPE(x)      (x *)(malloc((unsigned)sizeof(x)))


/********************/
/* useful constants */
/********************/

#define PI              3.141592        /* the venerable pi */
#define PITIMES2        6.283185        /* 2 * pi */
#define PIOVER2         1.570796        /* pi / 2 */
#define E               2.718282        /* the venerable e */
#define SQRT2           1.414214        /* sqrt(2) */
#define SQRT3           1.732051        /* sqrt(3) */
#define GOLDEN          1.618034        /* the golden ratio */
#define DTOR            0.017453        /* convert degrees to radians */
#define RTOD            57.29578        /* convert radians to degrees */


/************/
/* booleans */
/************/

#define TRUE            1
#define FALSE           0
#define ON              1
#define OFF             0
/* typedef int boolean; */                      /* boolean data type */
/* typedef boolean flag; */                     /* flag data type */
/*
float V2SquaredLength(), V2Length();
float V2Dot(), V2DistanceBetween2Points();
Vector2 *V2Negate(), *V2Normalize(), 
*V2Scale(), *V2Add(), *V2Sub();
Vector2 *V2Lerp(), *V2Combine(), *V2Mul(), *V2MakePerpendicular();
Vector2 *V2New(), *V2Duplicate();
Point2 *V2MulPointByMatrix();
Matrix3 *V2MatMul();

float V3SquaredLength(), V3Length();
float  V3Dot(),
	V3SquaredDistanceBetween2Points(),
	V3DistanceBetween2Points();

Vector3 *V3Normalize(), *V3Scale(),
	*V3Add(),
	*V3Sub();

Vector3 *V3Lerp(), *V3Combine(), *V3Mul(), *V3Cross();
Vector3 *V3Negate(),*V3New(), *V3Duplicate();
Point3 *V3MulPointByMatrix();
Matrix4 *V3MatMul();
*/
float V3SquaredLength(Vector3 *a);

/* returns length of input vector */
float V3Length(Vector3 *a);

/* negates the input vector and returns it */
Vector3 *V3Negate(Vector3 *v);

/* normalizes the input vector and returns it */
Vector3 *V3Normalize(Vector3 *v);

/* scales the input vector to the new length and returns it */
Vector3 *V3Scale(Vector3 *v, float newlen);

/* return vector sum c = a+b */
Vector3 *V3Add(Vector3 *a, Vector3 *b, Vector3 *c);

/* return vector difference c = a-b */
Vector3 *V3Sub(Vector3 *a, Vector3 *b, Vector3 *c);

/* return the dot product of vectors a and b */
float V3Dot(Vector3 *a, Vector3 *b);

/* linearly interpolate between vectors by an amount alpha */
/* and return the resulting vector. */
/* When alpha=0, result=lo.  When alpha=1, result=hi. */
Vector3 *V3Lerp(Vector3 *lo, Vector3 *hi,float  alpha, Vector3 *result);

/* make a linear combination of two vectors and return the result. */
/* result = (a * ascl) + (b * bscl) */
Vector3 *V3Combine (Vector3 *a, Vector3 *b, Vector3 *result, float ascl, float bscl);
                                                                                            
/* multiply two vectors together component-wise and return the result */
Vector3 *V3Mul (Vector3 *a, Vector3 *b, Vector3 *result);

/* return the distance between two points */
float V3DistanceBetween2Points(Point3 *a, Point3 *b);

/* return the distance between two points */
float V3SquaredDistanceBetween2Points(Point3 *a, Point3 *b);

/* return the cross product c = a cross b */
Vector3 *V3Cross(Vector3 *a, Vector3 *b, Vector3 *c);

/* create, initialize, and return a new vector */
Vector3 *V3New(float x, float y, float z);

Vector3 *V3Duplicate(Vector3 *a);

/* multiply a point by a matrix and return the transformed point */
Point3 *V3MulPointByMatrix(Point3 *p, Matrix4 *m);

/* multiply together matrices c = ab */
/* note that c must not point to either of the input matrices */
Matrix4 *V3MatMul(Matrix4 *a, Matrix4 *b, Matrix4 *c);


