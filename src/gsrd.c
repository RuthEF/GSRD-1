
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
   if (pFB && pFB->pAB && pO)
   {
      char path[256];
      int m= sizeof(path)-1, n= 0;

      //if (NULL == pAI->dfi.outPath) { pAI->dfi.outPath= pAI->dfi.inPath; }

      if (pA->dfi.outPath)
      {
         n+= snprintf(path+n, m-n, "%s", pA->dfi.outPath);
         if ('/' != path[n-1]) { path[n++]= '/'; path[n]= 0; }
      }
      if (pA->dfi.outName) { n+= snprintf(path+n, m-n, "%s", pA->dfi.outName); }
      else { n+= snprintf(path+n, m-n, "%s", "gsrd"); } 

      n+= snprintf(path+n, m-n, "%05lu(%lu,%lu,2)F64.raw", pFB->iter, pO->def.x, pO->def.y);
      r= saveBuff(pFB->pAB, path, sizeof(Scalar) * pO->n);
      printf("saveFrame() - %s %p %zu bytes\n", path, pFB->pAB, r);
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
{
   const size_t n= pO->def.x * pO->def.y; // pO->n / 2
   FSFmt fmt;

   frameStat(&(pF->s), pF->pAB, pO);

   printf("summarise() - \n\t%zu  N min max sum mean var\n", pF->iter);
   fmt.pFtr= "\n"; fmt.pSep= " | <=0 ";
   fmt.limPer.min= 5000; fmt.limPer.max= n; fmt.sPer= 100.0 / n;
   fmt.pHdr= "\tA: "; 
   printNFS(pF->s.a, 2, &fmt);
   fmt.pHdr= "\tB: "; 
   printNFS(pF->s.b, 2, &fmt);
} // summarise

typedef struct { size_t s, e; } IdxSpan;
#define MAX_TAB_IS 32
size_t compare (HostFB * const pF1, HostFB * const pF2, const ImgOrg * const pO, const Scalar eps)
{
   const size_t n= pO->def.x * pO->def.y; // pO->n / 2
   const Scalar * const pA1= pF1->pAB;
   const Scalar * const pB1= pF1->pAB + pO->stride[3];
   const Scalar * const pA2= pF2->pAB;
   const Scalar * const pB2= pF2->pAB + pO->stride[3];
   FieldStat sa[3], sb[3];
   FSFmt fmt;
   size_t i;
   IdxSpan tabIS[MAX_TAB_IS];
   U32 nTabIS= 0;

   printf("----\ncompare() - iter: %zu\t\t%zu\n", pF1->iter, pF2->iter);

   initNFS(sa, 3, NULL, 0);
   initNFS(sb, 3, NULL, 0);
   
   for (i=1; i < n; i++)
   {
      const Index j= i * pO->stride[0];

      const Scalar da= pA1[j] - pA2[j];
      const Scalar db= pB1[j] - pB2[j];
      const U8 iA= (da < -eps) | ((da > eps)<<1);
      const U8 iB= (db < -eps) | ((db > eps)<<1);
      statAdd(sa + iA, da);
      statAdd(sb + iB, db);
      if ((iA + iB) > 0)
      {
         U32 k=0;
         while (k < nTabIS)
         {
            if (i == (tabIS[k].s - 1)) { tabIS[k].s--; break; }
            if (i == (tabIS[k].e + 1)) { tabIS[k].e++; break; }
            k++;
         }
         if ((k >= nTabIS) && (nTabIS < MAX_TAB_IS)) { tabIS[nTabIS].s= tabIS[nTabIS].e= i; nTabIS++; }
      }
   }
   printf("\tDiff: N min max sum mean var\n");
   fmt.pFtr= "\n"; fmt.pSep= " | <>e ";
   fmt.limPer.min= 5000; fmt.limPer.max= n; fmt.sPer= 100.0 / n;
   fmt.pHdr= "\tdA: "; 
   printNFS(sa, 3, &fmt);
   fmt.pHdr= "\tdB: ";
   printNFS(sb, 3, &fmt);

   if (nTabIS > 0)
   {
      printf("tabIS[%u]:", nTabIS);
      for (U32 k=0; k<nTabIS; k++)
      {
         if (tabIS[k].s == tabIS[k].e) { printf("%zu, ", tabIS[k].s); }
         else { printf("%zu..%zu, ", tabIS[k].s, tabIS[k].e); }
      }
      printf("\n\n");
   }
   return(sa[1].n + sa[2].n + sb[1].n + sb[2].n);
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
         printf("%s -> v[%d]=(", ai.dfi.initFilePath, ai.dfi.nV);
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
         if (0 == loadBuff(pFrame->pAB, pDFI->initFilePath, pDFI->bytes))  //printf("nB=%zu\n",
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
            gCtx.i+= procNI(pFrame[(k^0x1)].pAB, pFrame[k].pAB, &(gCtx.org), &(gCtx.pv), iM);
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
         { nErr= compare(pFrame, pF2, &(gCtx.org), 1.0/(1<<30)); }
      }
   }
   releaseCtx(&gCtx);
 
   if ( nErr != 0 ) { printf( "Test FAILED\n"); }
   else {printf( "Test PASSED\n"); }

   return(0);
} // main
