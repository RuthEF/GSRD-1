#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef __PGI   // HACK
#define INLINE inline
//#include <linux/ktime.h>
#else // clang
#define INLINE
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))
typedef struct timeval timestruct;


//typedef float Scalar;
typedef double Scalar;
typedef struct { void *p; size_t bytes; } MemBuff;
typedef struct
{
   MemBuff buff;
   Scalar  *pI, *pE1, *pE2, *pR1, *pR2;
   size_t  n;
} DataContext;

typedef double SMVal; // Stat measure value

typedef struct
{
   SMVal m[3];
} StatMom;

typedef struct
{
   size_t nD, nNE, nNR, nXER, nEL;
   StatMom s;
} Diff;

typedef struct
{
   StatMom  c[2];
   Diff     d;
} Analysis;

/*---*/

void vSet (Scalar * pR, const size_t n, const Scalar v)
{
   for (size_t i= 0; i < n; ++i ) { pR[i]= v; }
} // vSet

void vAdd (Scalar * restrict pR, const Scalar * const pV1, const Scalar * const pV2, const size_t n)
{
   for (size_t i= 0; i < n; ++i ) { pR[i]= pV1[i] + pV2[i]; }
} // vAdd

void vAddA (Scalar * restrict pR, const Scalar * const pV1, const Scalar * const pV2, const size_t n)
{
   #pragma acc parallel loop
   for (size_t i= 0; i < n; ++i ) { pR[i]= pV1[i] + pV2[i]; }
} // vAddA

void diffuse (Scalar * restrict pR, const Scalar * const pS, const size_t n, const Scalar w[3])
{
   const size_t m= n - 1;
   pR[0]= w[0] * pS[m] + w[1] * pS[0] * w[2] + pS[1];	// periodic boundary wrap
   pR[m]= w[0] * pS[m-1] + w[1] * pS[m] + w[2] * pS[0]; 
   for (size_t i= 1; i < m; ++i ) { pR[i]= w[0] * pS[i-1] + w[1] * pS[i] + w[2] * pS[i+1]; }
} // diffuse

void diffuseAG (Scalar * restrict pR, const Scalar * const pS, const size_t n, const Scalar w[3])
{
// implicit copyin(w[:3],pS[-1:n+2]) copyout(pR[:n]) => GHOST WRAP, WRONG!
   #pragma acc parallel loop
   for (size_t i= 0; i < n; ++i ) { pR[i]= w[0] * pS[i-1] + w[1] * pS[i] + w[2] * pS[i+1]; }
} // diffuseAG

INLINE void diffuseA (Scalar * restrict pR, const Scalar * const pS, const size_t n, const Scalar w[3])
{
   #pragma acc data present( pR[:n], pS[:n], w[3] )
   {
      const size_t m= n - 1;
      #pragma acc parallel loop
      for (size_t i= 1; i < m; ++i ) { pR[i]= w[0] * pS[i-1] + w[1] * pS[i] + w[2] * pS[i+1]; }
      #pragma acc parallel       // necessary for correct result
      {
         pR[0]= w[0] * pS[m]   + w[1] * pS[0] * w[2] + pS[1];	// periodic boundary wrap
         pR[m]= w[0] * pS[m-1] + w[1] * pS[m] + w[2] * pS[0];
      }
   }
} // diffuseA

void diffuse1A (Scalar * restrict pR, const Scalar * const pS, const size_t n, const Scalar w[3])
{
   //pragma acc data copy( pR[:n] ) copyin( pS[:n], w[3] )	// FAIL negatives [...i, m-i...]
   //pragma acc data copyout( pR[:n] ) copyin( pS[:n], w[3] )	// FAIL: nexative [0, m] 
   #pragma acc data present_or_create( pR[:n] ) \
			present_or_copyin( pS[:n], w[3] ) \
			copyout( pR[:n] ) 			// FAIL: nexative [0, m]
   diffuseA(pR,pS,n,w);
} // diffuse1A

void diffuse2IA (const size_t nI, Scalar * restrict pTR, Scalar * restrict pSR, const size_t n, const Scalar w[3])
{
   #pragma acc data region present_or_create( pTR[:n] ) copy( pSR[:n] ) present_or_copyin( w[:3] )
   {
      //pragma acc ???
      for ( size_t i= 0; i < nI; ++i )
      {
         diffuseA(pTR,pSR,n,w);
         diffuseA(pSR,pTR,n,w);
      }
   }
} // diffuse2IA

size_t alignPo2M (const size_t s, const size_t po2m)
{
   return( (s + po2m) & ~po2m );
} // alignPo2M

int initBuff (DataContext *pDC, const size_t n, const unsigned char alignShift)
{
   const size_t am= (1 << alignShift) - 1;
   const size_t b= alignPo2M(sizeof(Scalar) * n, am);
   size_t i;
 
   pDC->buff.p= NULL;
   pDC->n= n;
   //pDC->buff.p= malloc(5 * b); // acc data present ( pR ) fails with this style of allocation
   if (pDC->buff.p)
   {
      pDC->buff.bytes= 5 * b;
      pDC->pI= (Scalar*) alignPo2M((size_t)(pDC->buff.p), am);
      pDC->pE1= (Scalar*) alignPo2M((size_t)(pDC->pI), am);
      pDC->pE2=  (Scalar*) alignPo2M((size_t)(pDC->pE1 + n), am);
      pDC->pR1=  (Scalar*) alignPo2M((size_t)(pDC->pE2 + n), am);
      pDC->pR2=  (Scalar*) alignPo2M((size_t)(pDC->pR1 + n), am);
   }
   else
   {
      pDC->pI=  (Scalar*) malloc(b);
      pDC->pE1= (Scalar*) malloc(b);
      pDC->pE2= (Scalar*) malloc(b);
      pDC->pR1= (Scalar*) malloc(b);
      pDC->pR2= (Scalar*) malloc(b);
   }
   printf("init() - I,E1,E2,R1,R2: %p %p %p %p %p\n", pDC->pI, pDC->pE1, pDC->pE2, pDC->pR1, pDC->pR2);
   return(0);
} // initBuff

void initData (DataContext *pDC, const Scalar m)
{
   if (pDC->pI)
   {
      vSet(pDC->pI, pDC->n, 0.0);
      pDC->pI[0]=   m * 0.125;
      pDC->pI[pDC->n-1]= m * 0.125;
      pDC->pI[pDC->n/2]= m * 0.75;
   }
   if (pDC->pE1) { vSet(pDC->pE1, pDC->n, -1.0E6); }
   if (pDC->pE2) { vSet(pDC->pE2, pDC->n, -2.0E7); }
   if (pDC->pR1) { vSet(pDC->pR1, pDC->n, -3.0E8); }
   if (pDC->pR2) { vSet(pDC->pR2, pDC->n, -4.0E9); }
} // initData

void release (DataContext *pDC)
{
   if (pDC)
   {
      if (pDC->buff.p) { free(pDC->buff.p); }
      else
      {
         if (pDC->pI)  { free(pDC->pI); }
         if (pDC->pE1) { free(pDC->pE1); }
         if (pDC->pE2) { free(pDC->pE2); }
         if (pDC->pR2) { free(pDC->pR1); }
         if (pDC->pR2) { free(pDC->pR2); }
      }
      memset(pDC, 0, sizeof(*pDC));
   }
} // release

size_t analyse (Analysis * const pA, const Scalar vr[], const Scalar ve[], const size_t n, const size_t verbosity)
{
   Analysis a= {0,};
   
   a.c[0].m[0]= a.c[1].m[0]= n;
   for(size_t i = 0; i < n; ++i )
   {  // First sum conservation data
      const Scalar r= vr[i], e= ve[i];
      a.c[0].m[1]+= r;
      a.c[0].m[2]+= r * r;
      a.c[1].m[1]+= e;
      a.c[1].m[2]+= e * e;

      // Check for numerical discrepancy...
      unsigned char b=0, sb=0;
      a.d.nD+= b= r != e;
      a.d.s.m[1]+= e-r;
      a.d.s.m[2]+= (e-r) * (e-r);

      // algorithmic error...
      if (b) { a.d.nXER+= b= (e > 0.0) ^ (r > 0.0); sb+= b; }
      a.d.nNE+= b= e < 0.0; sb+= b;
      a.d.nNR+= b= r < 0.0; sb+= b;
      a.d.nEL+= (sb > 0);
      if ((sb > 0) && (a.d.nEL < verbosity))
      {
         printf("[%zu] %G, %G\n", i, r, e);
      }
   }
   a.d.s.m[0]= a.d.nD;  // unweighted
   if (pA) { *pA= a; }
   return(a.d.nNE + a.d.nNR + a.d.nXER);
} // analyse

SMVal deltaT (void)
{
   static struct timeval t[2]={0,};
   static int i= 0;
   SMVal dt;
   GETTIME(t+i);
   dt= 1E-6 * USEC(t[i^1],t[i]);
   i^= 1;
   return(dt);
} // deltaT

int main( int argc, char* argv[] )
{
   size_t iter, i, n, sumErr=0;
   DataContext dc= {NULL, };
   //Scalar *pR, *pV1, *pV2, *pE;
   Analysis a= {0,};

   if( argc > 1 ) { n= atoi( argv[1] ); }
   if ( n <= 0 ) { n= 1<<16; }

   initBuff(&dc, n, 12); // 4K (page) alignment
   //vAdd(dc.pE, dc.pV1, dc.pV2, n);
   //vAddA(dc.pR, dc.pV1, dc.pV2, n);

   {
      SMVal tE[3];
      const Scalar w[3]={ 0.25, 0.5, 0.25 }; // 1D isotropic diffusion weights
      // w[1]= 1.0 - (w[0] + w[2]);
      iter= n / 8;

      // Set initial state & warm-up algorithmic data flow
      initData(&dc, 100.0E50); // MAX=1.79E308
      diffuse(dc.pE1, dc.pI, dc.n, w);
      diffuse1A(dc.pR1, dc.pI, dc.n, w);

      // Start timing
      deltaT();
      for ( i= 0; i < iter; ++i )
      {  // Unaccelerated
         diffuse(dc.pE2, dc.pE1, dc.n, w);
         diffuse(dc.pE1, dc.pE2, dc.n, w);
      }
      tE[0]= deltaT();
      for ( i= 0; i < iter; ++i )
      {  // Accelerated but with potential for unnecessary buffer copies
         diffuse1A(dc.pR2, dc.pR1, dc.n, w);
         diffuse1A(dc.pR1, dc.pR2, dc.n, w);
      }
      tE[1]= deltaT();

      // Optimally accelerated version: 
      // R2 buffer should unchanged if GPU copy works as expected
      vSet(dc.pR2, dc.n, -4E9); 
      // Reset R1 to initial state for meaningful numerical comparison
      diffuse1A(dc.pR1, dc.pI, dc.n, w);

      deltaT();
      diffuse2IA(iter, dc.pR2, dc.pR1, dc.n, w);
      tE[2]= deltaT();
      printf("\ttE= %G, %G, %G\n", tE[0], tE[1], tE[2]);
      
      iter= 2 * i;
   }

   {
      static const char * const m[2]= { "PASS", "FAIL" };
      printf("---\n%zu iterations on %zu elements:\n", iter, dc.n);
      printf("Primary buffers: R1 vs. E1\n");
      sumErr= analyse(&a, dc.pR1, dc.pE1, dc.n, 50);
      printf("\tConservation: %G, %G\n", a.c[0].m[1], a.c[1].m[1]);
      printf("\t%zu discrepancies (M1=%G, M2=%G)\n", a.d.nD, a.d.s.m[1], a.d.s.m[2] );
      printf("\t%zu ErrLoc; %s\n", a.d.nEL, m[sumErr>0] );
      if (sumErr > 0)
      {
         printf("\tNeg: R=%zu E=%zu, Exc: %zu\n", a.d.nNR, a.d.nNE, a.d.nXER);
      }
      printf("Secondary buffers R2 vs. E2\n");
      size_t aux= analyse(&a, dc.pR2, dc.pE2, dc.n, 5);
      printf("\t%zu discrepancies\n", a.d.nD );
   }
   release(&dc);
   return(-(sumErr > 0));
} // main
