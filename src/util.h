
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif

typedef signed char  I8;
typedef signed short I16;
typedef signed long  I32;

typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned long  U32;

typedef struct { U16 x, y; } V2U16;
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
   size_t   maxIter, subIter;
} ProcInfo;

typedef struct
{
   DataFileInfo   dfi;
   ProcInfo       proc;
} ArgInfo;


// extern const char *sc (const char *s, const char c, const char * const e, const I8 o);
//extern int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[]);
extern int scanArgs (ArgInfo * pAI, const char *a[], int nA);

#endif // UTIL_H
