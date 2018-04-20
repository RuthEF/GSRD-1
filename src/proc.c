//// proc.c - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#include "proc.h"

#ifdef ACC
#include <openacc.h>
#endif
#ifdef OMP
#include <omp.h>
#endif
//#ifdef MPI
//#include <mpi.h>
//#endif
//#include <mpi.h>

#ifdef __PGI   // HACKY
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
   U8 iHost;
   U8 pad[1];
} AccDevTable;

typedef struct
{
   U32 o, n; // offset & count
} SDC; // Sub-Domain-Copy

typedef struct
{
   MinMaxU32 mm;  // Y spans
   SDC in, out, upd;
} DomSub;

typedef struct
{
   DomSub  d[2];
   AccDev   dev;
   U8 pad[2];
} DSMapNode;

// typedef struct { V2U32 min, max; } DomSub2D;

static AccDevTable gDev={0,};


/*** Dummies for build compatibility ***/
#ifndef ACC
#define acc_device_host 0
#define acc_device_nvidia 0
#define acc_device_not_host 0
int acc_get_num_devices (int t) { return(0); }
int acc_get_device_num (int t) { return(-1); }
void acc_set_device_num (int n, int t) { ; }
void acc_wait_all (void) { ; }
#endif
#ifndef OMP
int omp_get_max_threads (void) { return(0); }
int omp_get_num_threads (void) { return(0); }
int omp_get_thread_num (void) { return(0); }
#endif

/*** Misc util ***/

static void addDevType (AccDevTable *pT, const U8 c, const U8 nC)
{
   int n= nC;
   U8 nA= 0;
   while (--n >= 0)
   {
      if (pT->nDev < ACC_DEV_MAX)
      {
         pT->d[pT->nDev].c= c;
         pT->d[pT->nDev].n= n;
         ++(pT->nDev);
      }
   }
} // addDevType

/*** Application (model) routines ***/

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
   #pragma acc data present_or_create( pTR[:pO->n] ) copy( pSR[:pO->n] ) \
                    present_or_copyin( pO[:1], pP[:1] )
   {
      for (U32 i= 0; i < nI; ++i )
      {
         procA(pTR,pSR,pO,&(pP->base));
         procA(pSR,pTR,pO,&(pP->base));
      }
   }
   return(2*nI);
} // proc2IA

U32 proc2I1A (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 nI)
{
   #pragma acc data present_or_create( pR[:pO->n] ) copyin( pS[:pO->n] ) copyout( pR[:pO->n] ) \
                    present_or_copyin( pO[:1], pP[:1] )
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


/*** multi-device testing ***/
/* DEPRECATE
INLINE void procAXYDS
(
   Scalar * restrict pR,
   const Scalar * restrict pS,
   const ImgOrg * pO,
   const BaseParamVal * pP,
   const DomSub d[2]
)
{
//present( pR[d[0].in.o:d[0].in.n], pS[d[0].in.o:d[0].in.n], pR[d[1].in.o:d[1].in.n], pS[d[1].in.o:d[1].in.n] ) 
   #pragma acc data present( pR[:pO->n], pS[:pO->n], pO[:1], pP[:1], d[:2] )
   {
      #pragma acc parallel loop
      for (U32 i= 0; i < 2; ++i )
      {
         //pragma acc parallel loop
         for (U32 y= d[i].mm.min; y <= d[i].mm.max; ++y )
         {
            #pragma acc loop vector
            for (U32 x= 0; x < pO->def.x; ++x )
            {
               proc1XY(pR, pS, x, y, pO, pP);
            }
         }
      }
   }
} // procAXYDS
*/
void procB
(
   Scalar * restrict pR,
   Scalar * restrict pS,
   const ImgOrg       * pO,
   const BaseParamVal * pP,
   const DSMapNode   * pDSMN
)
{
   const DomSub * pD= pDSMN->d;
   const size_t i0a= pD[0].in.o, i0n= pD[0].in.n;
   const size_t i0b= i0a + pO->stride[2];
   const size_t i1a= pD[1].in.o, i1n= pD[1].in.n;
   const size_t i1b= i1a + pO->stride[2];

   const size_t o0a= pD[0].upd.o, o0n= pD[0].upd.n;
   const size_t o0b= i0a + pO->stride[2];
   const size_t o1a= pD[1].upd.o, o1n= pD[1].upd.n;
   const size_t o1b= i1a + pO->stride[2];

   acc_set_device_num( pDSMN->dev.n, pDSMN->dev.c );
   /*pragma acc data present_or_create( pTR[ i0a:i0n ], pTR[ i1a:i1n ], pTR[ i0b:i0n ], pTR[ i1b:i1n ] ) \
               copyin( pSR[ i0a:i0n ], pSR[ i1a:i1n ], pSR[ i0b:i0n ], pSR[ i1b:i1n ] ) \
               copyout( pTR[ o0a:o0n ], pTR[ o1a:o1n ], pTR[ o0b:o0n ], pTR[ o1b:o1n ] ) \
               copyin( pO[:1], pP[:1], pD[:2] )
*/
   #pragma acc data present_or_create( pR[:pO->n] ) copy( pS[:pO->n] ) present_or_copyin( pO[:1], pP[:1], pD[:2] ) 
   #pragma acc parallel async( pDSMN->dev.n )
   {
      //pragma acc parallel loop
      for (U32 i= 0; i < 2; ++i )
      {
         //pragma acc parallel loop
         for (U32 y= pD[i].mm.min; y <= pD[i].mm.max; ++y )
         {
            #pragma acc loop vector
            for (U32 x= 0; x < pO->def.x; ++x )
            {
               proc1XY(pR, pS, x, y, pO, pP);
            }
         }
      }
   }
} // procB

U32 hackMD
(
   Scalar * restrict pTR,
   Scalar * restrict pSR,
   const ImgOrg   * pO,
   const ParamVal * pP,
   const U32 nI
)
{
   DSMapNode aD[3];
   DomSub *pD;
   U32 nD= 0, mD= MIN(2, gDev.nDev);
   if (mD > 1)
   {
      U32 o= 0, n= pO->def.y;
      if (gDev.iHost < gDev.nDev)
      {
         U32 se= 7;
         aD[nD].dev= gDev.d[gDev.iHost];
         pD= aD[nD].d;

         pD->mm.min= 0;
         pD->mm.max= se - 1;
         pD->in.o= 0;
         pD->in.n= 2 + pD->mm.max - pD->mm.min;
         pD->upd= pD->out= pD->in;
         pD++;
         pD->mm.min= pO->def.y - se;
         pD->mm.max= pO->def.y - 1;
         pD->in.o= pD->mm.min - 1;
         pD->in.n= 2 + pD->mm.max - pD->mm.min;
         pD->upd= pD->out= pD->in;
         nD++;
         o+= se;
         n-= 2 * se;
      }
      if (nD < mD)
      {
         U32 iNH= 0;
         iNH+= (0 == gDev.iHost);

         aD[nD].dev= gDev.d[iNH];
         pD= aD[nD].d;

         pD[0].mm.min= o;
         pD[0].mm.max= pD[0].mm.min + (n / 2) - 1;
         pD[0].in.o= pD[0].mm.min - 1;
         pD[0].in.n= 2 + pD[0].mm.max - pD[0].mm.min;
         pD[0].upd= pD[0].out= pD[0].in;

         pD[1].mm.min= pD[0].mm.max + 1;
         pD[1].mm.max= pD[1].mm.min + n - (n / 2) - 1;
         pD[1].in.o= pD[1].mm.min;
         pD[1].in.n= 2 + pD[1].mm.max - pD[1].mm.min;
         pD[1].upd= pD[1].out= pD[1].in;
        nD++;
      }

      for ( U32 j=0; j<nD; j++)
      {
         for ( U32 i= 0; i<2; ++i )
         {
            aD[j].d[i].in.o*=  pO->stride[1];
            aD[j].d[i].in.n*=  pO->stride[1];
            aD[j].d[i].out.o*= pO->stride[1];
            aD[j].d[i].out.n*= pO->stride[1];
            aD[j].d[i].upd.o*= pO->stride[1];
            aD[j].d[i].upd.n*= pO->stride[1];
         }

         printf("[%u] %u:%u\n", j, aD[j].dev.c, aD[j].dev.n); pD= aD[j].d; // dump
         printf("   mm \t%u,%u; %u,%u\n", pD[0].mm.min, pD[0].mm.max, pD[1].mm.min, pD[1].mm.max);
         printf("   in \t%u,%u; %u,%u\n", pD[0].in.o, pD[0].in.n, pD[1].in.o, pD[1].in.n);
         printf("   out\t%u,%u; %u,%u\n", pD[0].out.o, pD[0].out.n, pD[1].out.o, pD[1].out.n);
         printf("   upd\t%u,%u; %u,%u\n", pD[0].upd.o, pD[0].out.n, pD[1].out.o, pD[1].out.n);
      }
      if (nD > 1)
      {
         for (U32 i= 0; i < nI; ++i )
         {
            //pragma omp parallel for
            for (U32 j= 0; j < nD; ++j)
            {
               procB(pTR, pSR, pO, &(pP->base), aD+j);
            }
            //pragma omp parallel for
            for (U32 j= 0; j < nD; ++j)
            {
               procB(pSR, pTR, pO, &(pP->base), aD+i);
            }
         }
         return(2 * nI);
      }
   }
   return(0);
} // hackMD

/*** External Interface ***/

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
   int initOK= (0 == (f & (PROC_FLAG_ACCGPU|PROC_FLAG_ACCHOST)));
   int nNV= acc_get_num_devices( acc_device_nvidia );
   int nH= acc_get_num_devices( acc_device_host );
   int nNH= acc_get_num_devices( acc_device_not_host );

   printf("procInitAcc() - nH=%d nNV=%d, (other=%d)\n", nH, nNV, nNH - nNV);
   gDev.nDev= 0;
   gDev.iHost= -1;
   if ((nH > 0) && (f & PROC_FLAG_ACCHOST))
   {
      printf("\tH:id=%d\n", acc_get_device_num(acc_device_host));
      addDevType(&gDev, acc_device_host, nH);
      gDev.iHost= 0;
   }
   if ((nNV > 0) && (f & PROC_FLAG_ACCGPU))
   {
      printf("\tNV:id=%d\n", acc_get_device_num(acc_device_nvidia));
      addDevType(&gDev, acc_device_nvidia, nH);
   }
   initOK+= gDev.nDev;
   if (gDev.nDev > 0)
   {
      gDev.iCurr= 0;
      acc_set_device_num( gDev.d[0].n, gDev.d[0].c );
   }
   return(initOK > 0);
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
//

U32 procNI
(
   Scalar * restrict pR,
   Scalar * restrict pS,
   const ImgOrg   * pO,
   const ParamVal * pP,
   const U32 nI
)
{
   if (nI & 1) return proc2I1A(pR, pS, pO, pP, nI>>1);
   else
   {
      U32 n= hackMD(pR, pS, pO, pP, nI>>1);
      if (0 == n) { n= proc2IA(pR, pS, pO, pP, nI>>1); }
      return(n);
   }
} // procNI

void procTest (void)
{
#ifdef OMP
   int i, n= omp_get_max_threads();
   printf("procTest(%d)-\n", n);
   //n= MIN(2,n);
   #pragma omp parallel num_threads(n)
   {
      n= omp_get_num_threads();
      i= omp_get_thread_num();
      printf("Hello %d / %d\n", i, n);
   }
   printf("-procTest()\n");
#endif
} // procTest

void procMPITest (void)
{
//#ifdef MPI
   int rank,size; 
   /* Initialize the MPI library */ 
   //MPI_Init(&argc,&argv);
   
   /* Determine the calling process rank and total number of ranks */ 
   //MPI_Comm_rank(MPI_COMM_WORLD,&rank); 
   //MPI_Comm_size(MPI_COMM_WORLD,&size); 
   /* Call MPI routines like MPI_Send, MPI_Recv, ... 
   */ ... /* 
   Shutdown MPI library */ 
    //MPI_Finalize();
 
 printf("-procMPITest()\n");
//#endif
} // procMPITest
