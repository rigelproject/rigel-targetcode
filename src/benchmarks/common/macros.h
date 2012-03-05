#define ALIGNED_MALLOC(numelements, type, alignmentshift, alignedptr, baseptr) do { \
    baseptr = (type *) malloc((numelements)*sizeof(*(baseptr)) + ((1 << (alignmentshift))-1)); \
    if((baseptr) == NULL) \
    { \
        printf("Error malloc'ing %d elements of type " #type " aligned to 2^%d-byte boundaries\n", (numelements), (alignmentshift)); \
        RigelAbort(); \
    } \
    alignedptr = (type *)((((unsigned int)(baseptr)) + ((1 << (alignmentshift)) - 1)) & (0xFFFFFFFFU ^ ((1 << (alignmentshift)) - 1))); \
} while(0)

#define abs(x) ( x<0 ? (-x) : x )
