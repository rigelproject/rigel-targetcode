#ifndef __DESCRIPTOR_BUFFER_H__
#define __DESCRIPTOR_BUFFER_H__

#define _WRITEBACK_DESCRIPTOR(desc) do { WRITEBACK_DESCRIPTOR((desc)); } while(0)

#define _INVALIDATE_DESCRIPTOR(desc) do { INVALIDATE_DESCRIPTOR((desc)); } while(0)
#include "common.h"
#include "rigel-tasks.h"
#include "rigel.h"

//Must be a power of 2, less than 2^31
#define DESCRIPTOR_BUFFER_SIZE 1024
#define DESCRIPTOR_BUFFER_MASK (DESCRIPTOR_BUFFER_SIZE-1)

#define DESCRIPTOR_BUFFER_HANDLES_WRITEBACK 0x1
#define DESCRIPTOR_BUFFER_HANDLES_INVALIDATE 0x2

typedef struct _descriptor_buffer
{
  rtm_range_t list[DESCRIPTOR_BUFFER_SIZE];
  unsigned int head;
  unsigned int tail;
  unsigned int flags;
} descriptor_buffer;

void init_descriptor_buffer(descriptor_buffer *buf, unsigned int inflags);
void empty_all_descriptors(descriptor_buffer *buf);
void add_descriptor(descriptor_buffer *buf, rtm_range_t *desc);

#endif //#ifndef __DESCRIPTOR_BUFFER_H__
