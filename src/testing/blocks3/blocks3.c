#include <stdlib.h>
#include <Block.h>
#include <rigel.h> 

typedef void (^UpdateBlock)(void);
 
UpdateBlock CreateUpdateBlock(float *array, int i) {
    UpdateBlock b = ^{ array[i] *= 2.0; };
    return Block_copy(b);
}
 
void ReleaseUpdateBlock(UpdateBlock b) {
    Block_release(b);
}
 
void DispatchBlocks(UpdateBlock *updateBlocks, int n)
{
    int i;
    #pragma omp parallel for
    for ( i = 0; i < n; ++i ) updateBlocks[i]();
}
 
int main(void) {
    const int N = 1000;
    float a[N];
    UpdateBlock blocks[N];
    ClearTimer(0);
    ClearTimer(1);
    ClearTimer(2);
    ClearTimer(3);
    StartTimer(0);
    // Initialize array
    int i;
    for ( i = 0; i < N; ++i ) a[i] = (float)i;
    StopTimer(0);
    StartTimer(1);
    // Create blocks
    for ( i = 0; i < N; ++i ) blocks[i] = CreateUpdateBlock(a, i);
    StopTimer(1);
    StartTimer(2);
    // Dispatch and run blocks
    DispatchBlocks(blocks, N);
    StopTimer(2);
    StartTimer(3);
    // Release blocks
    for ( i = 0; i < N; ++i ) ReleaseUpdateBlock(blocks[i]);
    StopTimer(3);
    return 0;
}

