#ifndef __DESCRIPTOR_BUFFER_H__
#define __DESCRIPTOR_BUFFER_H__

#include "rigel.h"
#include "common.h"

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
  void (*wb)(rtm_range_t *);
  void (*inv)(rtm_range_t *);
} descriptor_buffer;

void init_descriptor_buffer(descriptor_buffer *buf, unsigned int inflags, void (* initwb)(rtm_range_t *), void (* initinv)(rtm_range_t *));
//These get filled in by the benchmark code
void writeback_descriptor(rtm_range_t *desc);
void invalidate_descriptor(rtm_range_t *desc);
///////////////////////////////////////////
void handle_descriptor(descriptor_buffer *buf, rtm_range_t *r);
void new_writeback_function(descriptor_buffer *buf, void (* newwb)(rtm_range_t *));
void new_invalidate_function(descriptor_buffer *buf, void (* newinv)(rtm_range_t *));
void empty_all_descriptors(descriptor_buffer *buf);
void add_descriptor(descriptor_buffer *buf, rtm_range_t *desc);

#endif //#ifndef __DESCRIPTOR_BUFFER_H__
