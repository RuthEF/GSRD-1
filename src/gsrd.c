
#include "proc.h"

typedef struct
{
   ParamVal    pv;
   HostBuffTab hbt;
   ImgOrg      org;
   U32         i;
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

   if (b2F > n)
   {
      initParam(&(pC->pv), gKL, &(pC->org.def), 0.100, 0.005);

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

size_t saveFrame 
(
   const HostFB * const pFB, 
   const ImgOrg * const pO, 
   const ArgInfo * const pA
)
{
   size_t r= 0;
   char name[64];
   //int t= 3;
   if (pFB && pFB->pAB && pO)
   {
      snprintf(name, sizeof(name)-1, "%s/gsrd%05lu(%lu,%lu,2)F64.raw", pA->dfi.outPath, pFB->iter, pO->def.x, pO->def.y);
      do
      {
         r= saveBuff(pFB->pAB, name, sizeof(Scalar) * pO->n);
      } while (0); // ((r <= 0) && (t-- > 0) && sleep(1));
      //if (r > 0)
      {
         printf("saveFrame() - %s %p %zu bytes\n", name, pFB->pAB, r);
      }
   }
   return(r);
} // saveFrame

void frameStat (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO)
{
   const size_t n= pO->def.x * pO->def.y; // pO->n / 2
   const Scalar * const pA= pAB;
   const Scalar * const pB= pAB + pO->stride[3];
   BlockStat s;
   size_t i;

   initNFS(s.a+0, 2, pA, 1); // KLUDGY
   initNFS(s.b+0, 2, pB, 1);
   
   for (i=1; i < n; i++)
   {
      const Index j= i * pO->stride[0];
      const Scalar a= pA[j];
      const Scalar b= pB[j];

      statAdd(s.a + (a <= 0), a);
      statAdd(s.b + (b <= 0), b);
   }
   if (pS) { *pS= s; }
} // frameStat

void summarise (HostFB * const pF, const ImgOrg * const pO)
{  //procF
   frameStat(&(pF->s), pF->pAB, pO);
   printf("summarise() - \n\t%zu\tN\tmin\tmax\tsum\tmean\tvar\n", pF->iter);
   printNFS("\tA: ", pF->s.a, 2, " | <=0 ", "\n");
   printNFS("\tB: ", pF->s.b, 2, " | <=0 ", "\n");
} // summarise

void compare (HostFB * const pF1, HostFB * const pF2, const ImgOrg * const pO)
{
   printf("compare() - iter: %zu\t\t%zu\n", pF1->iter, pF2->iter);
} // compare

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
   if (procInitAcc(pPI->flags) && initCtx(&gCtx, pDFI->v[0], pDFI->v[1], 8))
   {
      size_t iM, iR;
      SMVal tE0, tE1;
      HostFB *pFrame;
      char t[8];
      U8 afb=0, nIdx=0, fIdx[4], k;

      do
      {
         tE0= tE1= 0;
         iM= pPI->maxIter;
         if (pPI->subIter > 0) { iM= pPI->subIter; }
         gCtx.i= 0;
         afb&= 0x3;
         pFrame= gCtx.hbt.hfb + afb;
         if (0 == loadBuff(pFrame->pAB, pDFI->inPath, pDFI->bytes))  //printf("nB=%zu\n",
         {
            initHFB(pFrame, gCtx.org.def, 32);
            saveFrame(pFrame, &(gCtx.org), &ai);
         }
         pFrame->iter= gCtx.i;
         summarise(pFrame, &(gCtx.org));
         printf("---- %s ----\n", procGetCurrAccTxt(t, sizeof(t)-1));
         do
         {
            k= gCtx.i & 0x1;
            iR= pPI->maxIter - gCtx.i;
            if (iM > iR) { iM= iR; }

            deltaT();
            gCtx.i+= proc2I1A(pFrame[(k^0x1)].pAB, pFrame[k].pAB, &(gCtx.org), &(gCtx.pv), iM>>1);
            tE0= deltaT();
            tE1+= tE0;
            
            k= gCtx.i & 0x1;
            pFrame[k].iter= gCtx.i;

            summarise(pFrame+k, &(gCtx.org));
            printf("\ttE= %G, %G\n", tE0, tE1);

            saveFrame(pFrame+k, &(gCtx.org), &ai);
         } while (gCtx.i < pPI->maxIter);
         if (nIdx < 4) { fIdx[nIdx++]= afb+k; }
         afb+= 2;
      } while (procSetNextAcc(FALSE));
      if (nIdx > 1)
      {
         HostFB *pF2= gCtx.hbt.hfb+fIdx[1];
         pFrame=  gCtx.hbt.hfb+fIdx[0];
         //if (pFrame->iter == pF1->iter) 
         { compare(pFrame, pF2, &(gCtx.org)); }
      }
   }
   releaseCtx(&gCtx);
 
   if ( nErr != 0 ) { printf( "Test FAILED\n"); }
   else {printf( "Test PASSED\n"); }

   return(0);
} // main
