//
// rigelsim.h
//
// Defines functions that can be used to speed up IO and data manipulation.
// All of these functions execute a system call and take one cycle to
// complete.  
//

#ifndef __RIGELSIM_H__
#define __RIGELSIM_H__

#include <stdint.h>

//
// Memory Manipulation
//

void *RigelSimSetMemory(void *dst, int value, uint32_t len);
void * RigelSimCopyMemory(void *dst, void *src, uint32_t len);
int RigelSimCompareMemory(const void *ptr1, const void *, uint32_t len);

//
// Data Loading Functions
//

uint32_t RigelSimReadf(const char *filename, float *data, uint32_t len);
uint32_t RigelSimReadi(const char *filename, int *data, uint32_t len);
uint32_t RigelSimReadui(const char *filename, unsigned int *data, uint32_t len);
uint32_t RigelSimReadb(const char *filename, char *data, uint32_t len);
uint32_t RigelSimReadub(const char *filename, unsigned char *data, uint32_t len);

//
// Data Writing Functions
//

uint32_t RigelSimWritef(const char *filename, const float *data, uint32_t len);
uint32_t RigelSimWritei(const char *filename, const int *data, uint32_t len);
uint32_t RigelSimWriteui(const char *filename, const unsigned int *data, uint32_t len);
uint32_t RigelSimWriteb(const char *filename, const char *data, uint32_t len);
uint32_t RigelSimWriteub(const char *filename, const unsigned char *data, uint32_t len);

//
// Initialization Functions
//

void RigelSimSetf(float* data, float value, uint32_t len);
void RigelSimSeti(int *data, int value, uint32_t len);
void RigelSimSetui(unsigned int *data, unsigned int value, uint32_t len);
void RigelSimSetb(char *data, char value, uint32_t len);
void RigelSimSetub(unsigned char *data, unsigned char value, uint32_t len);

//
// Randomization Functions
//

void RigelSimRandomf(float *data, uint32_t len);
void RigelSimRandomi(int *data, uint32_t len);
void RigelSimRandomui(unsigned int *data, uint32_t len);
void RigelSimRandomb(char *data, uint32_t len);
void RigelSimRandomub(unsigned char *data, uint32_t len);

void RigelSimPrintString(const char *str);


#endif // __RIGELSIM_H__
