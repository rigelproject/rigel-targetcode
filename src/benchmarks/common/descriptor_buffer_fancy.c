#include "descriptor_buffer_fancy.h"

void init_descriptor_buffer(descriptor_buffer *buf, unsigned int inflags, void (* initwb)(rtm_range_t *), void (* initinv)(rtm_range_t *))
{
  buf->head = 0;
  buf->tail = 0;
  buf->flags = inflags;
  buf->wb = initwb;
  buf->inv = initinv;
}

void handle_descriptor(descriptor_buffer *buf, rtm_range_t *r)
{
  if(buf->flags & DESCRIPTOR_BUFFER_HANDLES_WRITEBACK)
    buf->wb(r);
  if(buf->flags & DESCRIPTOR_BUFFER_HANDLES_INVALIDATE)
    buf->inv(r);
}

void new_writeback_function(descriptor_buffer *buf, void (* newwb)(rtm_range_t *))
{
  buf->wb = newwb;
}

void new_invalidate_function(descriptor_buffer *buf, void (* newinv)(rtm_range_t *))
{
  buf->inv = newinv;
}

void empty_all_descriptors(descriptor_buffer *buf)
{
  if(buf->head == buf->tail)
    return;
  while(buf->tail != buf->head)
  {
    rtm_range_t *r = &(buf->list[buf->tail]);
    handle_descriptor(buf, r);
    buf->tail = (buf->tail + 1) & DESCRIPTOR_BUFFER_MASK;
  }
}

void add_descriptor(descriptor_buffer *buf, rtm_range_t *desc)
{
  if(((buf->head + 1) & DESCRIPTOR_BUFFER_MASK) == buf->tail) //Full
  {
#ifdef DESCRIPTOR_BUFFER_EMPTY_ALL
    empty_all_descriptors(buf);
#else
    rtm_range_t *r = &(buf->list[buf->tail]);
    handle_descriptor(buf, r);
    buf->tail = (buf->tail + 1) & DESCRIPTOR_BUFFER_MASK;
#endif
  }
  rtm_range_t *head = &(buf->list[buf->head]);
  head->v1.v1 = desc->v1.v1;
  head->v2.v2 = desc->v2.v2;
  head->v3.v3 = desc->v3.v3;
  head->v4.v4 = desc->v4.v4;
  buf->head = (buf->head + 1) & DESCRIPTOR_BUFFER_MASK;
}
