#include "proc.h"

#ifdef __PGI
//OPENACC
#include <openacc.h>
//#include <assert.h>
//#include <accelmath.h>
#endif

#ifdef OPENACC
flubber
#endif


/***/

#define LAPLACE2D2S9P(pS,i,sv,k) ( (pS)[0] * k[0] + \
   ( (pS)[ i - sv.x ] + (pS)[ i + sv.x ] + (pS)[ i - sv.y ] + (pS)[ i + sv.y ] ) * k[1] + \
   ( (pS)[ i - sv.x - sv.y ] + (pS)[ i + sv.x - sv.y ] + (pS)[ i + sv.y - sv.x ] + (pS)[ i + sv.x + sv.y ] ) * k[2] )

// Stride 0,1 -> +X +Y
//#pragma acc routine
inline Scalar laplace2D2S9P (const Scalar * const pS, const Stride s[2], const Scalar k[3])
{
   return( pS[0] * k[0] +
          (pS[-s[0]] + pS[s[0]] + pS[-s[1]] + pS[s[1]]) * k[1] +
          (pS[-s[1]-s[0]] + pS[-s[1]+s[0]] + pS[s[1]-s[0]] + pS[s[1]+s[0]]) * k[2] );
} // laplace2D2S9P

// Stride 0..3 -> +-X +-Y
//#pragma acc routine vector
inline Scalar laplace2D4S9P (const Scalar * const pS, const Stride s[4], const Scalar k[3])
{
   return( pS[0] * k[0] +
           (pS[ s[0] ] + pS[ s[1] ] + pS[ s[2] ] + pS[ s[3] ]) * k[1] + 
           (pS[ s[0]+s[2] ] + pS[ s[0]+s[3] ] + pS[ s[1]+s[2] ] + pS[ s[1]+s[3] ]) * k[2] ); 
} // laplace2D4S9P

//#pragma acc routine vector
inline void proc1 (Scalar * const pR, const Scalar * const pI, const int i, const int j, const Stride wrap[4], const ParamVal * const pP)
{
   const Scalar * const pA= pI+i, a= *pA;
   const Scalar * const pB= pI+i+j, b= *pB;
   const Scalar rab2= pP->kRR * a * b * b;
#if 0
   pR->pA[i]= a + LAPLACE2D4S9P(pA, wrap, pP->kL.a) - rab2 + pP->kRA * (1 - a);
   pR->pB[i+j]= b + LAPLACE2D4S9P(pB, wrap, pP->kL.b) + rab2 - pP->kDB * b;
#else
   pR[i]= a + laplace2D4S9P(pA, wrap, pP->kL.a) - rab2 + pP->kRA * (1 - a);
   pR[i+j]= b + laplace2D4S9P(pB, wrap, pP->kL.b) + rab2 - pP->kDB * b;
#endif
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

Bool32 procInitAcc (void) // arg param ?
{
   int nNV= acc_get_num_devices( acc_device_nvidia );
   int nH= acc_get_num_devices( acc_device_host );
   int nNH= acc_get_num_devices( acc_device_not_host );

   printf("procInitAcc() - nNV=%d, nH=%d, nNH=%d\n", nNV, nH, nNH);
   if (nNV > 0)
   {
      acc_init( acc_device_nvidia );
   }
   else if (nH > 0)
   {
      acc_init( acc_device_host );
   }
   else { return(FALSE); }
   // Default assume success
   return(TRUE);
} // procInitAcc

void procBindData (const HostBuff * const pHB, const ParamVal * const pP, const ImgOrg * const pO, const U32 iS)
{
   U32 iR = iS ^ 1;
   Scalar * restrict pS= pHB->pAB[iS];
   Scalar * restrict pR= pHB->pAB[iR];
   #pragma acc data copyin( pP[0:1], pO[0:1] )
   #pragma acc data copyin( pP->pKRR[0:pP->n], pP->pKRA[0:pP->n], pP->pKDB[0:pP->n] )
   #pragma acc data copyin( pS[0:pO->n] )
   #pragma acc data create( pR[0:pO->n] )
   ;
   return;
} // bindData

// _rab2= KR0 * a * b * b
// a+= laplace9P(a, KLA0) - _rab2 + mKRA * (1 - a)
// b+= laplace9P(b, KLB0) + _rab2 - mKDB * b
U32 proc (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 iterMax)
{
   const SV2 sv2={pO->stride[0],pO->stride[1]};
   Stride wrap[6]; // LRBT strides for boundary wrap
   const V2U32 end= {pO->def.x-1, pO->def.y-1};
   U32 x, y, iter= 0;

      #pragma acc data present( pP, pO, pS[0:pO->n], pR[0:pO->n]  )
      #pragma acc data copyout( pR[0:pO->n] )
      for ( iter= 0; iter < iterMax; ++iter )
      {
	 // Process according to memory access pattern (boundary wrap)
	 // First the interior
	 // private( pR[0:pO->n] )
#ifdef ACC_AUTO
         #pragma acc kernels loop
#else
         #pragma acc parallel loop
#endif
         for ( y= 1; y < end.y; ++y )
         {
            #pragma acc loop vector	// gang, vector(256)
            for ( x= 1; x < end.x; ++x )
            {
               const Index i= y * pO->stride[1] + x * pO->stride[0];
               const Index j= i + pO->stride[3];
               const Scalar a= pS[i];
               const Scalar b= pS[j];
               const Scalar rab2= pP->pKRR[x] * a * b * b;
               //const Scalar ar= KRA0 * (1 - a);
               //const Scalar bd= KDB0 * b;
#ifndef LAPLACE_FUNCTION
               pR[i]= a + LAPLACE2D2S9P(pS, i, sv2, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a);
               pR[j]= b + LAPLACE2D2S9P(pS, j, sv2, pP->kL.b) + rab2 - pP->pKDB[y] * b;
#else
               pR[i]= a + laplace2D2S9P(pS+i, pO->stride, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a);
               pR[j]= b + laplace2D2S9P(pS+j, pO->stride, pP->kL.b) + rab2 - pP->pKDB[y] * b;
#endif
            }
         }

         // Boundaries
#if 1 // Non-corner Edges
         // Share the two middle wrap strides between edges (order unimportant due to symmetric convolution kernel)
         //wrap[0]= def.x; wrap[1]= def.x*(def.y-1);	// 0..3 LO, 2..5 HI
         //wrap[2]= -1; wrap[3]= 1;
         //wrap[4]= -def.x*(def.y-1); wrap[5]= -def.x;

         wrap[0]= pO->stride[1]; wrap[1]= pO->stride[2] - pO->stride[1];	// 0..3 LO, 2..5 HI
         wrap[2]= -pO->stride[0]; wrap[3]= pO->stride[0];
         wrap[4]= pO->stride[1] - pO->stride[2]; wrap[5]= -pO->stride[1];

         // Horizontal top & bottom
#ifdef ACC_AUTO
         #pragma acc kernels loop
#else
         #pragma acc parallel loop
#endif
         for ( x= 1; x < end.x; ++x )
         {
            const Index i1= x * pO->stride[0];
            const Index j1= i1 + pO->stride[3];
            const Scalar a1= pS[i1];
            const Scalar b1= pS[j1];
            Scalar rab2= pP->pKRR[x] * a1 * b1 * b1;
            //const Scalar ar1= KRA0 * (1 - a1);
            //const Scalar bd1= KDB0 * b1;
            pR[i1]= a1 + laplace2D4S9P(pS+i1, wrap+0, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a1);
            pR[j1]= b1 + laplace2D4S9P(pS+j1, wrap+0, pP->kL.b) + rab2 - pP->pKDB[0] * b1;

            const Stride offsY= pO->stride[2] - pO->stride[1];
            const Index i2= i1 + offsY;
            const Index j2= j1 + offsY;
            const Scalar a2= pS[i2];
            const Scalar b2= pS[j2];
            rab2= pP->pKRR[x] * a2 * b2 * b2;
            pR[i2]= a2 + laplace2D4S9P(pS+i2, wrap+2, pP->kL.a) - rab2 + pP->pKRA[x] * (1 - a2);
            pR[j2]= b2 + laplace2D4S9P(pS+j2, wrap+2, pP->kL.b) + rab2 - pP->pKDB[end.y] * b2;
         }
#endif
#if 1
         // Vertical edges (non-corner)
         //wrap[0]= def.x-1; wrap[1]= 1;
         //wrap[2]= def.x; wrap[3]= -def.x;
         //wrap[4]= -1; wrap[5]= 1-def.x;

         wrap[0]= pO->stride[1] - pO->stride[0]; wrap[1]= pO->stride[0];
         wrap[2]= pO->stride[1]; wrap[3]= -pO->stride[1];
         wrap[4]= -pO->stride[0]; wrap[5]= pO->stride[0] - pO->stride[1];

         // left & right
#ifdef ACC_AUTO
         #pragma acc kernels loop
#else
         #pragma acc parallel loop
#endif
         for ( y= 1; y < end.y; ++y )
         {
            Scalar a, b, rab2;
            const Index i1= y * pO->stride[1];
            const Index j1= i1 + pO->stride[3];
            a= pS[i1];
            b= pS[j1];
            rab2= pP->pKRR[0] * a * b * b;
            pR[i1]= a + laplace2D4S9P(pS+i1, wrap+0, pP->kL.a) - rab2 + pP->pKRA[0] * (1 - a);
            pR[j1]= b + laplace2D4S9P(pS+j1, wrap+0, pP->kL.b) + rab2 - pP->pKDB[y] * b;

            const Index offsX= pO->stride[1] - pO->stride[0];
            const Index i2= i1 + offsX;
            const Index j2= j1 + offsX;
            a= pS[i2];
            b= pS[j2];
            rab2= pP->pKRR[end.x] * a * b * b;
            pR[i2]= a + laplace2D4S9P(pS+i2, wrap+2, pP->kL.a) - rab2 + pP->pKRA[end.x] * (1 - a);
            pR[j2]= b + laplace2D4S9P(pS+j2, wrap+2, pP->kL.b) + rab2 - pP->pKDB[y] * b;
         }
#endif
#if 1	// The four corners: R,L * B,T
         //wrap[0]= def.x-1; wrap[1]= 1;
         //wrap[2]= def.x; wrap[3]= def.x*(def.y-1);
         //wrap[4]= -1; wrap[5]= 1-def.x;

         wrap[0]= pO->stride[1] - pO->stride[0]; wrap[1]= pO->stride[0];
         wrap[2]= pO->stride[1]; wrap[3]= pO->stride[2] - pO->stride[1];
         wrap[4]= -pO->stride[0]; wrap[5]= pO->stride[0] - pO->stride[1];

         proc1(pR, pS, 0, pO->stride[3], wrap+0, pP);
         proc1(pR, pS, pO->stride[1]-pO->stride[0], pO->stride[3], wrap+2, pP);

         //wrap[0]= def.x-1; wrap[1]= 1; 
         //wrap[2]= -def.x*(def.y-1); wrap[3]= -def.x;
         wrap[2]= -wrap[2]; wrap[3]= -wrap[3];

         proc1(pR, pS, pO->stride[2]-pO->stride[1], pO->stride[3], wrap+0, pP);
         proc1(pR, pS, pO->stride[2]-pO->stride[0], pO->stride[3], wrap+2, pP);
#endif
         if (iter < iterMax) { Scalar *pT= (Scalar*)pS; pS= pR; pR= pT; } // SWAP()
      } // for iter < iterMax
   return(iter);
} // proc
