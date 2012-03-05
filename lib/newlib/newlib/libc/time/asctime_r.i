# 1 "asctime_r.c"
# 1 "asctime_r.c" 1
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 111 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "asctime_r.c" 2




# 1 "/media/d2/rigel/include/stdio.h" 1



# 1 "/media/d2/rigel/include/sys/cdefs.h" 1
# 5 "/media/d2/rigel/include/stdio.h" 2
# 1 "/media/d2/rigel/include/machine/inttypes.h" 1
# 51 "/media/d2/rigel/include/machine/inttypes.h"
typedef signed long long __int64_t;
typedef signed int __int32_t;
typedef signed short __int16_t;
typedef signed char __int8_t;

typedef unsigned long long __uint64_t;
typedef unsigned int __uint32_t;
typedef unsigned short __uint16_t;
typedef unsigned char __uint8_t;



typedef __int32_t __intptr_t;
typedef __uint32_t __uintptr_t;
# 6 "/media/d2/rigel/include/stdio.h" 2
# 1 "/media/d2/rigel/include/machine/ansi.h" 1
# 34 "/media/d2/rigel/include/machine/ansi.h"
typedef __int32_t __ptrdiff_t;
typedef __uint32_t __size_t;
typedef __int32_t __ssize_t;
typedef long int __off_t;
typedef __uint32_t __mode_t;
# 7 "/media/d2/rigel/include/stdio.h" 2
# 1 "/media/d2/rigel/include/sys/va_list.h" 1
# 44 "/media/d2/rigel/include/sys/va_list.h"
typedef void *__va_list;
# 8 "/media/d2/rigel/include/stdio.h" 2


# 1 "/media/d2/rigel/include/shared/null.h" 1
# 11 "/media/d2/rigel/include/stdio.h" 2
# 1 "/media/d2/rigel/include/shared/size_t.h" 1
# 34 "/media/d2/rigel/include/shared/size_t.h"
typedef __size_t size_t;
# 12 "/media/d2/rigel/include/stdio.h" 2
# 1 "/media/d2/rigel/include/spinlock.h" 1






typedef volatile struct __spin_lock_t {
 volatile unsigned int lock_val;
 unsigned char padding[32 - sizeof(unsigned int)];
} spin_lock_t __attribute__ ((aligned (8)));

typedef volatile struct __atomic_flag_t {
 volatile unsigned int flag_val;
 unsigned char padding[32 - sizeof(unsigned int)];
} atomic_flag_t __attribute__ ((aligned (8)));
# 37 "/media/d2/rigel/include/spinlock.h"
extern spin_lock_t BIG_RIGEL_LOCK;

void spin_lock_init(volatile spin_lock_t *s);
# 119 "/media/d2/rigel/include/spinlock.h"
int spin_lock_function(spin_lock_t *s);
int spin_lock_paused_function(spin_lock_t *s);
int spin_unlock_function(spin_lock_t *s);

int atomic_flag_check(volatile atomic_flag_t *f);
void atomic_flag_set(volatile atomic_flag_t *f);
void atomic_flag_clear(volatile atomic_flag_t *f);

void atomic_flag_spin_until_clear(volatile atomic_flag_t *f);
void atomic_flag_spin_until_set(volatile atomic_flag_t *f);
# 13 "/media/d2/rigel/include/stdio.h" 2




typedef int (*__FILE_close_func_t)(void *fp);
typedef int (*__FILE_feof_func_t)(void *fp);
typedef int (*__FILE_read_func_t)(void *fp, void *buf, int size);
typedef int (*__FILE_ungetc_func_t)(void *fp, int resetchar);
typedef int (*__FILE_write_func_t)(void *fp, const void *buf, int size);
typedef int (*__FILE_flush_func_t)(void *fp);
typedef __off_t (*__FILE_seek_func_t)(void *fp, __off_t off, int whence);
typedef int (*__FILE_fileno_func_t)(void *fp);
typedef int (*__FILE_clearerr_func_t)(void *fp);
typedef int (*__FILE_ferror_func_t)(void *fp);

typedef struct __FILE_struct {
  __FILE_close_func_t close;
  __FILE_feof_func_t feof;
  __FILE_read_func_t read;


  __FILE_read_func_t filemap;
  __FILE_read_func_t fileslurp;
  __FILE_write_func_t filedump;
  __FILE_ungetc_func_t ungetc;
  __FILE_write_func_t write;
  __FILE_flush_func_t flush;
  __FILE_seek_func_t seek;
  __FILE_fileno_func_t fileno;
  __FILE_clearerr_func_t clearerr;
  __FILE_ferror_func_t ferror;



 spin_lock_t file_lock;
} FILE;

typedef struct __UNBUF_STREAM_struct __UNBUF_STREAM_t;
__UNBUF_STREAM_t* __unbuf_istream_open(const char *pathname);
__UNBUF_STREAM_t* __unbuf_istream_fdopen(int fd);
__UNBUF_STREAM_t* __unbuf_ostream_open(const char *pathname);
__UNBUF_STREAM_t* __unbuf_ostream_open_append(const char *pathname);
__UNBUF_STREAM_t* __unbuf_ostream_fdopen(int fd);
__UNBUF_STREAM_t* __unbuf_ostream_fdopen_append(int fd);

typedef struct __BUF_STREAM_struct __BUF_STREAM_t;
__BUF_STREAM_t* __buf_istream_open(const char *pathname);
__BUF_STREAM_t* __buf_istream_fdopen(int fd);
__BUF_STREAM_t* __buf_ostream_open(const char *pathname);
__BUF_STREAM_t* __buf_ostream_open_append(const char *pathname);
__BUF_STREAM_t* __buf_ostream_fdopen(int fd);
__BUF_STREAM_t* __buf_ostream_fdopen_append(int fd);
int __buf_stream_atexit();

typedef struct __STRING_STREAM_struct __STRING_STREAM_t;
__STRING_STREAM_t* __string_istream_open(char *str);
__STRING_STREAM_t* __string_ostream_open(char *str);

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;





extern int _sys_nerr;
extern const char *const _sys_errlist[];
# 106 "/media/d2/rigel/include/stdio.h"
int fclose(FILE *);
int feof(FILE *);
int ferror(FILE *);
int fflush(FILE *);
int fgetc(FILE *);
char *fgets(char *, int, FILE *);
FILE *fopen(const char *, const char *);
FILE *fdopen(const int, const char *);
int fprintf(FILE *, const char *, ...);
int fputc(int, FILE *);
int fputs(const char *, FILE *);
size_t fread(void *, size_t, size_t, FILE *);
int fscanf(FILE *, const char *, ...);
int fseek(FILE *, long, int);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fileno(FILE *);
void clearerr(FILE *);
int getc(FILE *);
int getchar(void);
long ftell(FILE *);


void perror(const char *);
int printf(const char *, ...);
int putc(int, FILE *);
int putchar(int);
int puts(const char *);
int remove(const char *);
int rename (const char *, const char *);
void rewind(FILE *);
int scanf(const char *, ...);
void setbuf(FILE *, char *);
int setvbuf(FILE *, char *, int, size_t);
int sprintf(char *, const char *, ...);
int sscanf(char *, const char *, ...);
FILE *tmpfile(void);
char *tmpnam(char *);
int ungetc(int, FILE *);
int vfprintf(FILE *, const char *, __va_list);
int vfscanf(FILE *, char const *, __va_list);
int vprintf(const char *, __va_list);
int vscanf(const char*, __va_list);
int vsprintf(char *, const char *, __va_list);
int vsscanf(char *, const char *, __va_list);

size_t ffilemap(void *buf, size_t size, size_t nobj, FILE* fp);
size_t ffileslurp(void *buf, size_t requested_bytes, FILE* fp);
size_t ffiledump(void *buf, size_t requested_bytes, FILE* fp);
# 6 "asctime_r.c" 2
# 1 "/media/d2/rigel/include/time.h" 1





typedef __uint32_t time_t;
typedef __uint32_t clock_t;







struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};


clock_t clock(void);
char *ctime(const time_t *);
time_t time(time_t *);
struct tm*localtime(const time_t *timep);


struct timespec {
 time_t tv_sec;
 long tv_nsec;
};
# 7 "asctime_r.c" 2

char *
_DEFUN (asctime_r, (tim_p, result),
 _CONST struct tm *tim_p _AND
 char *result)
{
  static _CONST char day_name[7][3] = {
 "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
  static _CONST char mon_name[12][3] = {
 "Jan", "Feb", "Mar", "Apr", "May", "Jun",
 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  sprintf (result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
    day_name[tim_p->tm_wday],
    mon_name[tim_p->tm_mon],
    tim_p->tm_mday, tim_p->tm_hour, tim_p->tm_min,
    tim_p->tm_sec, 1900 + tim_p->tm_year);
  return result;
}
