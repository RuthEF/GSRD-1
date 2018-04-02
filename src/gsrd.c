
#include "proc.h"

typedef struct
{
   ParamVal    pv;
   HostBuffTab hbt;
   ImgOrg      org;
   size_t      iter;
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
         pC->iter= 0;
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

size_t loadFrame 
(
   HostFB               * const pFB, 
   const DataFileInfo  * const pDFI
)
{
   size_t r= 0;
   if (pFB && pFB->pAB && pDFI && (pDFI->flags & DFI_FLAG_VALIDATED))
   {
      r= loadBuff(pFB->pAB, pDFI->filePath, pDFI->bytes);
      if (r > 0) { pFB->iter= pDFI->iter; }
   }
   return(r);
} // loadFrame

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

      if (pA->files.outPath)
      {
         n+= snprintf(path+n, m-n, "%s", pA->files.outPath);
         if ('/' != path[n-1]) { path[n++]= '/'; path[n]= 0; }
      }
      if (pA->files.outName) { n+= snprintf(path+n, m-n, "%s", pA->files.outName); }
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
   IdxSpan tabDiff[MAX_TAB_IS];
   U32 nTabDiff= 0;

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
         while (k < nTabDiff)
         {
            if (i == (tabDiff[k].s - 1)) { tabDiff[k].s--; break; }
            if (i == (tabDiff[k].e + 1)) { tabDiff[k].e++; break; }
            k++;
         }
         if ((k >= nTabDiff) && (nTabDiff < MAX_TAB_IS)) { tabDiff[nTabDiff].s= tabDiff[nTabDiff].e= i; nTabDiff++; }
      }
   }
   printf("\tDiff: N min max sum mean var\n");
   fmt.pFtr= "\n"; fmt.pSep= " | <>e ";
   fmt.limPer.min= 5000; fmt.limPer.max= n; fmt.sPer= 100.0 / n;
   fmt.pHdr= "\tdA: "; 
   printNFS(sa, 3, &fmt);
   fmt.pHdr= "\tdB: ";
   printNFS(sb, 3, &fmt);

   if (nTabDiff > 0)
   {
      printf("--------\n\ntabDiff[%u]:", nTabDiff);
      for (U32 k=0; k<nTabDiff; k++)
      {
         if (tabDiff[k].s == tabDiff[k].e) { printf("%zu, ", tabDiff[k].s); }
         else { printf("%zu..%zu, ", tabDiff[k].s, tabDiff[k].e); }
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
      printf("proc: f=0x%zX, m=%zu, s=%zu\n", ai.proc.flags, ai.proc.maxIter, ai.proc.subIter);
   }
   const DataFileInfo * const pIF= &(ai.files.init);
   const ProcInfo * const pPI= &(ai.proc);
   if (procInitAcc(pPI->flags) && initCtx(&gCtx, pIF->v[0], pIF->v[1], 8))
   {
      SMVal tE0, tE1;
      HostFB *pFrame, *pF2=NULL;
      char t[8];
      U8 afb=0, nIdx=0, fIdx[4], k=0;

      do
      {
         size_t iM= pPI->maxIter;
         tE0= tE1= 0;
         if (pPI->subIter > 0) { iM= pPI->subIter; }
         afb&= 0x3;
         pFrame= gCtx.hbt.hfb + afb;
         if (0 == loadFrame(pFrame, pIF))  //printf("nB=%zu\n",
         {
            initHFB(pFrame, gCtx.org.def, 32);
            saveFrame(pFrame, &(gCtx.org), &ai);
         }
         gCtx.iter= pFrame->iter;
         //k= (gCtx.iter - pFrame->iter) & 0x1;
         summarise(pFrame, &(gCtx.org));
         printf("---- %s ----\n", procGetCurrAccTxt(t, sizeof(t)-1));
         do
         {
            size_t iR= pPI->maxIter - gCtx.iter;
            if (iM > iR) { iM= iR; }

            deltaT();
            gCtx.iter+= procNI(pFrame[(k^0x1)].pAB, pFrame[k].pAB, &(gCtx.org), &(gCtx.pv), iM);
            tE0= deltaT();
            tE1+= tE0;
            
            k= (gCtx.iter - pFrame->iter) & 0x1;
            pFrame[k].iter= gCtx.iter;

            summarise(pFrame+k, &(gCtx.org));
            printf("\ttE= %G, %G\n", tE0, tE1);

            saveFrame(pFrame+k, &(gCtx.org), &ai);
         } while (gCtx.iter < pPI->maxIter);
         if (nIdx < 4) { fIdx[nIdx++]= afb+k; }
         afb+= 2;
      } while (procSetNextAcc(PROC_NOWRAP));
      if (nIdx > 1)
      {
         pF2= gCtx.hbt.hfb+fIdx[1];
         pFrame=  gCtx.hbt.hfb+fIdx[0];
         //if (pFrame->iter == pF1->iter) 
         { nErr= compare(pFrame, pF2, &(gCtx.org), 1.0/(1<<30)); }
      }
      if ((nIdx > 0) && (nIdx < 4))
      {
         fIdx[nIdx]= ( fIdx[nIdx-1] + 1 ) & 0x3;
         if (loadFrame( gCtx.hbt.hfb+fIdx[nIdx], &(ai.files.cmp) ))
         {
            pF2= gCtx.hbt.hfb + fIdx[nIdx];
            nErr= compare(pFrame, pF2, &(gCtx.org), 1.0/(1<<10));
         }
      }
   }
   releaseCtx(&gCtx);
 
   if ( nErr != 0 ) { printf( "Test FAILED\n"); }
   else {printf( "Test PASSED\n"); }

   return(0);
} // main
