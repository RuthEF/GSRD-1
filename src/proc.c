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


/***/
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
INLINE void proc1 (Scalar * const pR, const Scalar * const pI, const Index i, const Stride j, const Stride wrap[4], const BaseParamVal * const pP)
{
   const Scalar * const pA= pI+i, a= *pA;
   const Scalar * const pB= pI+i+j, b= *pB;
   const Scalar rab2= pP->kRR * a * b * b;

   pR[i]= a + laplace2D4S9P(pA, wrap, pP->kL.a) - rab2 + pP->kRA * (1 - a);
   pR[i+j]= b + laplace2D4S9P(pB, wrap, pP->kL.b) + rab2 - pP->kDB * b;
} // proc1

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
#ifdef OPEN_ACC
   int nNV= acc_get_num_devices( acc_device_nvidia );
   int nH= acc_get_num_devices( acc_device_host );
   int nNH= acc_get_num_devices( acc_device_not_host );
   
   printf("procInitAcc() - nNV=%d, nH=%d (other=%d)\n", nNV, nH, nNH - nNV);
   if ((nNV > 0) && (f & PROC_FLAG_ACCGPU))
   {
      acc_init( acc_device_nvidia ); // get_err?
      ++nInit;
   }
   else if ((nH > 0) && (f & PROC_FLAG_ACCHOST))
   {
      acc_init( acc_device_host );
      ++nInit;
   }
#endif
   return(nInit > 0);
} // procInitAcc
/* DEPRECATE
void procBindData (const HostBuff * const pHB, const ParamVal * const pP, const ImgOrg * const pO, const U32 iS)
{
   return;
   U32 iR = iS ^ 1;
   Scalar * restrict pS= pHB->pAB[iS];
   Scalar * restrict pR= pHB->pAB[iR];

   printf("procBindData() - pP=%p, pO=%p\n", pP, pO);
   #pragma acc data copyin( pP[0:1], pO[0:1] )
   #pragma acc data copyin( pP->pKRR[0:pP->n], pP->pKRA[0:pP->n], pP->pKDB[0:pP->n] )
   #pragma acc data copyin( pS[0:pO->n] )
   #pragma acc data create( pR[0:pO->n] )
   ;
} // procBindData
*/
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
// Simple parameters
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
            const Index i= y * pO->stride[1] + x * pO->stride[0];
            const Index j= i + pO->stride[3];
            const Scalar a= pS[i];
            const Scalar b= pS[j];
            const Scalar rab2= pP->kRR * a * b * b;
            //const Scalar ar= KRA0 * (1 - a);
            //const Scalar bd= KDB0 * b;
            pR[i]= a + laplace2D2S9P(pS+i, pO->stride, pP->kL.a) - rab2 + pP->kRA * (1 - a);
            pR[j]= b + laplace2D2S9P(pS+j, pO->stride, pP->kL.b) + rab2 - pP->kDB * b;
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
         Scalar rab2= pP->kRR * a1 * b1 * b1;
         //const Scalar ar1= KRA0 * (1 - a1);
         //const Scalar bd1= KDB0 * b1;
         pR[i1]= a1 + laplace2D4S9P(pS+i1, pO->wrap.h+0, pP->kL.a) - rab2 + pP->kRA * (1 - a1);
         pR[j1]= b1 + laplace2D4S9P(pS+j1, pO->wrap.h+0, pP->kL.b) + rab2 - pP->kDB * b1;

         const Stride offsY= pO->stride[2] - pO->stride[1];
         const Index i2= i1 + offsY;
         const Index j2= j1 + offsY;
         const Scalar a2= pS[i2];
         const Scalar b2= pS[j2];
         rab2= pP->kRR * a2 * b2 * b2;
         pR[i2]= a2 + laplace2D4S9P(pS+i2, pO->wrap.h+2, pP->kL.a) - rab2 + pP->kRA * (1 - a2);
         pR[j2]= b2 + laplace2D4S9P(pS+j2, pO->wrap.h+2, pP->kL.b) + rab2 - pP->kDB * b2;
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
         rab2= pP->kRR * a * b * b;
         pR[i1]= a + laplace2D4S9P(pS+i1, pO->wrap.v+0, pP->kL.a) - rab2 + pP->kRA * (1 - a);
         pR[j1]= b + laplace2D4S9P(pS+j1, pO->wrap.v+0, pP->kL.b) + rab2 - pP->kDB * b;

         const Index offsX= pO->stride[1] - pO->stride[0];
         const Index i2= i1 + offsX;
         const Index j2= j1 + offsX;
         a= pS[i2];
         b= pS[j2];
         rab2= pP->kRR * a * b * b;
         pR[i2]= a + laplace2D4S9P(pS+i2, pO->wrap.v+2, pP->kL.a) - rab2 + pP->kRA * (1 - a);
         pR[j2]= b + laplace2D4S9P(pS+j2, pO->wrap.v+2, pP->kL.b) + rab2 - pP->kDB * b;
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
} // procA

U32 proc2IA (Scalar * restrict pTR, Scalar * restrict pSR, const ImgOrg * pO, const ParamVal * pP, const U32 nI)
{
   #pragma acc data region present_or_create( pTR[:pO->n] ) present_or_copyin( pO[:1], pP[:1], \
                           copy( pSR[:pO->n] )
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
   //printf("proc2I1A()-\n");
   #pragma acc data region present_or_create( pR[:pO->n] ) present_or_copyin( pO[:1], pP[:1], \
                           copyin( pS[:pO->n] ) copyout( pR[:pO->n] )
   {
      procA(pR,pS,pO,&(pP->base));
      for (U32 i= 0; i < nI; ++i )
      {
         procA(pS,pR,pO,&(pP->base));
         procA(pR,pS,pO,&(pP->base));
      }
   }
   printf("-proc2I1A()\n");
   return(2*nI+1);
} // proc2I1A

void procSummarise (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO)
{
   const size_t n= pO->def.x * pO->def.y;
   BlockStat s;
   
   initFS(&(s.a), pAB);
   initFS(&(s.b), pAB + pO->stride[3]);
   #pragma acc data copy(s) present( pAB[0:pO->n] ) present(pO)
   {
      #pragma acc parallel loop
      for (size_t i=1; i<n; i++)
      {
         const Index j= i * pO->stride[0];
         const Index k= j + pO->stride[3];
         const Scalar a= pAB[j];
         if (a < s.a.min) { s.a.min= a; }
         if (a > s.a.max) { s.a.max= a; }
         s.a.s.m[1]+= a;      //sum1+= a;
         s.a.s.m[2]+= a * a;  // sum2+= a * a;
         const Scalar b= pAB[k];
         if (b < s.b.min) { s.b.min= b; }
         if (b > s.b.max) { s.b.max= b; }
         s.b.s.m[1]+= b;      //sum1+= b;
         s.b.s.m[2]+= b * b;  //sum2+= b * b;
      }
      s.a.s.m[0]= s.b.s.m[0]= n;
   }
   if (pS) { *pS= s; }
   //else
   {
      printf("summarise() - \n\t   min, max, sum1, sum2\n");
      printFS("\tA: ", &(s.a), "\n");
      printFS("\tB: ", &(s.b), "\n");
   }
} // procSummarise

