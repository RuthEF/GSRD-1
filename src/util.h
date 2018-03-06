
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// General compiler tweaks
#pragma clang diagnostic ignored "-Wmissing-field-initializers"


#define PROC_FLAG_ACCHOST  (1<<0)
#define PROC_FLAG_ACCGPU   (1<<1)

#ifndef SWAP
#define SWAP(Type,a,b) { Type tmp= (a); (a)= (b); (b)= tmp; }
#endif

#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

typedef signed char  I8;
typedef signed short I16;
typedef signed long  I32;

typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned long  U32;

typedef int Bool32;

typedef struct
{
   size_t bytes;
   void  *p;
} MemBuff;

typedef struct { U16 x, y; } V2U16;
typedef struct { U32 x, y; } V2U32;

typedef struct { U16 start, len; } ScanSeg;

typedef struct
{
   const char  *path, *name;
   size_t      bytes;
   ScanSeg     vSS;
   int         v[4], nV;
   U8          elemBits;
} DataFileInfo;

typedef struct
{
   size_t   flags, maxIter, subIter;
} ProcInfo;

typedef struct
{
   DataFileInfo   dfi;
   ProcInfo       proc;
} ArgInfo;

/***/

extern size_t fileSize (const char * const path);
extern size_t loadBuff (void * const pB, const char * const path, const size_t bytes);
extern size_t saveBuff (const void * const pB, const char * const path, const size_t bytes);


// extern const char *sc (const char *s, const char c, const char * const e, const I8 o);
//extern int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[]);
extern int scanArgs (ArgInfo * pAI, const char * const a[], int nA);

#endif // UTIL_H
