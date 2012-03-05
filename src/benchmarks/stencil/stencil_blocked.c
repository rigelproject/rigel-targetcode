//Performs one timestep of a 7-point stencil computation over a portion of the 3D grid
//This fits into the overall multi-core, multi-timestep framework as follows:
//Each core will figure out what portion of the 3D grid it will solve external to the function,
//and pass in lower and upper loop bounds for the x, y, and z dimensions as arguments.
//After calling this function and computing its part of the output array, the core will flush its output data
//to ensure its coherence across the chip, then enter a barrier.  This StencilProbe()->flush->barrier loop will 
//occur a number of times equal to the number of timesteps specified.
//After all cores have entered the barrier
void stencil_1ts(float *curr, float *next, int xlow, int xhigh, int ylow, int yhigh, int zlow, int zhigh) {
  // Fool compiler so it doesn't insert a constant here
  float fac = curr[0];
  int i, j, k;

  assert(xlow >= 0);
  assert(xhigh >= 0);
  assert(ylow >= 0);
  assert(yhigh >= 0);
  assert(zlow >= 0);
  assert(zhigh >= 0);

  for (k = zlow; k < zhigh; k++) {
    for (j = ylow; j < yhigh; j++) {
      for (i = xlow; i < xhigh; i++) {
        //if(Index3D(nx,ny,i+1,j+1,k+1) >= nx*ny*nz)
        //  printf("!");
        //if(Index3D(nx,ny,i-1,j-1,k-1) < 0)
        //  printf("?");
        next[Index3D (nx, ny, i, j, k)] = 
        curr[Index3D (nx, ny, i, j, k + 1)] +
        curr[Index3D (nx, ny, i, j, k - 1)] +
        curr[Index3D (nx, ny, i, j + 1, k)] +
        curr[Index3D (nx, ny, i, j - 1, k)] +
        curr[Index3D (nx, ny, i + 1, j, k)] +
        curr[Index3D (nx, ny, i - 1, j, k)] 
        - 6.0f * curr[Index3D (nx, ny, i, j, k)] / (fac*fac);
      }
    }
  }
}

void stencil_1ts_writeback(float *curr, float *next, int xlow, int xhigh, int ylow, int yhigh, int zlow, int zhigh) {
  int j, k;
  for (k = zlow; k < zhigh; k++)
  {
    for (j = ylow; j < yhigh; j++)
    {
      float *addr = &(next[Index3D (nx, ny, xlow, j, k)]);
      float *end = addr + (xhigh-xlow);
      while(addr < end)
      {
        //RigelPrint(addr);
        RigelWritebackLine(addr);
        addr += 8;
      }
      //RigelPrint(end);
      RigelWritebackLine(end);
    }
  }
}
void stencil_1ts_invalidate(float *curr, float *next, int xlow, int xhigh, int ylow, int yhigh, int zlow, int zhigh) {
  int j, k;
  for (k = zlow-1; k < zhigh+1; k++)
  {
    for (j = ylow-1; j < yhigh+1; j++)
    {
      float *addr = &(curr[Index3D (nx, ny, (xlow-1), j, k)]);
      float *end = addr + (xhigh-xlow) + 2;
      //Only need one invalidate per cache line, but we may skip the last line if
      //the alignment is off, so do an extra one after the for loop.
      while(addr < end)
      {
        RigelInvalidateLine(addr);
        addr += 8;
      }
      RigelInvalidateLine(end);
    }
  }
}
