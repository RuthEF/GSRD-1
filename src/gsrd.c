
#include "proc.h"

typedef struct
{
   MemBuff  buff; // DEPRECATE
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
   const size_t b2F= 2 * n * sizeof(Scalar);
   pC->buff.p= NULL; pC->buff.bytes= 0;
   if (b2F > n)
   {
      initParam(&(pC->pv), gKL, &(pC->org.def), 0.100, 0.005);
      //printf("initCtx() - %zu bytes @ %p\n", b, p);

      initOrg(&(pC->org), w, h, 0);
      
      pC->hb.pAB[0]= malloc(b2F);
      pC->hb.pAB[1]= malloc(b2F);
      if (pC->hb.pAB[0] && pC->hb.pAB[1])
      {
         if (nF >= 5) { pC->hb.pC= malloc(b2F / 2); } else { pC->hb.pC= NULL; }
         pC->i= 0;
         return(pC);
      }
   }
   return(NULL);
} // initCtx

void releaseCtx (Context * const pC)
{
   //if (pC && pC->buff.p && (pC->buff.bytes > 0)) { free(pC->buff.p); memset(pC, 0, sizeof(*pC)); }
   if (pC)
   {
      free((void*)(pC->pv.pK));
      free(pC->hb.pAB[0]);
      free(pC->hb.pAB[1]);
      memset(pC, 0, sizeof(*pC));
   }
} // releaseCtx

size_t saveFrame (const Scalar * const pB, const V2U32 def, const U32 iNum)
{
   char name[64];
   const size_t n= def.x * def.y * 2;
   size_t r= 0;
   int t= 3;
   if (n > 0)
   {
      snprintf(name, sizeof(name)-1, "raw/gsrd%05lu(%lu,%lu,2)F64.raw", iNum, def.x, def.y);
      do
      {
         r= saveBuff(pB, name, sizeof(Scalar) * n);
      } while (0); // ((r <= 0) && (t-- > 0) && sleep(1));
      //if (r > 0)
      {
         printf("saveFrame() - %s %p %zu bytes\n", name, pB, r);
      }
   }
   return(r);
} // saveFrame

void bindCtx (const Context * pC)
{
   if (pC) { ; }
   //procBindData( &(pC->hb), &(pC->pv), &(pC->org), pC->i );
} // bindCtx

void summarise (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO)
{  // HACKY ignores interleaving/padding
   const size_t n= pO->def.x * pO->def.y;
   const Scalar * const pA= pAB;
   const Scalar * const pB= pAB + pO->stride[3];
   BlockStat s;
   
   initFS(&(s.a), pA);
   initFS(&(s.b), pB);
   
   for (size_t i=1; i<n; i++)
   {
      const Index j= i * pO->stride[0];
      const Scalar a= pA[j];
      if (a < s.a.min) { s.a.min= a; }
      if (a > s.a.max) { s.a.max= a; }
      s.a.s.m[1]+= a;      //sum1+= a;
      s.a.s.m[2]+= a * a;  // sum2+= a * a;
      const Scalar b= pB[j];
      if (b < s.b.min) { s.b.min= b; }
      if (b > s.b.max) { s.b.max= b; }
      s.b.s.m[1]+= b;      //sum1+= b;
      s.b.s.m[2]+= b * b;  //sum2+= b * b;
   }
   s.a.s.m[0]= s.b.s.m[0]= n;
   if (pS) { *pS= s; }
   //else
   {
      printf("summarise() - \n\t   min, max, sum1, sum2\n");
      printFS("\tA: ", &(s.a), "\n");
      printFS("\tB: ", &(s.b), "\n");
   }
} // summarise

int main ( int argc, char* argv[] )
{
   int n= 0, i= 0, nErr= 0;
   ArgInfo ai={0,};

   if (argc > 1)
   {
      n= scanArgs(&ai, (const char **)(argv+1), argc-1);
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
      printf("proc: f=0x%zX, m=%zu, s=%zu\n", ai.proc.flags, ai.proc.maxIter, ai.proc.subIter);
   }

   const DataFileInfo *pDFI= &(ai.dfi);
   const ProcInfo *pPI= &(ai.proc);
   if (procInitAcc(pPI->flags) && initCtx(&gCtx, pDFI->v[0], pDFI->v[1], 4))
   {
      size_t i= 0, iM= pPI->maxIter, iR;
      SMVal tE0=0, tE1=0;
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

         deltaT();
         gCtx.i+= proc2I1A(gCtx.hb.pAB[(k^0x1)], gCtx.hb.pAB[k], &(gCtx.org), &(gCtx.pv), iM>>1);
         tE0= deltaT();
         tE1+= tE0;
         
         k= gCtx.i & 0x1;
         summarise(&bs, gCtx.hb.pAB[k], &(gCtx.org));
         printf("\ttE= %G, %G\n", tE0, tE1);

         saveFrame(gCtx.hb.pAB[k], gCtx.org.def, gCtx.i);
      } while (gCtx.i < pPI->maxIter);
      releaseCtx(&gCtx);
   }

   if ( nErr != 0 ) { printf( "Test FAILED\n"); }
   else {printf( "Test PASSED\n"); }

   return(0);
} // main
