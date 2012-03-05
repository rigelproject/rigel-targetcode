#include "descriptor_buffer.h"

void init_descriptor_buffer(descriptor_buffer *buf, unsigned int inflags)
{
  buf->head = 0;
  buf->tail = 0;
  buf->flags = inflags;
}

void empty_all_descriptors(descriptor_buffer *buf)
{
  if(buf->head == buf->tail)
    return;

  while(buf->tail != buf->head)
  {
//    RigelPrint(0xAAAAAAAA);
    rtm_range_t *r = &(buf->list[buf->tail]);
    //handle_descriptor(buf, r);
    //Inlining by hand :-/
  switch(buf->flags)
  {
    case 0:
      break;
    case (DESCRIPTOR_BUFFER_HANDLES_WRITEBACK):
      _WRITEBACK_DESCRIPTOR(r);
      break;
    case (DESCRIPTOR_BUFFER_HANDLES_INVALIDATE):
      _INVALIDATE_DESCRIPTOR(r);
      break;
    case (DESCRIPTOR_BUFFER_HANDLES_WRITEBACK | DESCRIPTOR_BUFFER_HANDLES_INVALIDATE):
//RigelPrint(0xBBBBBBBB);
      _WRITEBACK_DESCRIPTOR(r);
//RigelPrint(0xCCCCCCCC);
      _INVALIDATE_DESCRIPTOR(r);
//RigelPrint(0xDDDDDDDD);
      break;
  }
    buf->tail = (buf->tail + 1) & DESCRIPTOR_BUFFER_MASK;
  //  RigelPrint((buf->head << 16) | (buf->tail));
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
    //handle_descriptor(buf, r);
    //Inlining by hand :-/
    switch(buf->flags)
    {
      case 0:
        break;
      case (DESCRIPTOR_BUFFER_HANDLES_WRITEBACK):
        _WRITEBACK_DESCRIPTOR(r);
        break;
      case (DESCRIPTOR_BUFFER_HANDLES_INVALIDATE):
        _INVALIDATE_DESCRIPTOR(r);
        break;
      case (DESCRIPTOR_BUFFER_HANDLES_WRITEBACK | DESCRIPTOR_BUFFER_HANDLES_INVALIDATE):
        _WRITEBACK_DESCRIPTOR(r);
        _INVALIDATE_DESCRIPTOR(r);
        break;
    }
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

