//
// Marching Cubes Example Program
// by Cory Bloyd (dejaspaminacan@my-deja.com)
//
// A simple, portable and complete implementation of the Marching Cubes
// and Marching Tetrahedrons algorithms in a single source file.
// There are many ways that this code could be made faster, but the
// intent is for the code to be easy to understand.
//
// For a description of the algorithm go to
// http://www.swin.edu.au/astronomy/pbourke/modelling/polygonise/
//
// This code is public domain.
//
#include "../common/common.h"
#include "rigel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <incoherent_malloc.h>
#include <spinlock.h>
#include "rigel-tasks.h"

#define MIN(x,y) (((x)>(y)) ? (y) : (x))
#define MAX(x,y) (((x)>(y)) ? (x) : (y))

//Uncomment this to dump a binary file at the end of the simulation
//Containing all triangles found by marching cubes.
//bin_to_obj can be used to turn this binary file into a
//.obj file for use in a 3D mesh viewer (e.g., MeshLab).
//#define DUMP_TRIANGLES

ATOMICFLAG_INIT_CLEAR(flag);
int incoherent_malloc_enabled;
const int QID = 0;

// From common.h
int TCMM_INV_LAZY;
int TCMM_INV_EAGER;
int TCMM_WRB_LAZY;
int TCMM_WRB_EAGER;


unsigned int nx, ny, nz, bx, by, bz, sx, sy, sz;
float   *datamat;
float   ***data;
float   *outdata;
float   **triangles;
float   **trianglePtrs;
float   fStepSize;
float   fTargetValue;

typedef struct
{
  float fX;
  float fY;
  float fZ;
} vector;

//These tables are used so that everything can be done in little loops that you can look at all at once
// rather than in pages and pages of unrolled code.

//a2fVertexOffset lists the positions, relative to vertex0, of each of the 8 vertices of a cube
static const float a2fVertexOffset[8][3] =
{
  {0.0f, 0.0f, 0.0f},{1.0f, 0.0f, 0.0f},{1.0f, 1.0f, 0.0f},{0.0f, 1.0f, 0.0f},
  {0.0f, 0.0f, 1.0f},{1.0f, 0.0f, 1.0f},{1.0f, 1.0f, 1.0f},{0.0f, 1.0f, 1.0f}
};

static const unsigned int a2uVertexOffset[8][3] =
{
  {0, 0, 0},{1, 0, 0},{1, 1, 0},{0, 1, 0},
  {0, 0, 1},{1, 0, 1},{1, 1, 1},{0, 1, 1}
};

//a2iEdgeConnection lists the index of the endpoint vertices for each of the 12 edges of the cube
static const int a2iEdgeConnection[12][2] =
{
  {0,1}, {1,2}, {2,3}, {3,0},
  {4,5}, {5,6}, {6,7}, {7,4},
  {0,4}, {1,5}, {2,6}, {3,7}
};

//a2fEdgeDirection lists the direction vector (vertex1-vertex0) for each edge in the cube
static const float a2fEdgeDirection[12][3] =
{
  {1.0f, 0.0f, 0.0f},{0.0f, 1.0f, 0.0f},{-1.0f, 0.0f, 0.0f},{0.0f, -1.0f, 0.0f},
  {1.0f, 0.0f, 0.0f},{0.0f, 1.0f, 0.0f},{-1.0f, 0.0f, 0.0f},{0.0f, -1.0f, 0.0f},
  {0.0f, 0.0f, 1.0f},{0.0f, 0.0f, 1.0f},{ 0.0f, 0.0f, 1.0f},{0.0f,  0.0f, 1.0f}
};

static const int a2iEdgeDirection[12][3] =
{
  {1, 0, 0},{0, 1, 0},{-1, 0, 0},{0, -1, 0},
  {1, 0, 0},{0, 1, 0},{-1, 0, 0},{0, -1, 0},
  {0, 0, 1},{0, 0, 1},{ 0, 0, 1},{0,  0, 1}
};

//a2iTetrahedronEdgeConnection lists the index of the endpoint vertices for each of the 6 edges of the tetrahedron
static const int a2iTetrahedronEdgeConnection[6][2] =
{
  {0,1},  {1,2},  {2,0},  {0,3},  {1,3},  {2,3}
};

float fSample(float fX, float fY, float fZ);
float fGridSample(unsigned int fX, unsigned int fY, unsigned int fZ);

void vMarchingCubes();
void vMarchCube1(float fX, float fY, float fZ, float fScale);
void vMarchCubeGrid(unsigned int fX, unsigned int fY, unsigned int fZ);
void vMarchCube2(float fX, float fY, float fZ, float fScale);
void (*vMarchCube)(float fX, float fY, float fZ, float fScale) = vMarchCube1;


//fGetOffset finds the approximate point of intersection of the surface
// between two points with the values fValue1 and fValue2
float fGetOffset(float fValue1, float fValue2, float fValueDesired)
{
  float fDelta = fValue2 - fValue1;

  if(fDelta == 0.0f)
  {
    return 0.5f;
  }
  return (fValueDesired - fValue1)/fDelta;
}


void vNormalizeVector(vector *rfVectorResult, vector *rfVectorSource)
{
  float fOldLength;
  float fScale;

  fOldLength = sqrtf( (rfVectorSource->fX * rfVectorSource->fX) +
          (rfVectorSource->fY * rfVectorSource->fY) +
          (rfVectorSource->fZ * rfVectorSource->fZ) );

  if(fOldLength == 0.0f)
  {
    rfVectorResult->fX = rfVectorSource->fX;
    rfVectorResult->fY = rfVectorSource->fY;
    rfVectorResult->fZ = rfVectorSource->fZ;
  }
  else
  {
    fScale = 1.0f/fOldLength;
    rfVectorResult->fX = rfVectorSource->fX*fScale;
    rfVectorResult->fY = rfVectorSource->fY*fScale;
    rfVectorResult->fZ = rfVectorSource->fZ*fScale;
  }
}

float fSample(float fX, float fY, float fZ)
{
  //return (6.0f*fX - 3.0f)*sinf(6.0f*fY - 3.0f) + sinf(powf(0.5f*fZ - 2.0f, 3.0f));
  return ((fX*fX)+(fY*fY)+(fZ*fZ)-(2.0f*2.0f));
}

float fGridSample(unsigned int fX, unsigned int fY, unsigned int fZ)
{

  //if(fX > nx || fY > ny || fZ > nz)
  //{
  //  printf("%u %u %u\n", fX, fY, fZ);
  //  RigelAbort();
  //}
  //printf("%f\n", data[fZ][fX][fY]);
  return data[fZ][fX][fY];
}

//vGetNormal() finds the gradient of the scalar field at a point
//This gradient can be used as a very accurate vertex normal for lighting calculations
void vGetNormal(vector *rfNormal, float fX, float fY, float fZ)
{
  rfNormal->fX = fSample(fX-0.01f, fY, fZ) - fSample(fX+0.01f, fY, fZ);
  rfNormal->fY = fSample(fX, fY-0.01f, fZ) - fSample(fX, fY+0.01f, fZ);
  rfNormal->fZ = fSample(fX, fY, fZ-0.01f) - fSample(fX, fY, fZ+0.01f);
  vNormalizeVector(rfNormal, rfNormal);
}


//vMarchCube1 performs the Marching Cubes algorithm on a single cube
void vMarchCube1(float fX, float fY, float fZ, float fScale)
{
  extern int aiCubeEdgeFlags[256];
  extern int a2iTriangleConnectionTable[256][16];
  int thread = RigelGetThreadNum();

  int iCorner, iVertex, iVertexTest, iEdge, iTriangle, iFlagIndex, iEdgeFlags;
  float fOffset;
  //vector sColor;
  float afCubeValue[8];
  vector asEdgeVertex[12];
  //vector asEdgeNorm[12];

  //Make a local copy of the values at the cube's corners
  for(iVertex = 0; iVertex < 8; iVertex++)
  {
    afCubeValue[iVertex] = fGridSample(fX + a2fVertexOffset[iVertex][0]*fScale,
               fY + a2fVertexOffset[iVertex][1]*fScale,
               fZ + a2fVertexOffset[iVertex][2]*fScale);
  }

  //Find which vertices are inside of the surface and which are outside
  iFlagIndex = 0;
  for(iVertexTest = 0; iVertexTest < 8; iVertexTest++)
  {
    if(afCubeValue[iVertexTest] <= fTargetValue)
      iFlagIndex |= 1<<iVertexTest;
  }

  //Find which edges are intersected by the surface
  iEdgeFlags = aiCubeEdgeFlags[iFlagIndex];

  //If the cube is entirely inside or outside of the surface, then there will be no intersections
  if(iEdgeFlags == 0)
  {
    return;
  }

  //Find the point of intersection of the surface with each edge
  //Then find the normal to the surface at those points
  for(iEdge = 0; iEdge < 12; iEdge++)
  {
    //if there is an intersection on this edge
    if(iEdgeFlags & (1<<iEdge))
    {
      fOffset = fGetOffset(afCubeValue[ a2iEdgeConnection[iEdge][0] ],
                 afCubeValue[ a2iEdgeConnection[iEdge][1] ], fTargetValue);

      asEdgeVertex[iEdge].fX = fX + (a2fVertexOffset[ a2iEdgeConnection[iEdge][0] ][0]  +  fOffset * a2fEdgeDirection[iEdge][0]) * fScale;
      asEdgeVertex[iEdge].fY = fY + (a2fVertexOffset[ a2iEdgeConnection[iEdge][0] ][1]  +  fOffset * a2fEdgeDirection[iEdge][1]) * fScale;
      asEdgeVertex[iEdge].fZ = fZ + (a2fVertexOffset[ a2iEdgeConnection[iEdge][0] ][2]  +  fOffset * a2fEdgeDirection[iEdge][2]) * fScale;

      //vGetNormal(&(asEdgeNorm[iEdge]), asEdgeVertex[iEdge].fX, asEdgeVertex[iEdge].fY, asEdgeVertex[iEdge].fZ);
    }
  }


  //Draw the triangles that were found.  There can be up to five per cube
  for(iTriangle = 0; iTriangle < 5; iTriangle++)
  {
    if(a2iTriangleConnectionTable[iFlagIndex][3*iTriangle] < 0)
      break;
    for(iCorner = 0; iCorner < 3; iCorner++)
    {
      iVertex = a2iTriangleConnectionTable[iFlagIndex][3*iTriangle+iCorner];
      *(trianglePtrs[thread]++) = asEdgeVertex[iVertex].fX;
      *(trianglePtrs[thread]++) = asEdgeVertex[iVertex].fY;
      *(trianglePtrs[thread]++) = asEdgeVertex[iVertex].fZ;
      //vGetColor(sColor, asEdgeVertex[iVertex], asEdgeNorm[iVertex]);
      //glColor3f(sColor.fX, sColor.fY, sColor.fZ);
      //glNormal3f(asEdgeNorm[iVertex].fX,   asEdgeNorm[iVertex].fY,   asEdgeNorm[iVertex].fZ);
      //glVertex3f(asEdgeVertex[iVertex].fX, asEdgeVertex[iVertex].fY, asEdgeVertex[iVertex].fZ);
    }
  }
}

//vMarchCube1 performs the Marching Cubes algorithm on a single cube
void vMarchCubeGrid(unsigned int fX, unsigned int fY, unsigned int fZ)
{
  extern int aiCubeEdgeFlags[256];
  extern int a2iTriangleConnectionTable[256][16];
  int thread = RigelGetThreadNum();

  //RigelPrint((fX << 16) | (fY << 8) | fZ);

  int iCorner, iVertex, iVertexTest, iEdge, iTriangle, iFlagIndex, iEdgeFlags;
  float fOffset;
  //vector sColor;
  float afCubeValue[8];
  vector asEdgeVertex[12];
  //vector asEdgeNorm[12];

  //Make a local copy of the values at the cube's corners
  for(iVertex = 0; iVertex < 8; iVertex++)
  {
    afCubeValue[iVertex] = fGridSample(fX + a2uVertexOffset[iVertex][0],
               fY + a2uVertexOffset[iVertex][1],
               fZ + a2uVertexOffset[iVertex][2]);
  }

  //Find which vertices are inside of the surface and which are outside
  iFlagIndex = 0;
  for(iVertexTest = 0; iVertexTest < 8; iVertexTest++)
  {
    if(afCubeValue[iVertexTest] <= fTargetValue)
      iFlagIndex |= 1<<iVertexTest;
  }

  //Find which edges are intersected by the surface
  iEdgeFlags = aiCubeEdgeFlags[iFlagIndex];

  //If the cube is entirely inside or outside of the surface, then there will be no intersections
  if(iEdgeFlags == 0)
  {
    return;
  }

  //Find the point of intersection of the surface with each edge
  //Then find the normal to the surface at those points
  for(iEdge = 0; iEdge < 12; iEdge++)
  {
    //if there is an intersection on this edge
    if(iEdgeFlags & (1<<iEdge))
    {
      fOffset = fGetOffset(afCubeValue[ a2iEdgeConnection[iEdge][0] ],
                 afCubeValue[ a2iEdgeConnection[iEdge][1] ], fTargetValue);

      asEdgeVertex[iEdge].fX = fX + (a2fVertexOffset[ a2iEdgeConnection[iEdge][0] ][0]  +  fOffset * a2fEdgeDirection[iEdge][0]);
      asEdgeVertex[iEdge].fY = fY + (a2fVertexOffset[ a2iEdgeConnection[iEdge][0] ][1]  +  fOffset * a2fEdgeDirection[iEdge][1]);
      asEdgeVertex[iEdge].fZ = fZ + (a2fVertexOffset[ a2iEdgeConnection[iEdge][0] ][2]  +  fOffset * a2fEdgeDirection[iEdge][2]);

      //vGetNormal(&(asEdgeNorm[iEdge]), asEdgeVertex[iEdge].fX, asEdgeVertex[iEdge].fY, asEdgeVertex[iEdge].fZ);
    }
  }


  //Draw the triangles that were found.  There can be up to five per cube
  for(iTriangle = 0; iTriangle < 5; iTriangle++)
  {
    if(a2iTriangleConnectionTable[iFlagIndex][3*iTriangle] < 0)
      break;
    for(iCorner = 0; iCorner < 3; iCorner++)
    {
      iVertex = a2iTriangleConnectionTable[iFlagIndex][3*iTriangle+iCorner];

      *(trianglePtrs[thread]++) = asEdgeVertex[iVertex].fX;
      *(trianglePtrs[thread]++) = asEdgeVertex[iVertex].fY;
      *(trianglePtrs[thread]++) = asEdgeVertex[iVertex].fZ;
      //vGetColor(sColor, asEdgeVertex[iVertex], asEdgeNorm[iVertex]);
      //glColor3f(sColor.fX, sColor.fY, sColor.fZ);
      //glNormal3f(asEdgeNorm[iVertex].fX,   asEdgeNorm[iVertex].fY,   asEdgeNorm[iVertex].fZ);
      //glVertex3f(asEdgeVertex[iVertex].fX, asEdgeVertex[iVertex].fY, asEdgeVertex[iVertex].fZ);
    }
  }
}


//vMarchingCubes iterates over the entire dataset, calling vMarchCube on each cube
void vMarchingCubes()
{
  int iX, iY, iZ;
  for(iX = 0; iX < (nx-1); iX++)
  for(iY = 0; iY < (ny-1); iY++)
  for(iZ = 0; iZ < (nz-1); iZ++)
  {
    vMarchCube(iX, iY, iZ, 1.0f);
    //vMarchCube(iX*fStepSize, iY*fStepSize, iZ*fStepSize, fStepSize);
  }
}


// For any edge, if one vertex is inside of the surface and the other is outside of the surface
//  then the edge intersects the surface
// For each of the 8 vertices of the cube can be two possible states : either inside or outside of the surface
// For any cube the are 2^8=256 possible sets of vertex states
// This table lists the edges intersected by the surface for all 256 possible vertex states
// There are 12 edges.  For each entry in the table, if edge #n is intersected, then bit #n is set to 1

int aiCubeEdgeFlags[256]=
{
  0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c, 0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
  0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c, 0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
  0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c, 0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
  0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac, 0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
  0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c, 0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
  0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc, 0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
  0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c, 0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
  0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc, 0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
  0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc, 0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
  0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c, 0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
  0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc, 0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
  0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c, 0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
  0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac, 0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
  0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c, 0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
  0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c, 0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
  0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c, 0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
};

//  For each of the possible vertex states listed in aiCubeEdgeFlags there is a specific triangulation
//  of the edge intersection points.  a2iTriangleConnectionTable lists all of them in the form of
//  0-5 edge triples with the list terminated by the invalid value -1.
//  For example: a2iTriangleConnectionTable[3] list the 2 triangles formed when corner[0]
//  and corner[1] are inside of the surface, but the rest of the cube is not.
//
//  I found this table in an example program someone wrote long ago.  It was probably generated by hand

int a2iTriangleConnectionTable[256][16] =
{
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
  {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
  {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
  {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
  {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
  {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
  {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
  {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
  {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
  {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
  {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
  {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
  {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
  {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
  {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
  {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
  {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
  {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
  {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
  {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
  {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
  {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
  {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
  {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
  {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
  {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
  {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
  {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
  {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
  {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
  {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
  {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
  {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
  {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
  {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
  {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
  {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
  {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
  {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
  {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
  {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
  {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
  {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
  {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
  {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
  {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
  {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
  {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
  {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
  {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
  {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
  {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
  {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
  {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
  {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
  {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
  {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
  {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
  {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
  {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
  {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
  {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
  {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
  {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
  {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
  {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
  {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
  {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
  {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
  {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
  {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
  {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
  {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
  {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
  {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
  {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
  {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
  {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
  {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
  {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
  {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
  {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
  {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
  {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
  {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
  {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
  {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
  {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
  {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
  {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
  {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
  {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
  {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
  {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
  {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
  {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
  {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
  {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
  {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
  {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
  {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
  {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
  {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
  {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
  {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
  {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
  {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
  {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
  {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
  {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
  {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
  {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
  {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
  {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
  {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
  {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
  {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
  {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
  {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
  {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
  {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
  {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
  {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
  {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
  {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
  {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
  {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
  {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
  {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
  {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
  {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
  {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
  {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
  {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
  {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
  {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
  {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
  {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
  {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
  {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
  {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
  {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
  {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
  {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
  {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
  {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
  {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
  {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
  {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
  {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
  {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
  {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
  {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
  {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
  {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
  {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
  {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
  {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
  {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
  {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
  {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
  {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
  {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
  {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
  {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
  {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
  {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
  {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
  {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

void marchWorkerThread()
{
  TQ_TaskDesc tdesc;
  TQ_RetType retval;

  //unsigned int numXBlocks = ((nx+(bx-1))/bx);
  //unsigned int numYBlocks = ((ny+(by-1))/by);
  //unsigned int numZBlocks = ((nz+(bz-1))/bz);
  //unsigned int numBlocks = numXBlocks*numYBlocks*numZBlocks;

  unsigned int numXBlocks = ((nx+(bx-1)) >> sx);
  unsigned int numYBlocks = ((ny+(by-1)) >> sy);
  unsigned int numZBlocks = ((nz+(bz-1)) / bz);
  unsigned int numBlocks = numXBlocks*numYBlocks*numZBlocks;

  unsigned int numXBlocksTmp = numXBlocks >> 1;
  unsigned int mx = 0x1, sx2 = 0;
  while(numXBlocksTmp)
  {
    sx2++;
    mx <<= 1;
    numXBlocksTmp >>= 1;
  }
  mx--;
  unsigned int numYBlocksTmp = numYBlocks >> 1;
  unsigned int my = 0x1, sy2 = 0;
  while(numYBlocksTmp)
  {
    sy2++;
    my <<= 1;
    numYBlocksTmp >>= 1;
  }
  my--;

  //RigelPrint(0x55555555);
  //RigelPrint(numXBlocks);
  //RigelPrint(numYBlocks);
  //RigelPrint(numZBlocks);
  //RigelPrint(numBlocks);

  if(RIGEL_LOOP(QID, numBlocks, 1, &tdesc) != TQ_RET_SUCCESS)
  {
    printf("Enqueue failed!\n");
    RigelAbort();
  }

  while (1) {
#ifdef FAST_DEQUEUE
    RIGEL_DEQUEUE(QID, &tdesc, &retval);
#else
    retval = RIGEL_DEQUEUE(QID, &tdesc);
#endif
    if (retval == TQ_RET_BLOCK) continue;
    if (retval == TQ_RET_SYNC) break;
    if (retval != TQ_RET_SUCCESS)
    {
      RigelPrint(0xDEADDEAD);
      RigelAbort();
    }
    else
    {
      unsigned int xmin, xmax, ymin, ymax, zmin, zmax;
      unsigned int tmp = tdesc.v3;
      unsigned int xblocknum = tmp & mx;
      tmp >>= sx2;
      unsigned int yblocknum = tmp & my;
      tmp >>= sy2;
      unsigned int zblocknum = tmp;
      xmin = xblocknum << sx;
      xmax = MIN(xmin+bx, (nx-1));
      ymin = yblocknum << sy;
      ymax = MIN(ymin+by, (ny-1));
      zmin = zblocknum << sz;
      zmax = MIN(zmin+bz, (nz-1));

      /*
      unsigned int xblocknum = tdesc.v3 % numXBlocks;
      tdesc.v3 -= xblocknum;
      tdesc.v3 /= numXBlocks;
      unsigned int yblocknum = tdesc.v3 % numYBlocks;
      tdesc.v3 -= yblocknum;
      tdesc.v3 /= numYBlocks;
      unsigned int zblocknum = tdesc.v3;
      xmin = xblocknum*bx;
      xmax = MIN(xmin+bx, (nx-1));
      ymin = yblocknum*by;
      ymax = MIN(ymin+by, (ny-1));
      zmin = zblocknum*bz;
      zmax = MIN(zmin+bz, (nz-1));
      */

			//Print this out to track run progress
      RigelPrint(0xF0F0F0F0);
      //RigelPrint(tdesc.v3);
      //RigelPrint(xblocknum);
      //RigelPrint(yblocknum);
      //RigelPrint(zblocknum);
      //RigelPrint(xmin);
      //RigelPrint(xmax);
      //RigelPrint(ymin);
      //RigelPrint(ymax);
      //RigelPrint(zmin);
      //RigelPrint(zmax);
      unsigned int i,j,k;
      for(i = xmin; i < xmax; i++)
        for(j = ymin; j < ymax; j++)
          for(k = zmin; k < zmax; k++)
            vMarchCubeGrid(i,j,k);
    }
  }
}

int main(int argc, char *argv[])
{
  int thread = RigelGetThreadNum();
  if(thread == 0)
  {
    SIM_SLEEP_ON();
    int threads = RigelGetNumThreads();
    RTM_SetWritebackStrategy();
    char inFilename[256];
    int targetValueInt;
    fscanf(stdin, "%s %d", inFilename, &targetValueInt);
    fTargetValue = (float) targetValueInt;
    fscanf(stdin, "%d %d %d", &bx, &by, &bz);

    FILE *inFile;
    if((inFile = fopen(inFilename, "rb")) == NULL)
    {
      printf("Error: could not open %s\n", inFilename);
      RigelAbort();
    }

		setbuf(inFile, NULL); //Make inFile unbuffered

    int nBytes;
    nBytes = fread(&nx, sizeof(unsigned int), 1, inFile);
    nBytes = fread(&ny, sizeof(unsigned int), 1, inFile);
    nBytes = fread(&nz, sizeof(unsigned int), 1, inFile);

		//Uncomment this to use half the dataset to make runs shorter.
    //nz /= 2;

    printf("Isosurface @ %d.0 from %s\n", targetValueInt, inFilename);
    printf("%d x %d x %d, ", nx, ny, nz);
    printf("%d x %d x %d blocks", bx, by, bz);

    sx = 0;
    unsigned int bxtmp = bx >> 1;
    while(bxtmp)
    {
      sx++;
      bxtmp >>= 1;
    }
    sy = 0;
    unsigned int bytmp = by >> 1;
    while(bytmp)
    {
      sy++;
      bytmp >>= 1;
    }
    sz = 0;
    unsigned int bztmp = bz >> 1;
    while(bztmp)
    {
      sz++;
      bztmp >>= 1;
    }
    //RigelPrint(sx);
    //RigelPrint(sy);
    //RigelPrint(sz);

    float *datamattmp;
    datamattmp=(float*)malloc(sizeof(float)*nx*ny*nz + 2047);
    datamat = (float*)((((unsigned int)datamattmp)+2047) & 0xFFFFF800U);
    nBytes = fread(datamat, sizeof(float)*nx*ny*nz, 1, inFile);
    fclose(inFile);
    data = (float***)malloc(sizeof(*data)*nz);
    unsigned int i, j;
    for(i = 0; i < nz; i++)
    {
      data[i] = (float**) malloc(sizeof(*(data[i]))*nx);
      for(j = 0; j < nx; j++)
      {
        data[i][j] = datamat+(nx*ny)*i+nx*j;
        RigelFlushLine(&(data[i][j]));
      }
      RigelFlushLine(&(data[i]));
    }

    outdata = (float *) malloc(256*1024*1024);
    if(outdata == NULL)
    {
      printf("Malloc of 1GB failed\n");
      RigelAbort();
    }
    printf("Big malloc succeeded!\n");
    triangles = (float **) malloc(threads*sizeof(*triangles));
    for(i = 0; i < threads; i++)
      triangles[i] = outdata + (256/4*1024/threads*1024)*i; 
    trianglePtrs = (float **) malloc(threads*sizeof(*trianglePtrs));
    for(i = 0; i < threads; i++)
    {
      trianglePtrs[i] = triangles[i];
      RigelFlushLine(&(triangles[i]));
      RigelFlushLine(&(trianglePtrs[i]));
    }
    RigelFlushLine(&triangles);
    RigelFlushLine(&trianglePtrs);
    //fStepSize = 4.0f/iDataSetSize;
    RIGEL_INIT(QID, MAX_TASKS);
    SW_TaskQueue_Set_TASK_GROUP_FETCH(4);
    SW_TaskQueue_Set_LOCAL_ENQUEUE_COUNT(4);
    ClearTimer(0);

    RigelFlushLine(&nx);
    RigelFlushLine(&ny);
    RigelFlushLine(&nz);
    RigelFlushLine(&bx);
    RigelFlushLine(&by);
    RigelFlushLine(&bz);
    RigelFlushLine(&sx);
    RigelFlushLine(&sy);
    RigelFlushLine(&sz);
    RigelFlushLine(&fStepSize);
    RigelFlushLine(&fTargetValue);

    printf("Starting\n");
    SIM_SLEEP_OFF();
    StartTimer(0);
    //vMarchingCubes();
    //StopTimer(0);
    atomic_flag_set(&flag);
  }
  else
  {
    atomic_flag_spin_until_set(&flag);
  }
  marchWorkerThread();
  if(thread == 0) {
    StopTimer(0);
#ifdef DUMP_TRIANGLES
		FILE *out = fopen("triangles.out", "wb");
		assert(out && "Could not open output file");
		int numTriangles = 0;
		for(int i = 0; i < RigelGetNumThreads(); i++)
      numTriangles += (trianglePtrs[i] - triangles[i])/9;
    fwrite(&numTriangles, sizeof(numTriangles), 1, out);
		for(int i = 0; i < RigelGetNumThreads(); i++)
			fwrite(triangles[i], sizeof(*(triangles[i])), trianglePtrs[i] - triangles[i], out);
		fclose(out);
#endif //#ifdef DUMP_TRIANGLES
	}
  RigelPrint(trianglePtrs[thread]-triangles[thread]);
  return 0;
}
