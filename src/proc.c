// proc.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#include "proc.h"

#ifdef __PGI   // HACK
#ifndef OPEN_ACC
#define OPEN_ACC
#endif
#include <openacc.h>
//#include <assert.h>
//#include <accelmath.h>

#define INLINE inline
#endif

#ifndef INLINE
#define INLINE
#endif

#define ACC_DEV_MAX 4

typedef struct
{
   U8 c, n;
} AccDev;

typedef struct
{
   AccDev d[ACC_DEV_MAX];
   U8 nDev, iCurr;
   U8 pad[2];
} AccDevTable;

typedef struct
{
   U32 min, max;  // Y spans
   U32 o, n;      // Sub-buffer offset & count
} DomSubS;

typedef struct
{
   DomSubS  ds;
   AccDev   dev;
   U8 pad[2];
} DSMapNode;

// typedef struct { V2U32 min, max; } DomSub2D;

static AccDevTable gDev={0,};


/***/

#ifndef OPEN_ACC
// Dummies for non-PGI build compatibility
#define acc_device_host 0
#define acc_device_nvidia 0
#define acc_device_not_host 0
int acc_get_num_devices (int t) { return(0); }
int acc_get_device_num (int t) { return(-1); }
void acc_set_device_num (int n, int t) { ; }
void acc_wait_all (void) { ; }
#endif

/*
#define LAPLACE2D2S9P(pS,i,sv,k) ( (pS)[0] * k[0] + \
   ( (pS)[ i - sv.x ] + (pS)[ i + sv.x ] + (pS)[ i - sv.y ] + (pS)[ i + sv.y ] ) * k[1] + \
   ( (pS)[ i - sv.x - sv.y ] + (pS)[ i + sv.x - sv.y ] + (pS)[ i + sv.y - sv.x ] + (pS)[ i + sv.x + sv.y ] ) * k[2] )
*/
// Stride 0,1 -> +X +Y
//#pragma acc routine
INLINE Scalar laplace2D2S9P (const Scalar * const pS, const Stride s[2], const Scalar k[3])
{
   return( pS[0] * k[0] +
          (pS[-s[0]] + pS[s[0]] + pS[-s[1]] + pS[s[1]]) * k[1] +
          (pS[-s[1]-s[0]] + pS[-s[1]+s[0]] + pS[s[1]-s[0]] + pS[s[1]+s[0]]) * k[2] );
} // laplace2D2S9P

// Stride 0..3 -> +-X +-Y
//#pragma acc routine vector
INLINE Scalar laplace2D4S9P (const Scalar * const pS, const Stride s[4], const Scalar k[3])
{
   return( pS[0] * k[0] +
           (pS[ s[0] ] + pS[ s[1] ] + pS[ s[2] ] + pS[ s[3] ]) * k[1] + 
           (pS[ s[0]+s[2] ] + pS[ s[0]+s[3] ] + pS[ s[1]+s[2] ] + pS[ s[1]+s[3] ]) * k[2] ); 
} // laplace2D4S9P

//#pragma acc routine vector
INLINE void proc1 (Scalar * const pR, const Scalar * const pS, const Index i, const Stride j, const Stride wrap[4], const BaseParamVal * const pP)
{
   const Scalar * const pA= pS+i, a= *pA;
   const Scalar * const pB= pS+i+j, b= *pB;
   const Scalar rab2= pP->kRR * a * b * b;

   pR[i]= a + laplace2D4S9P(pA, wrap, pP->kL.a) - rab2 + pP->kRA * (1 - a);
   pR[i+j]= b + laplace2D4S9P(pB, wrap, pP->kL.b) + rab2 - pP->kDB * b;
} // proc1

//#pragma acc routine vector
INLINE void proc1XY (Scalar * const pR, const Scalar * const pS, const Index x, const Index y, const ImgOrg *pO, const BaseParamVal * const pP)
{
   Stride wrap[4];

   wrap[0]= pO->nhStepWrap[ (x <= 0) ][0];
   wrap[1]= pO->nhStepWrap[ (x >= (Index)(pO->def.x-1)) ][1];
   wrap[2]= pO->nhStepWrap[ (y <= 0) ][2];
   wrap[3]= pO->nhStepWrap[ (y >= (Index)(pO->def.y-1)) ][3];

   proc1(pR, pS, x * pO->stride[0] + y * pO->stride[1], pO->stride[3], wrap, pP);
} // proc1XY

// Simple brute force implementation (every site boundary checked)
void procAXY (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const BaseParamVal * pP)
{
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1] )
   {
      #pragma acc parallel loop
      for (U32 y= 0; y < pO->def.y; ++y )
      {
         #pragma acc loop vector
         for (U32 x= 0; x < pO->def.x; ++x )
         {
            proc1XY(pR, pS, x, y, pO, pP);
         }
      }
   }
} // procAXY

void procAXYDS
(
   Scalar * restrict pR, 
   const Scalar * restrict pS, 
   const ImgOrg * pO, 
   const BaseParamVal * pP,
   const DomSubS * pDS
)
{
   #pragma acc data present( pR[pDS->o:pDS->n], pS[pDS->o:pDS->n], pO[:1], pP[:1], pDS[:1] )
   {
      #pragma acc parallel loop
      for (U32 y= pDS->min; y < pDS->max; ++y )
      {
         #pragma acc loop vector
         for (U32 x= 0; x < pO->def.x; ++x )
         {
            proc1XY(pR, pS, x, y, pO, pP);
         }
      }
   }
} // procAXYDS

/* Parameter variation
void procVA (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP)
{
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1], pP->pKRR[:pP->n], pP->pKRA[:pP->n], pP->pKDB[:pP->n] )
   {
      #pragma acc parallel loop
      for (U32 y= 1; y < (pO->def.y-1); ++y )
      {
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            const Index i= y * pO->stride[1] + x * pO->stride[0];
            const Index j= i + pO->stride[3];
            const Scalar a= pS[i];
            const Scalar b= pS[j];
            const Scalar rab2= pP->pKRR[x] * a * b * b;
            //const Scalar ar= KRA0 * (1 - a);
            //const Scalar bd= KDB0 * b;
            pR[i]= a + laplace2D2S9P(pS+i, pO->stride, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a);
            pR[j]= b + laplace2D2S9P(pS+j, pO->stride, pP->kL.b) + rab2 - pP->pKDB[y] * b;
         }
      }

      // Boundaries
#if 1 // Non-corner Edges

      // Horizontal top & bottom
      #pragma acc parallel loop
      for (U32 x= 1; x < (pO->def.x-1); ++x )
      {
         const Index i1= x * pO->stride[0];
         const Index j1= i1 + pO->stride[3];
         const Scalar a1= pS[i1];
         const Scalar b1= pS[j1];
         Scalar rab2= pP->pKRR[x] * a1 * b1 * b1;
         //const Scalar ar1= KRA0 * (1 - a1);
         //const Scalar bd1= KDB0 * b1;
         pR[i1]= a1 + laplace2D4S9P(pS+i1, pO->wrap.h+0, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a1);
         pR[j1]= b1 + laplace2D4S9P(pS+j1, pO->wrap.h+0, pP->kL.b) + rab2 - pP->pKDB[0] * b1;

         const Stride offsY= pO->stride[2] - pO->stride[1];
         const Index i2= i1 + offsY;
         const Index j2= j1 + offsY;
         const Scalar a2= pS[i2];
         const Scalar b2= pS[j2];
         rab2= pP->pKRR[x] * a2 * b2 * b2;
         pR[i2]= a2 + laplace2D4S9P(pS+i2, pO->wrap.h+2, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a2);
         pR[j2]= b2 + laplace2D4S9P(pS+j2, pO->wrap.h+2, pP->kL.b) + rab2 - pP->pKDB[pO->def.y-1] * b2;
      }
#endif
#if 1

      // left & right
      #pragma acc parallel loop
      for (U32 y= 1; y < (pO->def.y-1); ++y )
      {
         Scalar a, b, rab2;
         const Index i1= y * pO->stride[1];
         const Index j1= i1 + pO->stride[3];
         a= pS[i1];
         b= pS[j1];
         rab2= pP->pKRR[0] * a * b * b;
         pR[i1]= a + laplace2D4S9P(pS+i1, pO->wrap.v+0, pP->kL.a) - rab2 + pP->pKRA[0] * (1 - a);
         pR[j1]= b + laplace2D4S9P(pS+j1, pO->wrap.v+0, pP->kL.b) + rab2 - pP->pKDB[y] * b;

         const Index offsX= pO->stride[1] - pO->stride[0];
         const Index i2= i1 + offsX;
         const Index j2= j1 + offsX;
         a= pS[i2];
         b= pS[j2];
         rab2= pP->pKRR[pO->def.x-1] * a * b * b;
         pR[i2]= a + laplace2D4S9P(pS+i2, pO->wrap.v+2, pP->kL.a) - rab2 + pP->pKRA[pO->def.x-1] * (1 - a);
         pR[j2]= b + laplace2D4S9P(pS+j2, pO->wrap.v+2, pP->kL.b) + rab2 - pP->pKDB[y] * b;
      }
#endif
#if 1	// The four corners: R,L * B,T
      //pragma acc parallel
      {
         proc1(pR, pS, 0, pO->stride[3], pO->wrap.c+0, pP);
         proc1(pR, pS, pO->stride[1]-pO->stride[0], pO->stride[3], pO->wrap.c+2, pP);
         proc1(pR, pS, pO->stride[2]-pO->stride[1], pO->stride[3], pO->wrap.d+0, pP);
         proc1(pR, pS, pO->stride[2]-pO->stride[0], pO->stride[3], pO->wrap.d+2, pP);
      }
#endif
   } // ... acc data ..
} // procVA
*/
// Simple parameters, avoids most unnecessary boundary checking
void procA (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const BaseParamVal * pP)
{
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1] )
   {
      #pragma acc parallel loop
      for (U32 y= 1; y < (pO->def.y-1); ++y )
      {
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            proc1(pR, pS, y * pO->stride[1] + x * pO->stride[0], pO->stride[3], pO->nhStepWrap[0], pP);
         }
      }

      // Boundaries
      #pragma acc parallel
      {  // Horizontal top & bottom avoiding corners
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            proc1(pR, pS, x * pO->stride[0], pO->stride[3], pO->wrap.h+0, pP);
         }
         #pragma acc loop vector
         for (U32 x= 1; x < (pO->def.x-1); ++x )
         {
            const Stride offsY= pO->stride[2] - pO->stride[1];
            proc1(pR, pS, x * pO->stride[0] + offsY, pO->stride[3], pO->wrap.h+2, pP);
         }
         // left & right including corners (requires boundary check)
         #pragma acc loop vector
         for (U32 y= 0; y < pO->def.y; ++y )
         {
            proc1XY(pR, pS, 0, y, pO, pP);
         }
         #pragma acc loop vector
         for (U32 y= 0; y < pO->def.y; ++y )
         {
            proc1XY(pR, pS, pO->def.x-1, y, pO, pP);
         }
      } // ... acc parallel
   } // ... acc data ..
} // procA

U32 proc2IA
(
   Scalar * restrict pTR, // temp "result" (unused if acc. device has sufficient private memory)
   Scalar * restrict pSR, // source & result buffer
   const ImgOrg   * pO, 
   const ParamVal * pP, 
   const U32 nI
)
{
   #pragma acc data present_or_create( pTR[:pO->n] ) copy( pSR[:pO->n] )
   //                 present_or_copyin( pO[:1], pP[:1] )
   {
      for (U32 i= 0; i < nI; ++i )
      {
         procA(pTR,pSR,pO,&(pP->base));
         procA(pSR,pTR,pO,&(pP->base));
      }
   }
   return(2*nI);
} // proc2I1A

U32 proc2I1A (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 nI)
{
   #pragma acc data present_or_create( pR[:pO->n] ) copyin( pS[:pO->n] ) copyout( pR[:pO->n] )
   //                 present_or_copyin( pO[:1], pP[:1] )
   {
      procA(pR,pS,pO,&(pP->base));
      for (U32 i= 0; i < nI; ++i )
      {
         procA(pS,pR,pO,&(pP->base));
         procA(pR,pS,pO,&(pP->base));
      }
   }
   return(2*nI+1);
} // proc2I1A

U32 proc2IADS
(
   Scalar * restrict pTR, 
   Scalar * restrict pSR,
   const ImgOrg * pO,
   const ParamVal * pP,
   const DSMapNode aDSMN[],
   const U32 nDSMN,
   const U32 nI
)
{
   
   if (nI > 0)
   {
      for (U32 j= 0; j < nDSMN; ++j )
      {
         const DomSubS * pDS= &(aDSMN[j].ds);
         acc_set_device_num( aDSMN[j].dev.n, aDSMN[j].dev.c );
         #pragma acc data present_or_create( pTR[pDS->o:pDS->n] ) copyin( pSR[pDS->o:pDS->n] )
         { // present_or_copyin( pO[:1], pP[:1], pDS[:1] )
            procAXYDS(pTR,pSR,pO,&(pP->base), pDS );
         }
      } // j
      acc_wait_all();
      for ( U32 i= 1; i < nI; ++i )
      {
      }

      for (U32 j= 0; j < nDSMN; ++j )
      {
         const DomSubS * pDS= &(aDSMN[j].ds);
         acc_set_device_num( aDSMN[j].dev.n, aDSMN[j].dev.c );
         #pragma acc data present( pTR[pDS->o:pDS->n] ) copyout( pSR[pDS->o:pDS->n] )
         { // present( pO[:1], pP[:1], pDS[:1] ) 
            procAXYDS(pSR,pTR,pO,&(pP->base), pDS);
         }
      }
   }
   return(2*nI);
} // proc2IADS


/** EXT **/

/* /opt/pgi/linux.../2017/iclude/openacc.h

typedef enum{
        acc_device_none = 0,
        acc_device_default = 1,
        acc_device_host = 2,
        acc_device_not_host = 3,
        acc_device_nvidia = 4,
        acc_device_radeon = 5,
        acc_device_xeonphi = 6,
        acc_device_pgi_opencl = 7,
        acc_device_nvidia_opencl = 8,
        acc_device_opencl = 9
    }acc_device_t;

void acc_set_default_async(int async);
int acc_get_default_async(void);
extern int acc_get_num_devices( acc_device_t devtype );
extern acc_device_t acc_get_device(void);
extern void acc_set_device_num( int devnum, acc_device_t devtype );
extern int acc_get_device_num( acc_device_t devtype );
extern void acc_init( acc_device_t devtype );
extern void acc_shutdown( acc_device_t devtype );
extern void acc_set_deviceid( int devid );
extern int acc_get_deviceid( int devnum, acc_device_t devtype );
extern int acc_async_test( __PGI_LLONG async );
extern int acc_async_test_all(void);
extern void acc_async_wait( __PGI_LLONG async );
extern void acc_async_wait_all(void);
extern void acc_wait( __PGI_LLONG async );
extern void acc_wait_async( __PGI_LLONG arg, __PGI_LLONG async );
extern void acc_wait_all(void);
extern void acc_wait_all_async( __PGI_LLONG async );
extern int acc_on_device( acc_device_t devtype );
extern void acc_free(void*); */

Bool32 procInitAcc (size_t f) // arg param ?
{
   int nInit= (0 == (f & (PROC_FLAG_ACCGPU|PROC_FLAG_ACCHOST)));
   int nNV= acc_get_num_devices( acc_device_nvidia );
   int nH= acc_get_num_devices( acc_device_host );
   int nNH= acc_get_num_devices( acc_device_not_host );
   int id;

   printf("procInitAcc() - nNV=%d, nH=%d (other=%d)\n", nNV, nH, nNH - nNV);
   gDev.nDev= 0;
   if (nNV > 0)
   {
      id= acc_get_device_num(acc_device_nvidia);
      printf("\tNV:id=%d\n", id);
      if (f & PROC_FLAG_ACCGPU)
      {
         id= nNV;
         while (--id >= 0)
         {
            if (gDev.nDev < ACC_DEV_MAX)
            {
               gDev.d[gDev.nDev].c= acc_device_nvidia;
               gDev.d[gDev.nDev].n= id;
               ++gDev.nDev;
            }
            //acc_set_device_num( id, acc_device_nvidia );
            //acc_init( acc_device_nvidia ); // get_err?
            ++nInit;
         }
      }
   }
   if (nH > 0)
   {
      id= acc_get_device_num(acc_device_host);
      printf("\tH:id=%d\n", id);
      if (f & PROC_FLAG_ACCHOST)
      {
         id= nNH;
         while (--id >= 0)
         {
            if (gDev.nDev < ACC_DEV_MAX)
            {
               gDev.d[gDev.nDev].c= acc_device_host;
               gDev.d[gDev.nDev].n= id;
               ++gDev.nDev;
            }
            //acc_set_device_num( id, acc_device_host );
            //acc_init( acc_device_host );
            ++nInit;
         }
      }
   }
   if (gDev.nDev > 0)
   {
      gDev.iCurr= 0;
      acc_set_device_num( gDev.d[0].n, gDev.d[0].c );
   }
   return(nInit > 0);
} // procInitAcc

const char *procGetCurrAccTxt (char t[], int m)
{
   const AccDev * const pA= gDev.d + gDev.iCurr;
   const char *s="C";
#ifdef OPEN_ACC
   switch (pA->c)
   {
      case acc_device_nvidia : s= "NV"; break;
      case acc_device_host :   s= "H"; break;
      default : s= "?"; break;
   }
#endif // OPEN_ACC
   snprintf(t, m, "%s%u", s, pA->n);
   return(t);
} // procGetCurrAccTxt

Bool32 procSetNextAcc (Bool32 wrap)
{
#ifdef OPEN_ACC
   if (gDev.nDev > 0)
   {
      U8 iN= gDev.iCurr + 1;
      if (wrap) { iN= iN % gDev.nDev; }
      if (iN < gDev.nDev)
      {
         acc_set_device_num( gDev.d[iN].n, gDev.d[iN].c );
         gDev.iCurr= iN;
         return(TRUE);
      }
   }
#endif // OPEN_ACC
   return(FALSE);
} // procSetNextAcc


U32 procNI
(
   Scalar * restrict pR, 
   Scalar * restrict pS, 
   const ImgOrg * pO, 
   const ParamVal * pP, 
   const U32 nI
)
{
   #pragma acc data copyin( pO[:1], pP[:1] )
   {
      if (nI & 1) return proc2I1A(pR, pS, pO, pP, nI>>1);
      else return proc2IA(pR, pS, pO, pP, nI>>1);
   }
} // procNI
