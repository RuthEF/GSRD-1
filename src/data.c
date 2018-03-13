

#include "data.h"


/***/

void initOrg (ImgOrg * const pO, U16 w, U16 h, U8 flags)
{
   if (pO)
   {
      pO->def.x= w;
      pO->def.y= h;
      if (0 == (flags & 1))
      {  // planar
         pO->stride[0]= 1;     // next element, same phase
         pO->stride[3]= w * h; // same element, next phase
      }
      else
      {  // interleaved
         pO->stride[0]= 2; // same phase
         pO->stride[3]= 1; // next phase
      }
      pO->stride[1]= w * pO->stride[0]; // line
      pO->stride[2]= h * pO->stride[1]; // plane
      pO->n= 2 * pO->stride[2]; // complete buffer
   }
} // initOrg

//size_t paramBytes (U16 w, U16 h) { return(MAX(w,h) * 3 * sizeof(Scalar)); }

size_t initParam (ParamVal * const pP, const Scalar kL[3], const V2U32 *pD, Scalar varR, Scalar varD) // ParamArgs *
{
   U32 i, n= 0;
   Scalar *pA, *pB, *pC;
   // Set diffusion weights
   for ( i= 0; i<3; ++i ) { pP->kL.a[i]= kL[i] * KLA0; pP->kL.b[i]= kL[i] * KLB0; }
   pP->kRR= KR0;
   pP->kRA= KRA0;
   pP->kDB= KDB0;
   pA= pB= pC= NULL;
   if (pD)
   {
      n= MAX(pD->x, pD->y);
      pA= malloc( n * sizeof(*(pP->pKRR)) );
      if (pA) { pB= malloc( n * sizeof(*(pP->pKRA)) ); }
      if (pB) { pC= malloc( n * sizeof(*(pP->pKDB)) ); }
   }
   pP->n= n;
   if (pC)
   {
      Scalar kRR=KR0, kRA=KRA0, kDB=KDB0;
      Scalar dKRR=0, dKRA=0, dKDB=0;
      
      dKRR= (kRR * varR) / n;
      dKDB= (kDB * varD) / n;
      for (i= 0; i<n; ++i)
      {
         pA[i]= kRR; kRR+= dKRR;
         pB[i]= kRA; kRA+= dKRA;
         pC[i]= kDB; kDB+= dKDB;
      }
   } else
   {
      if (pA) { free(pA); }
      if (pB) { free(pB); }
      pA= pB= NULL;
   }
   pP->pKRR= pA;
   pP->pKRA= pB;
   pP->pKDB= pC;
   return(n);
} // initParam

void releaseParam (ParamVal * const pP)
{
   free((void*)(pP->pKRR));
   free((void*)(pP->pKRA));
   free((void*)(pP->pKDB));
} // releaseParam

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


void initFS (FieldStat * const pFS, const Scalar * const pS)
{
   if (pFS)
   {
      Scalar s= 0; // or NaN ?
      if (pS) { s= *pS; }
      pFS->min= pFS->max= s;

      pFS->s.m[0]= (NULL != pS);
      pFS->s.m[1]= s;
      pFS->s.m[1]= s * s;
      //pFS->sum1= s;
      //pFS->sum2= s * s;
   }
} // initFS

void printFS (const char *pHdr, const FieldStat * const pFS, const char *pFtr)
{
   if (pHdr && pHdr[0]) { printf("%s", pHdr); }
   if (pFS)
   {  const char *fsFmtStr= "%G, %G, %G, %G";
      if (sizeof(SSStat) > sizeof(double)) { fsFmtStr= "%G, %G, %LG, %LG"; }
      printf(fsFmtStr, pFS->min, pFS->max, pFS->s.m[1], pFS->s.m[2]);
   }
   if (pFtr && pFtr[0]) { printf("%s", pFtr); }
} // printFS


