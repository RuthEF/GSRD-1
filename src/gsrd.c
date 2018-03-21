
#include "proc.h"

typedef struct
{
   //MemBuff  buff; DEPRECATED
   ParamVal    pv;
   HostBuffTab hbt;
   ImgOrg      org;
   U32         i;
} Context;

/***/

static const Scalar gKL[3]= {-1, 0.2, 0.05};

static Context gCtx={0};

/***/

Bool32 initHBT (HostBuffTab * const pT, const size_t fb, const U16 nF)
{
   int i= 0;

   memset(pT, -1, sizeof(*pT));
   if (nF >= 5) { pT->pC= malloc(fb / 2); } else { pT->pC= NULL; }
   do
   {
      pT->hfb[i].pAB= malloc(fb);
   } while (pT->hfb[i].pAB && (++i < HFB_MAX));
   return(i == HFB_MAX);
} // initHBT

void releaseHBT (HostBuffTab * const pT)
{
   for ( int i= 0; i< HFB_MAX; ++i )
   { 
      if (pT->hfb[i].pAB) { free(pT->hfb[i].pAB); pT->hfb[i].pAB= NULL; }
   }
   if (pT->pC) { free(pT->pC); pT->pC= NULL; }
} // releaseHBT

Context *initCtx (Context * const pC, U16 w, U16 h, U16 nF)
{
   if (0 == w) { w= 256; }
   if (0 == h) { h= 256; }
   if (0 == nF) { nF= 4; }
   const size_t n= w * h;
   const size_t b2F= 2 * n * sizeof(Scalar);
   //pC->buff.p= NULL; pC->buff.bytes= 0;
   if (b2F > n)
   {
      initParam(&(pC->pv), gKL, &(pC->org.def), 0.100, 0.005);
      //printf("initCtx() - %zu bytes @ %p\n", b, p);

      initOrg(&(pC->org), w, h, 0);
      
      if (initHBT(&(pC->hbt), b2F, nF))
      {
         pC->i= 0;
         return(pC);
      }
   }
   return(NULL);
} // initCtx

void releaseCtx (Context * const pC)
{
   if (pC)
   {
      releaseParam(&(pC->pv));
      releaseHBT(&(pC->hbt));
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

void frameStat (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO)
{
   const size_t n= pO->def.x * pO->def.y;
   const Scalar * const pA= pAB;
   const Scalar * const pB= pAB + pO->stride[3];
   BlockStat s;
   size_t i, zI[50], nZ= 0;

   initFS(&(s.a), pA);
   initFS(&(s.b), pB);
   
   for (i=1; i<n; i++)
   {
      const Index j= i * pO->stride[0];
      const Scalar a= pA[j];
      const Scalar b= pB[j];
      if ((0 == a) || (0 == b)) { if (nZ < 50) { zI[nZ]= i; } nZ++; }
      if (a < s.a.min) { s.a.min= a; }
      if (a > s.a.max) { s.a.max= a; }
      s.a.s.m[1]+= a;
      s.a.s.m[2]+= a * a;
      if (b < s.b.min) { s.b.min= b; }
      if (b > s.b.max) { s.b.max= b; }
      s.b.s.m[1]+= b;
      s.b.s.m[2]+= b * b;
   }
   if (nZ > 0)
   {
      size_t mZ= MIN(50, nZ);
      printf("%zu zeros: %zu", nZ, zI[0]);
      i= 1;
      while (i < mZ) { printf(", %zu", zI[i++]); }
      printf("\n");
   }
   s.a.s.m[0]= s.b.s.m[0]= n;
   if (pS) { *pS= s; }
} // frameStat

void summarise (HostFB * const pF, const ImgOrg * const pO)
{  //procF
   frameStat(&(pF->s), pF->pAB, pO);
   printf("summarise() - \n\t%zu\tmin\t\tmax\t\tsum\t\tmean\t\tvar\n", pF->iter);
   printFS("\tA:\t", &(pF->s.a), "\n");
   printFS("\tB:\t", &(pF->s.b), "\n");
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
      size_t iM, iR;
      SMVal tE0, tE1;
      HostFB *pFrame;

      do
      {
         tE0= tE1= 0;
         iM= pPI->maxIter;
         if (pPI->subIter > 0) { iM= pPI->subIter; }
         gCtx.i= 0;
         pFrame= gCtx.hbt.hfb+0;
         if (0 == loadBuff(pFrame->pAB, pDFI->path, pDFI->bytes))  //printf("nB=%zu\n",
         {
            initHFB(pFrame, gCtx.org.def, 32);
            saveFrame(pFrame->pAB, gCtx.org.def, gCtx.i);
         }

         do
         {
            int k= gCtx.i & 0x1;
            pFrame= gCtx.hbt.hfb+k;

            iR= pPI->maxIter - gCtx.i;
            if (iM > iR) { iM= iR; }

            deltaT();
            gCtx.i+= proc2I1A(gCtx.hbt.hfb[(k^0x1)].pAB, pFrame->pAB, &(gCtx.org), &(gCtx.pv), iM>>1);
            tE0= deltaT();
            tE1+= tE0;
            
            k= gCtx.i & 0x1;
            pFrame= gCtx.hbt.hfb+k;
            pFrame->iter= gCtx.i;

            summarise(pFrame, &(gCtx.org));
            printf("\ttE= %G, %G\n", tE0, tE1);

            saveFrame(pFrame->pAB, gCtx.org.def, gCtx.i);
         } while (gCtx.i < pPI->maxIter);
         printf("----------\nprocNextAcc() ... \n");
      } while (procNextAcc(FALSE));
      releaseCtx(&gCtx);
   }

   if ( nErr != 0 ) { printf( "Test FAILED\n"); }
   else {printf( "Test PASSED\n"); }

   return(0);
} // main
