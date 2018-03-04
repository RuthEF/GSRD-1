
#include "proc.h"

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

         initOrg(&(pC->org), w, h, 0);
         pC->hb.pAB[0]= p;
         pC->hb.pAB[0]+= initParam(&(pC->pv), p, gKL, &(pC->org.def), 0.100, 0.005);
         if (nF >= 5) { pC->hb.pC= pC->hb.pAB[0]; pC->hb.pAB[0]+= n; } else { pC->hb.pC= NULL; }
         pC->hb.pAB[1]= pC->hb.pAB[0] + 2 * n;
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

size_t saveFrame (const Scalar * const pB, const V2U32 d, const U32 i)
{
   char name[64];
   const size_t n= d.x * d.y * 2;
   size_t r= 0;
   if (n > 0)
   {
      snprintf(name, sizeof(name)-1, "raw/gsrd%05lu(%lu,%lu,2)F64.raw", i, d.x, d.y);
      r= saveBuff(pB, name, sizeof(Scalar) * n);
      if (r > 0)
      {
         printf("saveFrame() - %s %p %zu bytes\n", name, pB, r);
      }
   }
   return(r);
} // saveFrame

void bindCtx (const Context * pC)
{
   procBindData( &(pC->hb), &(pC->pv), &(pC->org), pC->i );
/*
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
*/
} // bindCtx

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

   const DataFileInfo *pDFI= &(ai.dfi);
   const ProcInfo *pPI= &(ai.proc);
   if (procInitAcc() && initCtx(&gCtx, pDFI->v[0], pDFI->v[1], 4))
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
