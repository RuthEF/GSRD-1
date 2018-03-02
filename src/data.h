
#ifndef DATA_H
#define DATA_H

#include "util.h"

#define KR0  (Scalar)0.125
#define KRA0 (Scalar)0.0115
#define KDB0 (Scalar)0.0195
#define KLA0 (Scalar)0.25
#define KLB0 (Scalar)0.025


//typedef float Scalar;
typedef double       Scalar;
typedef signed long  Stride;
typedef signed long  Index;

typedef struct
{
   Scalar a[3], b[3];
} KLAB;

typedef struct
{
   KLAB   kL;
   Scalar kRR, kRA, kDB;
   const Scalar * restrict pKRR, * restrict pKRA, * restrict pKDB;
   size_t n;
} ParamVal;

typedef struct
{
   V2U32  def;
   Stride stride[4];
   size_t n;
} ImgOrg;

typedef struct
{
   Stride x,y;
} SV2;

typedef struct
{
   Scalar *pAB[2], *pC;
} HostBuff;

/***/

extern void initOrg (ImgOrg * const pO, U16 w, U16 h, U8 flags);

extern size_t paramBytes (U16 w, U16 h);

extern size_t initParam (ParamVal * const pP, void *p, const Scalar kl[3], const V2U32 * const pD, Scalar varR, Scalar varD); // ParamArgs *

extern size_t initBuff (const HostBuff *pB, const V2U32 d, const U16 step);

#endif // DATA_H
