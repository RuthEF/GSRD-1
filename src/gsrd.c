#ifdef OPENACC
#include <openacc.h>
//#include <assert.h>
//#include <accelmath.h>
#endif
#include "data.h"

#if defined(_WIN32) || defined(_WIN64)
#include <sys/timeb.h>
#define GETTIME(a) _ftime(a)
#define USEC(t1,t2) ((((t2).time-(t1).time)*1000+((t2).millitm-(t1).millitm))*100)
typedef struct _timeb timestruct;
#else
#include <sys/time.h>
#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))
typedef struct timeval timestruct;
#endif


typedef struct
{
   double min, max, sum;
} FieldStat;

typedef struct
{
   FieldStat a, b;
} BlockStat;

typedef struct
{
   MemBuff  buff;
   ParamVal pv;
   HostBuff hb;
   ImgOrg   org;
   U32      i;
} Context;

/***/

static const Scalar gKL[3]= {-1, 0.2, 0.05};

static Context gCtx={0};

/***/
Context *initCtx (Context * const pC, U16 w, U16 h, U16 nF)
{
   if (0 == w) { w= 256; }
   if (0 == h) { h= 256; }
   if (0 == nF) { nF= 4; }
   const size_t n= w * h;
   const size_t b= paramBytes(w,h) + n * nF * sizeof(Scalar);
   if (b > n)
   {
      void *p= malloc(b);
      if (p)
      {
         pC->buff.p= p;
         pC->buff.bytes= b;

         pC->hb.pAB[0]= p;
         pC->hb.pAB[0]+= initParam(&(pC->pv), p, w, h, 0.100, 0.005);
         if (nF >= 5) { pC->hb.pC= pC->hb.pAB[0]; pC->hb.pAB[0]+= n; } else { pC->hb.pC= NULL; }
         pC->hb.pAB[1]= pC->hb.pAB[0] + 2 * n;
         initOrg(&(pC->org), w, h, 0);
         pC->i= 0;
         return(pC);
      }
   }
   return(NULL);
} // initCtx

void releaseCtx (Context * const pC)
{
   if (pC && pC->buff.p && (pC->buff.bytes > 0)) { free(pC->buff.p); memset(pC, 0, sizeof(*pC)); }
} // releaseCtx

size_t initBuff (const HostBuff *pB, const V2U32 d, const U16 step)
{
   const size_t n= d.x * d.y;
   size_t  i, nB= 0;
   U16 x, y;

   for ( i= 0; i < n; ++i ) { pB->pAB[0][i]= 1.0; pB->pAB[0][n+i]= 0.0; }
   for ( y= step; y < d.y; y+= step )
   {
      for ( x= step; x < d.x; x+= step )
      {
         i= y * d.x + x;
         if (i < n) { pB->pAB[0][n+i]= 1.0; nB++; }
      }
   }
   return(nB);
} // initBuff

void saveFrame (const Scalar * const pB, const V2U32 d, const U32 i)
{
   char name[64];
   const size_t n= d.x * d.y * 2;

   if (n > 0)
   {
      snprintf(name, sizeof(name)-1, "raw/gsrd%05lu(%lu,%lu,2)F64.raw", i, d.x, d.y);
      if (saveBuff(pB, sizeof(Scalar) * n, name))
      {
         printf("saveFrame() - %s %p %zu bytes\n", name, pB, r);
      }
   }
} // saveFrame


#define LAPLACE2D2S9P(pS,i,sv,k) ( (pS)[0] * k[0] + \
   ( (pS)[ i - sv.x ] + (pS)[ i + sv.x ] + (pS)[ i - sv.y ] + (pS)[ i + sv.y ] ) * k[1] + \
   ( (pS)[ i - sv.x - sv.y ] + (pS)[ i + sv.x - sv.y ] + (pS)[ i + sv.y - sv.x ] + (pS)[ i + sv.x + sv.y ] ) * k[2] )

// Stride 0,1 -> +X +Y
inline Scalar laplace2D2S9P (const Scalar * const pS, const Stride s[2], const Scalar k[3])
{
   return( pS[0] * k[0] +
          (pS[-s[0]] + pS[s[0]] + pS[-s[1]] + pS[s[1]]) * k[1] +
          (pS[-s[1]-s[0]] + pS[-s[1]+s[0]] + pS[s[1]-s[0]] + pS[s[1]+s[0]]) * k[2] );
} // laplace2D2S9P

// Stride 0..3 -> +-X +-Y
inline Scalar laplace2D4S9P (const Scalar * const pS, const Stride s[4], const Scalar k[3])
{
   return( pS[0] * k[0] +
           (pS[ s[0] ] + pS[ s[1] ] + pS[ s[2] ] + pS[ s[3] ]) * k[1] + 
           (pS[ s[0]+s[2] ] + pS[ s[0]+s[3] ] + pS[ s[1]+s[2] ] + pS[ s[1]+s[3] ]) * k[2] ); 
} // laplace2D4S9P

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

void bindCtx (const Context * pC)
{
   const ParamVal *pP= &(pC->pv);
   const ImgOrg   *pO= &(pC->org);
   U32 iS= pC->i & 1;
   U32 iR = iS ^ 1;
   Scalar * pS= pC->hb.pAB[iS];
   Scalar * pR= pC->hb.pAB[iR];
   #pragma acc data copyin( pC->org, pC->pv )
   #pragma acc data copyin( pP->pKRR[0:pP->n], pP->pKRA[0:pP->n], pP->pKDB[0:pP->n] )
   #pragma acc data copyin( pS[0:pO->n] )
   #pragma acc data create( pR[0:pO->n] )
   ;
   return;
} // bindCtx

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
         #pragma acc kernels loop
         for ( y= 1; y < end.y; ++y )
         {
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
         #pragma acc kernels loop
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
         #pragma acc kernels loop
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

void summarise (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO)
{  // HACKY ignores interleaving/padding
   const size_t n= pO->def.x * pO->def.y;
   const Scalar * const pA= pAB;
   const Scalar * const pB= pAB + pO->stride[3];
   BlockStat s;
   s.a.min= s.a.max= s.a.sum= pA[0];
   s.b.min= s.b.max= s.b.sum= pB[0];
   for (size_t i=1; i<n; i++)
   {
      const Index j= i * pO->stride[0];
      const Scalar a= pA[j];
      if (a < s.a.min) { s.a.min= a; }
      if (a > s.a.max) { s.a.max= a; }
      s.a.sum+= a;
      const Scalar b= pB[j];
      if (b < s.b.min) { s.b.min= b; }
      if (b > s.b.max) { s.b.max= b; }
      s.b.sum+= b;
   }
   if (pS) { *pS= s; }
   //else
   {
      printf("summarise() - \n\t   min, max, sum\n");
      printf("\tA: %G, %G, %G\n", s.a.min, s.a.max, s.a.sum);
      printf("\tB: %G, %G, %G\n", s.b.min, s.b.max, s.b.sum);
   }
} // summarise

int main ( int argc, char* argv[] )
{
   int n= 0, i= 0, nErr= 0;
   ArgInfo ai={0,};

   if (argc > 1)
   {
      n= scanArgs(&ai, argv+1, argc-1);
      if (ai.dfi.nV > 0)
      {
         size_t bits= ai.dfi.elemBits;
         printf("%s -> v[%d]=(", ai.dfi.name, ai.dfi.nV);
         for (int i=0; i < ai.dfi.nV; i++)
         {
            bits*= ai.dfi.v[i];
            printf("%d,", ai.dfi.v[i]);
         }
         printf(")*%u\n", ai.dfi.elemBits);
         if (ai.dfi.bytes != ((bits+7) >> 3)) { printf("WARNING: %zu != %zu", ai.dfi.bytes, bits); }
      }
   }
   //acc_init( acc_device_nvidia );

   const DataFileInfo *pDFI= &(ai.dfi);
   const ProcInfo *pPI= &(ai.proc);
   if (initCtx(&gCtx, pDFI->v[0], pDFI->v[1], 4))
   {
      size_t i= 0, iM= pPI->maxIter, iR;
      timestruct t1, t2;
      Scalar tE0=0, tE1=0;
      BlockStat bs={0};

      if (0 == loadBuff(gCtx.hb.pAB[0], pDFI->path, pDFI->bytes))
         //printf("nB=%zu\n",
      {
         initBuff(&(gCtx.hb), gCtx.org.def, 32);
         saveFrame(gCtx.hb.pAB[0], gCtx.org.def, gCtx.i);
      }
      if (pPI->subIter > 0) { iM= pPI->subIter; }
      //printf("iter=%zu,%zu\n", pPI->subIter, pPI->maxIter);

      bindCtx( &gCtx );
      do
      {
         int k= gCtx.i & 0x1;

         iR= pPI->maxIter - gCtx.i;
         if (iM > iR) { iM= iR; }

         GETTIME(&t1);
         gCtx.i+= proc(gCtx.hb.pAB[(k^0x1)], gCtx.hb.pAB[k], &(gCtx.org), &(gCtx.pv), iM);
         GETTIME(&t2);

         k= gCtx.i & 0x1;
         summarise(&bs, gCtx.hb.pAB[k], &(gCtx.org));
         tE0= 1E-6 * USEC(t1,t2);
         tE1+= tE0;
         printf("\ttE= %G, %G\n", tE0, tE1);
         saveFrame(gCtx.hb.pAB[k], gCtx.org.def, gCtx.i);
      } while (gCtx.i < pPI->maxIter);
      releaseCtx(&gCtx);
   }

   if ( nErr != 0 ) { printf( "Test FAILED\n"); }
   else {printf( "Test PASSED\n"); }

   return(0);
} // main
