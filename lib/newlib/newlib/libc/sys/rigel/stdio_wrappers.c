// Same interface as read except we do it all in one go in the simulator.
size_t
ffilemap(void *buf, size_t size, size_t nobj, FILE* fp)
{
  size_t requested_bytes = size * nobj;
  size_t actual_bytes;

  if (NULL == buf) { assert(0 && "void * buf is invalid!"); }
  if (NULL == fp) { assert(0 && "FILE *fp is invalid!"); }

        LOCK_FILE(fp);
        actual_bytes = fp->filemap(fp, buf, requested_bytes);
        UNLOCK_FILE(fp);

        return actual_bytes;
}

size_t
ffileslurp(void *buf, size_t requested_bytes, FILE* fp)
{
  size_t actual_bytes;

  if (NULL == buf) { assert(0 && "void * buf is invalid!"); }
  if (NULL == fp) { assert(0 && "FILE *fp is invalid!"); }

        LOCK_FILE(fp);
        actual_bytes = fp->fileslurp(fp, buf, requested_bytes);
        UNLOCK_FILE(fp);

        return actual_bytes;
}

size_t
ffiledump(void *buf, size_t requested_bytes, FILE* fp)
{
  size_t actual_bytes;

  if (NULL == fp) { assert(0 && "FILE *fp is invalid!"); }
  if (NULL == buf) { assert(0 && "void * buf is invalid!"); }

        LOCK_FILE(fp);
        actual_bytes = fp->filedump(fp, buf, requested_bytes);
        UNLOCK_FILE(fp);

        return actual_bytes;
}

